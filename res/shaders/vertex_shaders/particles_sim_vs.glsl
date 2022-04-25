#version 440

layout (location = 0) in uint  inType;
layout (location = 1) in uint  inTexID;
layout (location = 2) in vec3  inPosition;
layout (location = 3) in vec3  inVelocity;
layout (location = 4) in vec4  inColor;
layout (location = 5) in float inLifetime;
layout (location = 6) in vec4  inMetadata;
layout (location = 7) in vec4  inMetadata2;

layout (location = 0) out flat uint  outType; 
layout (location = 1) out flat uint  outTexID;
layout (location = 2) out vec3  outPosition;
layout (location = 3) out vec3  outVelocity;
layout (location = 4) out vec4  outColor;
layout (location = 5) out float outLifetime;
layout (location = 6) out vec4  outMetadata;
layout (location = 7) out vec4  outMetadata2;

// Simple passthrough to the geometry shader
void main() {
    outType      = inType;
    outTexID     = inTexID;
    outPosition  = inPosition;
    outVelocity  = inVelocity;
    outColor     = inColor;
    outLifetime  = inLifetime;
    outMetadata  = inMetadata;
    outMetadata2 = inMetadata2;
}

