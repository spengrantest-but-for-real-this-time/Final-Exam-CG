#version 450

layout (location = 0) in uint  inType;
layout (location = 1) in uint  inTexId;
layout (location = 2) in vec3  inPosition;
layout (location = 4) in vec4  inColor;
layout (location = 6) in vec4  inMetaData;
layout (location = 7) in vec4  inMetaData2;

layout (location = 0) out vec4 fragColor;
layout (location = 1) out flat uint outType;
layout (location = 2) out flat uint outTexID;
layout (location = 3) out vec3 outPosition;
layout (location = 4) out vec4  outMetaData;
layout (location = 5) out vec4  outMetaData2;

#include "../fragments/frame_uniforms.glsl"

void main() {
    outPosition = inPosition;
    fragColor = inColor;
    outType = inType;
    outTexID = inTexId;
    outMetaData = inMetaData;
    outMetaData2 = inMetaData2;
}