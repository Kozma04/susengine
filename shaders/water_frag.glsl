#version 460

#define SHADOW_CASCADES 8


in vec3 fragPosition;

uniform samplerCube environmentMap;
uniform sampler2D texDiffuse;
uniform vec3 cameraPosition;
uniform float time;

uniform sampler2D texture0;
uniform sampler2D shadowMap[SHADOW_CASCADES];
uniform int nShadowMaps;

in vec4 fragPositionShadow[SHADOW_CASCADES];
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;
out vec4 finalColor;

float ShadowCalculation() {
    vec3 projCoords;
    float shadow = 1.0, mapDepth;

    for(int i = 0; i < nShadowMaps; i++) {
        projCoords = fragPositionShadow[i].xyz / fragPositionShadow[i].w;
        projCoords = projCoords * 0.5 + 0.5;
        mapDepth = texture(shadowMap[i], projCoords.xy).r;
        if(projCoords.x < 0 || projCoords.x > 1 || projCoords.y < 0 || projCoords.y > 1)
            continue;
        shadow = projCoords.z - 0.0004 < mapDepth ? 1.0 : 0.0;
        break;
    }
    
    return shadow;
}

void main()
{
    vec3 color = vec3(0.8);
    vec3 normal = fragNormal;

    vec3 I = normalize(fragPosition - cameraPosition);
    vec3 N = normalize(normal);
    vec3 R = reflect(I, N);
    vec3 F = normalize(refract(I, -N, 1.0 / 1.33));

    float fresnel = pow(abs(dot(I, N)), 5.f);

    vec3 tex = texture(texDiffuse, fragTexCoord * 80 + vec2(time / 3.f) * vec2(normalize(vec2(1, 0.7)))).rgb;
    tex *= 1.2;

    vec3 colR = pow(texture(environmentMap, R).rgb, vec3(1.0 / 1.5));
    vec3 colF = pow(texture(environmentMap, F).rgb, vec3(1.0 / 1.5));

    if(ShadowCalculation() < 0.5) colF *= 0.2;
    color = mix(colR, colF, fresnel);
    color = mix(color, tex, 0.3);
    //color = texture(environmentMap, F).rgb;
    //color = mix(vec3(1, 1, 1), vec3(0), fresnel);

    
    finalColor = vec4(color, 1.0) * fragColor;
}