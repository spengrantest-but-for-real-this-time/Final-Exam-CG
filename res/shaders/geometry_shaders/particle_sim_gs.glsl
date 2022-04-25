#version 440

#define MAX_VERTS_OUT 33

layout (points) in;
layout (points) out;
layout (max_vertices = MAX_VERTS_OUT) out;

// Define inputs to match the vertex shader
layout (location = 0) in flat uint inType[];
layout (location = 1) in flat uint inTexID[];
layout (location = 2) in vec3 inPosition[];
layout (location = 3) in vec3 inVelocity[];
layout (location = 4) in vec4 inColor[];
layout (location = 5) in float inLifetime[];
layout (location = 6) in vec4 inMetadata[];
layout (location = 7) in vec4 inMetadata2[];

// Our per-vertex outputs
out flat uint out_Type;
out flat uint out_TexID;
out vec3 out_Position;
out vec4 out_Color;
out float out_Lifetime;
out vec3 out_Velocity;
out vec4 out_Metadata;
out vec4 out_Metadata2;

#include "../fragments/frame_uniforms.glsl"
#include "../fragments/random.glsl"

// Uniforms
uniform vec3  u_Gravity;

uniform mat4 u_ModelMatrix;

#define TYPE_EMITTER_STREAM 0
#define TYPE_EMITTER_SPHERE 1
#define TYPE_EMITTER_BOX 2
#define TYPE_EMITTER_CONE 3
#define TYPE_PARTICLE (1 << 17)

#define EMITTER_MASK 0x0000FFFF
#define PARTICLE_MASK 0x0000FFFF

// See https://thebookofshaders.com/10/
// Returns a random number between 0 and 1
float rand(vec2 seed) {
    return fract(sin(dot(seed, vec2(12.9898, 78.233) * 43758.5453123)));
}

vec3 point_on_sphere() {
    float z = random(u_Time + 1) * 2 - 1;
    float rxy = sqrt(1 - z * z);
    float phi = random(u_Time + 2) * 6.28318530718;
    return vec3(rxy * cos(phi), rxy * sin(phi), z);
}

void prep_emitter(out float startLife, out int toEmit) {
    float lifetime = inLifetime[0] - u_DeltaTime;
    int emitted = 1;
    vec4 meta = inMetadata[0];
    startLife = lifetime;
    toEmit = 0;
    
    while ((lifetime < 0) && (emitted < MAX_VERTS_OUT)) {
        lifetime += meta.x;
        toEmit ++;
        emitted++;
    }

    // Push the emitter back into the output stream
    out_Type     = inType[0];
    out_TexID    = inTexID[0];
    out_Position = inPosition[0];
    out_Velocity = inVelocity[0];
    out_Color    = inColor[0];
    out_Lifetime = lifetime;
    out_Metadata = inMetadata[0];
    out_Metadata2 = inMetadata2[0];
    
    EmitVertex();
    EndPrimitive();
}

void emit_stream() {
    float startLife;
    int toEmit;
    vec4 meta = inMetadata[0];
    vec4 meta2 = inMetadata2[0];
    
    vec3 velocity = inVelocity[0];
    vec2 lifeRange = meta.zw;
    vec2 sizeRange = meta2.xy;

    prep_emitter(startLife, toEmit);

    // If the lifetime is at 0, we emit a particle
    for (int ix = 0; ix < toEmit; ix++) {
        float timeAdjust = (-startLife + (ix * meta.x));
        out_Type = TYPE_PARTICLE;
        out_TexID = inTexID[0];
        out_Position = (u_ModelMatrix * vec4(inPosition[0] + velocity * timeAdjust, 1.0f)).xyz;
        out_Velocity = mat3(u_ModelMatrix) * velocity;

        float lifeScale = rand(mod(vec2(u_DeltaTime, u_Time), vec2(1,1)));
        out_Lifetime = lifeRange.x + (lifeRange.y - lifeRange.x) * lifeScale;

        float sizeScale = rand(mod(vec2(u_Time, u_DeltaTime), vec2(1,1)));
        out_Metadata = vec4(out_Lifetime, sizeRange.x + (sizeRange.y - meta.x) * sizeScale, 0, 0);
        
        out_Metadata2 = vec4(0);
        out_Color    = inColor[0];
        
        EmitVertex();
        EndPrimitive();
    }
}

void emit_box() {
    float startLife;
    int toEmit;
    vec4 meta = inMetadata[0];
    vec4 meta2 = inMetadata2[0];
    
    vec2 sizeRange = meta.yz;
    vec2 lifeRange = vec2(meta.w, meta2.x);
    vec3 halfExtents = meta2.yzw;

    prep_emitter(startLife, toEmit);

    // If the lifetime is at 0, we emit a particle
    for (int ix = 0; ix < toEmit; ix++) {
        float timeAdjust = (-startLife + (ix * meta.x));
        out_Type = TYPE_PARTICLE;
        out_TexID = inTexID[0];

        vec3 relative = vec3(
            (random(u_Time + 3) * 2 - 1) * halfExtents.x,
            (random(u_Time + 4) * 2 - 1) * halfExtents.y,
            (random(u_Time + 5) * 2 - 1) * halfExtents.z
        );
        vec3 velocity = normalize(relative) * inVelocity[0];

        out_Position = (u_ModelMatrix * vec4(inPosition[0] + relative + velocity * timeAdjust, 1.0f)).xyz;
        out_Velocity = mat3(u_ModelMatrix) * velocity;

        float lifeScale = rand(mod(vec2(u_DeltaTime, u_Time), vec2(1,1)));
        out_Lifetime = lifeRange.x + (lifeRange.y - lifeRange.x) * lifeScale;

        float sizeScale = rand(mod(vec2(u_Time, u_DeltaTime), vec2(1,1)));
        out_Metadata = vec4(out_Lifetime, sizeRange.x + (sizeRange.y - meta.x) * sizeScale, 0, 0);
        
        out_Metadata2 = vec4(0);
        out_Color    = inColor[0];
        
        EmitVertex();
        EndPrimitive();
    }
}

void emit_sphere() {
    float startLife;
    int toEmit;
    vec4 meta = inMetadata[0];
    vec4 meta2 = inMetadata2[0];

    float radius = inVelocity[0].y;
    float velocity = inVelocity[0].x;
    vec2 lifeRange = meta.zw;
    vec2 sizeRange = meta2.xy;

    prep_emitter(startLife, toEmit);

    // If the lifetime is at 0, we emit a particle
    for (int ix = 0; ix < toEmit; ix++) {
        float timeAdjust = (-startLife + (ix * meta.x));
        out_Type = TYPE_PARTICLE;
        out_TexID = inTexID[0];

        vec3 targetVelocity = point_on_sphere();
        vec3 targetPos = targetVelocity * random(u_Time + 3) * radius;

        out_Position = (u_ModelMatrix * vec4(inPosition[0] + targetPos + velocity * timeAdjust, 1.0f)).xyz;
        out_Velocity = mat3(u_ModelMatrix) * targetVelocity * velocity;

        float lifeScale = rand(mod(vec2(u_DeltaTime, u_Time), vec2(1,1)));
        out_Lifetime = lifeRange.x + (lifeRange.y - lifeRange.x) * lifeScale;

        float sizeScale = rand(mod(vec2(u_Time, u_DeltaTime), vec2(1,1)));
        out_Metadata = vec4(out_Lifetime, sizeRange.x + (sizeRange.y - sizeRange.x) * sizeScale, 0, 0);

        out_Metadata2 = vec4(0);
        out_Color    = inColor[0];
        
        EmitVertex();
        EndPrimitive();
    }
}

void emit_cone() {
    float startLife;
    int toEmit;
    vec4 meta = inMetadata[0];
    vec4 meta2 = inMetadata2[0];

    vec3 vOrigin = normalize(inVelocity[0]);
    float angle  = meta.y;
    vec2 lifeRange = meta.zw;
    vec2 sizeRange = meta2.xy;
    
    vec3 crossX = vec3(-vOrigin.z, vOrigin.x, vOrigin.y);
    if (dot(crossX, vOrigin) > 0.001) {
        crossX = vec3(-vOrigin.y, vOrigin.x, vOrigin.z);
    }
    vec3 crossY = cross(vOrigin, crossX);

    prep_emitter(startLife, toEmit);

    // If the lifetime is at 0, we emit a particle
    for (int ix = 0; ix < toEmit; ix++) {
        float timeAdjust = (-startLife + (ix * meta.x));
        out_Type = TYPE_PARTICLE;
        out_TexID = inTexID[0];

        float theta = acos(random_range(cos(angle), 1, u_Time + 2));
        float phi   = random_range(0.0, 6.28318530718, u_Time + 3);

        vec3 targetVelocity = sin(theta) * (cos(phi) * crossX + sin(phi) * crossY) + cos(theta) * vOrigin;
        targetVelocity *= length(inVelocity[0]);

        out_Position = (u_ModelMatrix * vec4(inPosition[0] + targetVelocity * timeAdjust, 1.0f)).xyz;
        out_Velocity = mat3(u_ModelMatrix) * targetVelocity;

        float lifeScale = rand(mod(vec2(u_DeltaTime, u_Time), vec2(1,1)));
        out_Lifetime = lifeRange.x + (lifeRange.y - lifeRange.x) * lifeScale;

        float sizeScale = rand(mod(vec2(u_Time, u_DeltaTime), vec2(1,1)));
        out_Metadata = vec4(out_Lifetime, sizeRange.x + (sizeRange.y - sizeRange.x) * sizeScale, 0, 0);

        out_Metadata2 = vec4(0);
        out_Color    = inColor[0];
        
        EmitVertex();
        EndPrimitive();
    }
}

void main() {
    float lifetime = inLifetime[0] - u_DeltaTime;
    vec4 meta = inMetadata[0];


    switch (inType[0]) {
        // Handling emitters
        case TYPE_EMITTER_STREAM:
            emit_stream();
            return;

        case TYPE_EMITTER_SPHERE:
            emit_sphere();
            return;

        case TYPE_EMITTER_BOX:
            emit_box();
            return;

        case TYPE_EMITTER_CONE:
            emit_cone();
            return;

        // Handling particles
        case TYPE_PARTICLE:
            if (lifetime > 0) {
                out_Type = TYPE_PARTICLE;
                out_TexID = inTexID[0];

                // Update position and apply forces
                out_Position = inPosition[0] + inVelocity[0] * u_DeltaTime;
                out_Velocity = inVelocity[0] + (u_Gravity * u_DeltaTime);
                                
                // Update lifetime
                out_Lifetime = lifetime;

                // For now, just pass through metadata and color
                out_Metadata = inMetadata[0];
                out_Color    = vec4(inColor[0].rgb, (lifetime / meta.x));
                out_Metadata = inMetadata[0];
                out_Metadata2 = inMetadata2[0];

                // Emit into vertex stream
                EmitVertex();
                EndPrimitive();
            }
            return;
        // Anything else, for debug purposes
        default:
            return;
    }
}