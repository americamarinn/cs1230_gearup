#version 330 core

// Inputs from vertex shader (world-space)
in vec3 wsPosition;
in vec3 wsNormal;

// Output
out vec4 fragColor;

// -------- Global coefficients (match C++ names) --------
uniform float k_a;
uniform float k_d;
uniform float k_s;

// -------- Material properties (match C++ names) --------
uniform vec3  cAmbient;
uniform vec3  cDiffuse;
uniform vec3  cSpecular;
uniform float shininess;

// camera world space
uniform vec3 camPos;

// light description
struct Light {
    int   type;      // 0 = point, 1 = directional, 2 = spot
    vec3  color;
    vec3  pos;
    vec3  dir;
    vec3  atten;
    float angle;
    float penumbra;  // outer - inner
};

uniform int   numLights;
uniform Light lights[8];

// UP UNTIL NOW we are just collecting stuff from realtime.cpp

// 1 / (a + b d + c d^2), clamped to [0,1]
//distance attenuation 
//for point and spot, get weaker as light is fartehr away. not a thing with directional
float distanceFalloff(vec3 coeffs, float d) {
    float a = coeffs.x;
    float b = coeffs.y;
    float c = coeffs.z;

    float denom = a + d * (b + c * d);
    float att = 1.0 / max(denom, 1e-6);
    return min(1.0, att);
}

// Smooth spotlight falloff between inner & outer angle
//angle to azis is the angle between light direction and direction to the point
float spotFalloff(float angleToAxis, float outerAngle, float penumbra) {
    float inner = outerAngle - penumbra;
    float outer = outerAngle;

    if (angleToAxis <= inner) return 1.0;
    if (angleToAxis >= outer) return 0.0;

    float t  = (angleToAxis - inner) / (outer - inner);

    float falloff = 3.0 * t * t - 2.0 * t * t *t;
    return 1.0 - falloff;
}

//SPOTLIGHT = ATTENUATION * SPOTFALLOFF

// Diffuse + specular from one light
vec3 shadeOneLight(Light light, vec3 N, vec3 P, vec3 V) {
    
    //direction from point to light
    vec3 L;
    float attenuation = 1.0; //standard att

    if (light.type == 0) {
        // point
        vec3 disp = light.pos - P;
        float dist = length(disp);
        L = disp / dist;
        attenuation = distanceFalloff(light.atten, dist);
    } else if (light.type == 1) {
        // directional
        L = normalize(-light.dir);
        attenuation = 1.0;
    } else {
        // spot
        vec3 disp = light.pos - P;
        float dist = length(disp);
        L = disp / dist;

        attenuation = distanceFalloff(light.atten, dist);
        //for spotlights we need to get angle to axis and adjust attenuation 
        float angleToAxis = acos(dot(-L, normalize(light.dir)));
        attenuation *= spotFalloff(angleToAxis, light.angle, light.penumbra);
    }

    
    float NdotL = max(dot(N, L), 0.0);
    
    //if light behind the surface
    if (NdotL <= 0.0) {
        return vec3(0.0);
    }

    // diffuse
    vec3 diffuse  = k_d * cDiffuse * NdotL * light.color;

    // specular
    // V = view direction, from point to camera
    //R is a reflection of L about N
    
    vec3 specular = vec3(0.0);
    if (shininess > 0.0 && k_s > 0.0) {
        vec3 R      = reflect(-L, N);
        float RdotV = max(dot(R, V), 0.0);
        float sTerm = pow(RdotV, shininess);
        specular    = k_s * cSpecular * sTerm * light.color;
    }

    //each light will return its local phong contribution in RGB
    return attenuation * (diffuse + specular);
}

// MAIN

void main() {
    vec3 N = normalize(wsNormal);
    vec3 V = normalize(camPos - wsPosition);

    // ambient term
    vec3 color = k_a * cAmbient;

    // lights
    int count = min(numLights, 8);
    for (int i = 0; i < count; ++i) {
        color += shadeOneLight(lights[i], N, wsPosition, V);
    }

    color = clamp(color, 0.0, 1.0);
    fragColor = vec4(color, 1.0);

    // Debug option:
    // fragColor = vec4(abs(N), 1.0);
}

// void main() {
//     // Flat debug color
//     fragColor = vec4(1.0, 0.2, 0.2, 1.0);
//     // Or: normals
//     // fragColor = vec4(abs(normalize(wsNormal)), 1.0);
// }
