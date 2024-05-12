#version 460


#define SHADOW_CASCADES 8


in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform vec4 meshCol;

uniform mat4 shadowProj[SHADOW_CASCADES];
uniform int nShadowMaps;

out vec3 fragPosition;
out vec4 fragPositionShadow[SHADOW_CASCADES];
out vec2 fragTexCoord;
out vec4 fragColor;
flat out vec3 fragNormal;


void main() {
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor * meshCol;
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 1.0)));
    
    for(int i = 0; i < nShadowMaps; i++)
        fragPositionShadow[i] = shadowProj[i] * matModel * vec4(vertexPosition, 1.0);

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}