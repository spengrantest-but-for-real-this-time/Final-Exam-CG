#version 430

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

uniform layout(binding = 0) sampler2D s_Albedo;
uniform layout(binding = 1) sampler2D s_NormalsMetallic;
uniform layout(binding = 2) sampler2D s_DiffuseAccumulation;
uniform layout(binding = 3) sampler2D s_SpecularAccumulation;
uniform layout(binding = 4) sampler2D s_Emissive;

#include "../fragments/frame_uniforms.glsl"
#include "../fragments/color_correction.glsl"
#include "../fragments/multiple_point_lights.glsl"

void main() {
    vec3 albedo = vec3(0);
    vec3 diffuse = vec3(0);
    vec3 specular = vec3(0);
    vec4 emissive = vec4(0);

    outColor = vec4(0);


    if (IsFlagSet(FLAG_ENABLE_ALBEDO)) {

    albedo = texture(s_Albedo, inUV).rgb;


    }

    if (IsFlagSet(FLAG_ENABLE_DIFFUSE)) {

    diffuse = texture(s_DiffuseAccumulation, inUV).rgb;


    }



        if (IsFlagSet(FLAG_ENABLE_SPECULAR)) {

    specular = texture(s_SpecularAccumulation, inUV).rgb;


    }


    if (IsFlagSet(FLAG_ENABLE_EMISSIVE)) {

    vec4 emissive = texture(s_Emissive, inUV);


    }




	outColor = vec4(albedo * (diffuse + specular + (emissive.rgb * emissive.a)), 1.0);
}