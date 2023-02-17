// Copyright (C) 2004-2023 Robert Griebl
// SPDX-License-Identifier: GPL-3.0-only

// Instanced line drawing. Based heavily on:
// https://wwwtyro.net/2019/11/18/instanced-lines.html
// The example code is in the public domain.

VARYING vec4 col;

void MAIN()
{
    vec3 position = VERTEX;
    vec3 p0 = qt_instanceTransform0.xyz;
    vec3 p1 = qt_instanceTransform1.xyz;
    vec3 p2 = qt_instanceTransform2.xyz;
    vec3 p3 = qt_instanceData.xyz;

    mat4 mvp = qt_viewProjectionMatrix * qt_parentMatrix * qt_modelMatrix;

    // Next we'll transform our vertices to clip space:
    vec4 clip0 = mvp * vec4(p0, 1.0);
    vec4 clip1 = mvp * vec4(p1, 1.0);

    // Then transform them to screen space:
    vec2 screen0 = resolution * (0.5 * clip0.xy / clip0.w + 0.5);
    vec2 screen1 = resolution * (0.5 * clip1.xy / clip1.w + 0.5);

    // Now check for conditional lines
    if (qt_instanceData.w > 0) {
        vec4 clip2 = mvp * vec4(p2, 1.0);
        vec4 clip3 = mvp * vec4(p3, 1.0);
        vec2 screen2 = resolution * (0.5 * clip2.xy / clip2.w + 0.5);
        vec2 screen3 = resolution * (0.5 * clip3.xy / clip3.w + 0.5);

        vec2 c10 = screen1 - screen0;
        vec2 c20 = screen2 - screen0;
        vec2 c30 = screen3 - screen0;

        if (sign(c10.x * c20.y - c10.y * c20.x) != sign(c10.x * c30.y - c10.y * c30.x)) {
            POSITION = vec4(0, 0, 0, 0);
            col = vec4(0, 0, 0, 0);
            return;
        }
    }

    //Then we'll expand the line segment as usual:
    vec2 xBasis = normalize(screen1 - screen0);
    vec2 yBasis = vec2(-xBasis.y, xBasis.x);
    vec2 pt0 = screen0 + customLineWidth * (position.x * xBasis + position.y * yBasis);
    vec2 pt1 = screen1 + customLineWidth * (position.x * xBasis + position.y * yBasis);
    vec2 pt = mix(pt0, pt1, position.z);

    // We'll also interpolate our clip coordinates across position.z so that we can pull
    // out interpolated z and w components.
    vec4 clip = mix(clip0, clip1, position.z);

    // Lines in inside corners of parts are a problem, because they are completely hidden
    // by the surounding surfaces' geometry. We can get around that by moving the line
    // towards the camera:
    //                ____ <-- this line
    //                 /\
    //   surface A -> /  \ <- surface B


    clip.z -= customLineWidth * sqrt(2) / resolution.x;
    clip = normalize(clip);

    // Now we'll recover our final position by converting our vertex back into clip space.
    // Note we're undoing the perspective divide we performed earlier:

    POSITION = vec4(clip.w * ((2.0 * pt) / resolution - 1.0), clip.z, clip.w);
    col = INSTANCE_COLOR;
}
