#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

out vec3 vNor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

void main() {
    // transform normal to world space (handles non-uniform scale later)
    mat3 normalMat = transpose(inverse(mat3(model)));
    vNor = normalize(normalMat * normal);

    gl_Position = proj * view * model * vec4(position, 1.0);
}
