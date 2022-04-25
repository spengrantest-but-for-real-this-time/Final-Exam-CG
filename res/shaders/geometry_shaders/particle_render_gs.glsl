#version 450

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

// Define inputs to match the vertex shader
layout (location = 0) in vec4 inFragColor[];
layout (location = 1) in flat uint inType[];
layout (location = 2) in flat uint inTexID[];
layout (location = 3) in vec3 inPosition[];
layout (location = 4) in vec4  inMetaData[];

layout (location = 0) out vec4 outFragColor;
layout (location = 1) out vec2 outUV;
layout (location = 2) out flat uint outTexID;

#include "../fragments/frame_uniforms.glsl"

#define TYPE_EMITTER 0
#define TYPE_PARTICLE 1

const uint EMITTER_MASK = 0x0000FFFF;

void main() {
    
    // If the particle type is an emitter, we can skip it
	if ((inType[0] & EMITTER_MASK) == inType[0]) {
		return;
	}

    // Get particle size from the attributes 
    float size = inMetaData[0].y;
    
    // Extract the right vector from view matrix
    vec3 right = vec3(
        u_View[0][0],
        u_View[1][0],
        u_View[2][0]
    );
    // Extract the up vector from view matrix
    vec3 up = vec3(
        u_View[0][1],
        u_View[1][1],
        u_View[2][1]
    );

    outFragColor = inFragColor[0];
    outTexID = inTexID[0];

    vec3 tl = inPosition[0] - ( right + up ) * size / 2;
    vec3 tr = inPosition[0] - ( right - up ) * size / 2;
    vec3 bl = inPosition[0] + ( right - up ) * size / 2;
    vec3 br = inPosition[0] + ( right + up ) * size / 2;

    outUV = vec2(0, 1);
    gl_Position = u_ViewProjection * vec4(tr, 1);    
    EmitVertex();

    outUV = vec2(0, 0);
    gl_Position = u_ViewProjection * vec4(tl, 1);    
    EmitVertex();
    
    outUV = vec2(1, 1);
    gl_Position = u_ViewProjection * vec4(br, 1);   
    EmitVertex();
    
    outUV = vec2(1, 0);
    gl_Position = u_ViewProjection * vec4(bl, 1);  
    EmitVertex();

    EndPrimitive();

}
