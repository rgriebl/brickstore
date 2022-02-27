#version 150 core

uniform vec3 lightPos;
uniform vec3 cameraPos;

in vec3 v;
in vec3 n;
in vec4 c;

out vec4 color;

const vec3 lightColor = vec3(1.0, 1.0, 1.0);
const float ambientStrength = 0.4;
const float specularStrength = 0.2;


void main()
{
    // ambient
    vec3 ambient = ambientStrength * lightColor;

    // diffuse
    vec3 norm = normalize(n);
    vec3 lightDir = normalize(lightPos - v);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // specular
    vec3 viewDir = normalize(cameraPos - v);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 result = (ambient + diffuse + specular) * c.rgb;
    color = vec4(result, c.a);
}
