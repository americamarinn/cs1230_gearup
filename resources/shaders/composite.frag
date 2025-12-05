#version 330 core
out vec4 FragColor;
in vec2 uv;

uniform sampler2D scene;
uniform sampler2D bloomBlur;
uniform float exposure;

void main() {
    vec3 hdrColor = texture(scene, uv).rgb;
    vec3 bloomColor = texture(bloomBlur, uv).rgb;

    // 1. Additive Blending
    vec3 result = hdrColor + bloomColor;

    // 2. Tone Mapping (Reinhard) - squashes bright values to [0,1]
    result = vec3(1.0) - exp(-result * exposure);

    // 3. Gamma Correction (Optional, but looks better)
    result = pow(result, vec3(1.0 / 2.2));

    FragColor = vec4(result, 1.0);
}



// #version 330 core
// out vec4 FragColor;
// in vec2 uv;

// uniform sampler2D scene;
// uniform sampler2D bloomBlur;

// void main() {
//     vec3 hdrColor = texture(scene, uv).rgb;
//     vec3 bloomColor = texture(bloomBlur, uv).rgb;
    
//     // Additive blending
//     vec3 result = hdrColor + bloomColor;
    
//     // Tone mapping (optional, prevents colors washing out to white)
//     // result = vec3(1.0) - exp(-result * 1.0);

//     FragColor = vec4(result, 1.0);
// }
