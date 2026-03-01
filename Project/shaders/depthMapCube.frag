#version 410 core
in vec4 fWorldPos;

uniform vec3 lightPos;
uniform float far_plane;

void main()
{
    float lightDistance = length(fWorldPos.xyz - lightPos);
    lightDistance /= far_plane;
    gl_FragDepth = lightDistance;
}
