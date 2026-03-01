#version 410 core
layout (location = 0) in vec3 vPosition;

uniform mat4 model;
uniform mat4 lightSpaceTrMatrix;

out vec4 fWorldPos;

void main()
{
    fWorldPos = model * vec4(vPosition, 1.0);
    gl_Position = lightSpaceTrMatrix * fWorldPos;
}