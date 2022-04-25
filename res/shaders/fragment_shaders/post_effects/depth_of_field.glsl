#version 440

// Modified from:
// http://tuxedolabs.blogspot.com/2018/05/bokeh-depth-of-field-in-single-pass.html

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

// Our color buffer to sample from
layout(binding = 0) uniform sampler2D a_Sampler;
// The depth buffer to use (non-linearized)
layout(binding = 1) uniform sampler2D a_Depth;

#include "../../fragments/frame_uniforms.glsl"

const float GOLDEN_ANGLE = 2.39996323;
const float MAX_BLUR_RADIUS = 20; // We impose a hard limit on blurring to avoid killing the GPU
const float RAD_SCALE = 0.5;

// Converts a screen space coord and a raw depth value into a world-space distance
// @param screen The screen-space coordinate to convert
// @param rawValue The raw, non-linear depth value to convert
// @returns A distance to the camera in world units
float DepthToDist(vec2 screen, float rawValue) {
	vec4 screenPos = vec4(screen.x, screen.y, rawValue, 1.0) * 2.0 - 1.0;
	vec4 viewPosition = u_InvProjection * screenPos;

	return -(viewPosition.z / viewPosition.w);
}

/*
* Calculates the Circle of Confusion for a given depth value
* @param depth The depth of the fragment to caluculate for (in world units)
* @param focalPlane The distance from the lense to the focal plane (in world units)
* @param focalLength The focal length parameter (calculated as 1/F = 1/focalPlane + 1/distToSensor)
* @see http://fileadmin.cs.lth.se/cs/Education/EDAN35/lectures/12DOF.pdf
*/
float getBlurSize(float depth, float focalPlane, float focalLength) {
	float coc = clamp(
        (focalLength * (focalPlane - depth)) / 
        (depth * (focalPlane - focalLength)), 
        -1.0, 1.0);
	return abs(coc) * u_Aperture;
}

/*
* Calculates our color for the depth of field effect
* @param texCoord The UV coordinate to solve for
* @param focusPoint The distance from the lense to the focal plane (in world units)
* @param focalLength The focal length parameter (calculated as 1/F = 1/focalPlane + 1/distToSensor)
*/
vec3 depthOfField(vec2 texCoord, float focusPoint, float focusLength) {
    // Determines the size of single texel
    vec2 texelSize = 1.0 / textureSize(a_Depth, 0);

    // Get our depth into view space, and use that to calculate our circle of confusion
    float centerDepth = DepthToDist(texCoord, texelFetch(a_Depth, ivec2(texCoord / texelSize), 0).r);
    float centerCOC = getBlurSize(centerDepth, focusPoint, focusLength);

    // Initialize out color and total number of samples
    vec3 color = texture(a_Sampler, texCoord).rgb;
    float tot = 1.0;

    // We'll blur our fragment outward in a circle
    float radius = RAD_SCALE;
    for (float ang = 0.0; radius < min(u_Aperture, MAX_BLUR_RADIUS); ang += GOLDEN_ANGLE)
    {
        // Determine the UV coord of the fragment we want to blur
        vec2 tc = texCoord + vec2(cos(ang), sin(ang)) * texelSize * radius;

        // Collect the color, depth, circle of confusion for that sample
        vec3 sampleColor = texture(a_Sampler, tc).rgb;
        float sampleDepth = DepthToDist(tc, texelFetch(a_Depth, ivec2(tc / texelSize), 0).r);
        float sampleCOC = getBlurSize(sampleDepth, focusPoint, focusLength);

        if (sampleDepth > centerDepth)
			sampleCOC = clamp(sampleCOC, 0.0, centerCOC);

        float m = smoothstep(radius - RAD_SCALE, radius + RAD_SCALE, sampleCOC);
        color += mix(color / tot, sampleColor, m);

        // Track that we have another sample, and advance our radius outward
        tot += 1.0;
        radius += RAD_SCALE / radius;
    }
    // We'll return the average of all our colors
    return color /= tot;
}

void main() {
    // Calculate our focal length
    float focalLength = 1.0f / (1.0 / u_FocalDepth + 1.0 / u_LensDepth);
    // Perform our DOF blurring
    vec3 dof = depthOfField(inUV, u_FocalDepth, focalLength);
    // Return the result
    outColor = vec4(dof, 1.0);
}