#version 150 core

uniform vec3 lightPos;
uniform vec3 cameraPos;

in vec3 v;
in vec4 c;

out vec4 color;


void main()
{
    color = c;
}
