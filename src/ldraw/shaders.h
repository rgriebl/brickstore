// Copyright (C) 2004-2025 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

static const char *vertexShaderSourcePhong20 =
        "#version 120\n"

        "attribute vec3 vertex;\n"
        "attribute vec3 normal;\n"
        "attribute vec4 color;\n"

        "varying vec3 v;\n"
        "varying vec3 n;\n"
        "varying vec4 c;\n"

        "uniform mat4 projMatrix;\n"
        "uniform mat4 modelMatrix;\n"
        "uniform mat4 viewMatrix;\n"
        "uniform mat3 normalMatrix;\n"

        "void main() {\n"
        "  n = normalMatrix * normal;\n"
        "  v = vec3(modelMatrix * vec4(vertex, 1));\n"
        "  gl_Position = projMatrix * viewMatrix * vec4(v, 1);\n"
        "  c = color;\n"
        "}\n";


static const char *vertexShaderSourcePhong33 =
        "#version 330 core\n"

        "layout (location = 0) in vec3 vertex;\n"
        "layout (location = 1) in vec3 normal;\n"
        "layout (location = 2) in vec4 color;\n"

        "out vec3 v;\n"
        "out vec3 n;\n"
        "out vec4 c;\n"

        "uniform mat4 projMatrix;\n"
        "uniform mat4 modelMatrix;\n"
        "uniform mat4 viewMatrix;\n"
        "uniform mat3 normalMatrix;\n"

        "void main() {\n"
        "  n = normalMatrix * normal;\n"
        "  v = vec3(modelMatrix * vec4(vertex, 1));\n"
        "  gl_Position = projMatrix * viewMatrix * vec4(v, 1);\n"
        "  c = color;\n"
        "}\n";




static const char *fragmentShaderSourceSimple20 =
        "#version 120\n"
        "varying vec3 v;\n"
        "varying vec3 n;\n"
        "varying vec4 c;\n"
        "void main() {\n"
        "  gl_FragColor = c;\n"
        "}\n";

static const char *fragmentShaderSourcePhong20 =
        "#version 120\n"

        "uniform vec3 lightPos;\n"
        "uniform vec3 cameraPos;\n"

        "varying vec3 v;\n"
        "varying vec3 n;\n"
        "varying vec4 c;\n"

        "const vec3 lightColor = vec3(1.0, 1.0, 1.0);\n"

        "void main() {\n"
        // ambient
        "  float ambientStrength = 0.4;\n"
        "  vec3 ambient = ambientStrength * lightColor;\n"

        // diffuse
        "  vec3 norm = normalize(n);\n"
        "  vec3 lightDir = normalize(lightPos - v);\n"
        "  float diff = max(dot(norm, lightDir), 0.0);\n"
        "  vec3 diffuse = diff * lightColor;\n"

        // specular
        "  float specularStrength = 0.3;\n"
        "  vec3 viewDir = normalize(cameraPos - v);\n"
        "  vec3 reflectDir = reflect(-lightDir, norm);\n"
        "  float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);\n"
        "  vec3 specular = specularStrength * spec * lightColor;\n"

        "  vec3 result = (ambient + diffuse + specular) * c.rgb;\n"
        "  gl_FragColor = vec4(result, c.a);\n"
        "}\n";


static const char *fragmentShaderSourcePhong33 =
        "#version 330 core\n"

        "uniform vec3 lightPos;\n"
        "uniform vec3 cameraPos;\n"

        "in vec3 v;\n"
        "in vec3 n;\n"
        "in vec4 c;\n"

        "out vec4 color;\n"

        "const vec3 lightColor = vec3(1.0, 1.0, 1.0);\n"

        "void main() {\n"
        // ambient
        "  float ambientStrength = 0.4;\n"
        "  vec3 ambient = ambientStrength * lightColor;\n"

        // diffuse
        "  vec3 norm = normalize(n);\n"
        "  vec3 lightDir = normalize(lightPos - v);\n"
        "  float diff = max(dot(norm, lightDir), 0.0);\n"
        "  vec3 diffuse = diff * lightColor;\n"

        // specular
        "  float specularStrength = 0.3;\n"
        "  vec3 viewDir = normalize(cameraPos - v);\n"
        "  vec3 reflectDir = reflect(-lightDir, norm);\n"
        "  float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);\n"
        "  vec3 specular = specularStrength * spec * lightColor;\n"

        "  vec3 result = (ambient + diffuse + specular) * c.rgb;\n"
        "  color = vec4(result, c.a);\n"
        "}\n";
