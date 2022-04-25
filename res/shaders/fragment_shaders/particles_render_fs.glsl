#version 450

layout (location = 0) in vec4 fragColor;
layout (location = 1) in vec2 inUv;
layout (location = 2) in flat uint inTexId;

layout(location = 0) out vec4 outColor;

uniform layout(location = 0) sampler2DArray s_Textures;

void main() { 
	outColor = fragColor * texture(s_Textures, vec3(inUv, inTexId));
	if (outColor.a < 0.5) {
		discard;
	}
}

