attribute highp vec3 vertex;
attribute highp vec3 normal;
attribute highp vec4 color;

varying highp vec3 v;
varying highp vec3 n;
varying highp vec4 c;

uniform highp mat4 projMatrix;
uniform highp mat4 modelMatrix;
uniform highp mat4 viewMatrix;
uniform highp mat3 normalMatrix;


void main()
{
    n = normalMatrix * normal;
    v = vec3(modelMatrix * vec4(vertex, 1));
    gl_Position = projMatrix * viewMatrix * vec4(v, 1);
    c = color;
}
