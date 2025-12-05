#version 330 core


// Declare input vertex attributes (specified by VBO + VAO)
// The “layout” qualifier indicates the index of the VAO attribute we want
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

//I sent the things separately so we do the multiplicaton here
// this is what transofmr to world space
uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

// Match the fragment shader inputs:
out vec3 wsPosition;
out vec3 wsNormal; // this is MVP (example of a slighlty less simple vertex shader in lecture!)

//here we do the multipliation instead of in the draw method
void main() {
    // World-space position
    //multiplying object-space position by model to get world-space position
    vec4 worldPosition = model * vec4(position, 1.0);
    wsPosition = worldPosition.xyz;

    // transforming to World-space normal
    wsNormal = mat3(model) * normal;

    // World to Clip-space position
    // gl_Position is a special vec4 output that every vertex shader must provide
    gl_Position = proj * view * worldPosition;

}
