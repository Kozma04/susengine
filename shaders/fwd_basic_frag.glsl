// https://learnopengl.com/code_viewer_gh.php?code=src/5.advanced_lighting/3.1.3.shadow_mapping/3.1.3.shadow_mapping.fs

#version 460


#define SHADOW_CASCADES 8


struct PointLight {
    vec3 pos;
    vec3 color;
    float range;
};

struct DirectionalLight {
    vec3 dir;
    vec3 color;
};


in vec3 fragPosition;
in vec4 fragPositionShadow[SHADOW_CASCADES];
in vec2 fragTexCoord;
flat in vec3 fragNormal;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D shadowMap[SHADOW_CASCADES];
uniform int nShadowMaps;

uniform PointLight lightPoint[16];
uniform DirectionalLight lightDir[4];
uniform vec3 lightAmbient;
uniform int nLightDir;
uniform int nLightPoint;

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

void main() {
    vec4 texelColor = fragColor * texture(texture0, fragTexCoord);
    vec3 light = lightAmbient;

    for(int i = 0; i < nLightDir; i++) {
        vec3 intensity = max(dot(lightDir[i].dir, -fragNormal), 0.0) * lightDir[i].color;
        light += intensity * ShadowCalculation();
    }

    for(int i = 0; i < nLightPoint; i++) {
        vec3 disp = lightPoint[i].pos - fragPosition;
        vec3 dir = normalize(disp);
        float dist = length(disp);
        float attenuation = min(lightPoint[i].range / (dist * dist + lightPoint[i].range), 1.0);

        light += max(dot(dir, fragNormal), 0.0) * lightPoint[i].color * attenuation;
    }

    finalColor = vec4(texelColor.rgb * light, fragColor.a);
    //finalColor = vec4(vec3(texture(shadowMap[0], gl_FragCoord.xy / vec2(2000, 1250)).r), 1);
    //finalColor = vec4(fragPositionShadow[0].xy / fragPositionShadow[0].w, 0, 1);
}