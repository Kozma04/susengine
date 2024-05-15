#version 460

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform vec4 meshCol;

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;


void main() {
    fragPosition = vertexPosition;
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor * meshCol;
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 1.0)));

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}