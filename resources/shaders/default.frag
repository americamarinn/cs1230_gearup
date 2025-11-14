#version 330 core

in vec3 vNor;
out vec4 fragColor;

void main() {
    // debug: color = normal
    fragColor = vec4(abs(normalize(vNor)), 1.0);
}
