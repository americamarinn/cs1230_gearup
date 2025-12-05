// Replace the ENTIRE content of gbuffer.frag with this
#version 330 core

// Change vec3 to vec4 for gPosition, gNormal, and gEmissive to match GL_RGBA16F
layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gAlbedo;
layout(location = 3) out vec4 gEmissive;

in vec3 worldPos;
in vec3 worldNormal;

uniform vec3 albedo;
uniform vec3 emissive;

void main() {
    // Must write a vec4 value to vec4 outputs
    gPosition = vec4(worldPos, 1.0);
    gNormal = vec4(normalize(worldNormal), 1.0);
    gAlbedo = vec4(albedo, 1.0);
    gEmissive = vec4(emissive, 1.0);
}
