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
uniform float time;
uniform vec3 cameraPosition;

uniform mat4 shadowProj[SHADOW_CASCADES];
uniform int nShadowMaps;

out vec4 fragPositionShadow[SHADOW_CASCADES];
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;


/*vec3 calculateScreenSpaceNormal(vec3 p) {
    vec3 dx = dFdx(p);
    vec3 dy = dFdy(p);
    return normalize(cross(dx, dy));
}*/

vec3 finalPos(vec3 pos) {
    vec3 p1 = pos * normalize(vec3(1, 0, 0.7)) * 0.2;
    vec3 p2 = pos * normalize(vec3(1, 0, 0.1)) * 0.2;
    vec3 p3 = pos * normalize(vec3(-1, 0, -0.5)) * 0.2;
    vec3 p4 = pos * normalize(vec3(-0.6, 0, 0.7)) * 0.2;

    float t = time * 2;
    float f1 = 0.6, a1 = 2;
    float f2 = 1, a2 = 1.1;
    float f3 = 1.4, a3 = 0.7;
    float f4 = 1.9, a4 = 0.4;

    float yDisp = sin((p1.x + p1.z + t) * f1) * a1;
    yDisp += sin((p2.x + p2.z + t) * f2) * a2;
    yDisp += sin((p3.x + p3.z + t) * f3) * a3;
    yDisp += sin((p4.x + p4.z + t) * f4) * a4;
    return pos + vec3(0, yDisp * 0.45, 0);
}

void main() {
    vec3 vp1 = finalPos(vertexPosition);
    vec3 vp2 = finalPos(vertexPosition + vec3(2, 0, 0));
    vec3 vp3 = finalPos(vertexPosition + vec3(0, 0, 2));

    fragPosition = vec3(matModel * vec4(vp1, 1.0));
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor * meshCol;

    vec3 dx = vp2 - vp1, dy = vp3 - vp1;
    fragNormal = normalize(cross(dx, dy));  //normalize(vec3(matNormal * vec4(vertexNormal, 1.0)));

    for(int i = 0; i < nShadowMaps; i++)
        fragPositionShadow[i] = shadowProj[i] * matModel * vec4(vertexPosition, 1.0);


    gl_Position = mvp * vec4(vp1, 1.0);
}