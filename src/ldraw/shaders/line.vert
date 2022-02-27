attribute highp vec3 vertex;
attribute highp vec4 color;

varying highp vec3 v;
varying highp vec4 c;

uniform highp mat4 projMatrix;
uniform highp mat4 modelMatrix;
uniform highp mat4 viewMatrix;


void main()
{
    v = vec3(modelMatrix * vec4(vertex, 1));
    gl_Position = projMatrix * viewMatrix * vec4(v, 1);
    c = color;
}
