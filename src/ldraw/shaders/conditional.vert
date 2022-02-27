attribute highp vec3 vertex0;
attribute highp vec3 vertex1;
attribute highp vec3 vertex2;
attribute highp vec3 vertex3;
attribute highp vec4 color;

varying highp vec3 v;
varying highp vec4 c;

uniform highp mat4 projMatrix;
uniform highp mat4 modelMatrix;
uniform highp mat4 viewMatrix;


void main()
{
    v = vec3(modelMatrix * vec4(vertex0, 1));

    mat4 m = projMatrix * viewMatrix * modelMatrix;
    vec4 v0 = m * vec4(vertex0, 1);
    vec4 v1 = m * vec4(vertex1, 1);
    vec4 v2 = m * vec4(vertex2, 1);
    vec4 v3 = m * vec4(vertex3, 1);

    // My old algorithm used on the GPU didn't translate too well into a shader
    // This is now mostly based on the idea behind LeoCAD's unlit_color_conditional_vs.glsl

    vec2 l = v1.xy - v0.xy;
    vec2 check20 = v2.xy - v0.xy;
    vec2 check30 = v3.xy - v0.xy;

    float cross20 = l.x * check20.y - l.y * check20.x;
    float cross30 = l.x * check30.y - l.y * check30.x;

    if (cross20 * cross30 >= 0.0) {
        gl_Position = projMatrix * viewMatrix * vec4(v, 1);
        c = color;
    } else {
        gl_Position = vec4(10000, 10000, 0, 0);
        c = vec4(0, 0, 0, 0);
    }
}
