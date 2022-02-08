uniform highp vec3 lightPos;
uniform highp vec3 cameraPos;

varying highp vec3 v;
varying highp vec3 n;
varying highp vec4 c;

const highp vec3 lightColor = vec3(1.0, 1.0, 1.0);

void main(void) {
    // ambient
    highp float ambientStrength = 0.4;
    highp vec3 ambient = ambientStrength * lightColor;

    // diffuse
    highp vec3 norm = normalize(n);
    highp vec3 lightDir = normalize(lightPos - v);
    highp float diff = max(dot(norm, lightDir), 0.0);
    highp vec3 diffuse = diff * lightColor;

    // specular
    highp float specularStrength = 0.3;
    highp vec3 viewDir = normalize(cameraPos - v);
    highp vec3 reflectDir = reflect(-lightDir, norm);
    highp float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.);
    highp vec3 specular = specularStrength * spec * lightColor;

    highp vec3 result = (ambient + diffuse + specular) * c.rgb;
    gl_FragColor = vec4(result, c.a);
}
