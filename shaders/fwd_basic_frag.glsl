#version 330

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
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;

uniform sampler2D texture0;

uniform PointLight lightPoint[16];
uniform DirectionalLight lightDir[4];
uniform vec3 lightAmbient;
uniform int nLightDir;
uniform int nLightPoint;

out vec4 finalColor;


void main()
{
    vec4 texelColor = fragColor;
    vec3 light = lightAmbient;

    for(int i = 0; i < nLightDir; i++) {
        light += max(dot(lightDir[i].dir, -fragNormal), 0.0) * lightDir[i].color;
    }

    for(int i = 0; i < nLightPoint; i++) {
        vec3 disp = lightPoint[i].pos - fragPosition;
        vec3 dir = normalize(disp);
        float dist = length(disp);
        float attenuation = min(lightPoint[i].range / (dist * dist + lightPoint[i].range), 1.0);

        light += max(dot(dir, fragNormal), 0.0) * lightPoint[i].color * attenuation;
    }

    finalColor = vec4(texelColor.rgb * light, 1);
}