uniform highp vec3 lightPos;
uniform highp vec3 cameraPos;

varying highp vec3 v;
varying highp vec4 c;


void main()
{
    gl_FragColor = c;
}
