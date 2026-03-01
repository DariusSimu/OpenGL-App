#version 410 core

uniform vec3 wireColor;
out vec4 fColor;

void main() 
{
    fColor = vec4(wireColor, 1.0);
}