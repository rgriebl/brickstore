#version 150 core

uniform mat4 projMatrix;
uniform mat4 modelMatrix;
uniform mat4 viewMatrix;

in vec3 vertex;
in vec4 color;

out vec3 v;
out vec4 c;


void main()
{
    v = vec3(modelMatrix * vec4(vertex, 1));
    gl_Position = projMatrix * viewMatrix * vec4(v, 1);
    c = color;
}
