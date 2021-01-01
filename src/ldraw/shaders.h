/* Copyright (C) 2004-2021 Robert Griebl. All rights reserved.
**
** This file is part of BrickStore.
**
** This file may be distributed and/or modified under the terms of the GNU
** General Public License version 2 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://fsf.org/licensing/licenses/gpl.html for GPL licensing information.
*/
#pragma once

static const char *vertexShaderSourceSimple =
        "#version 120\n"
        "uniform mat4 projMatrix;\n"
        "uniform mat4 mvMatrix;\n"
        "attribute vec4 vertex;\n"
        "attribute vec4 color;\n"
        "varying vec4 c;\n"
        "void main() {\n"
        "  gl_Position = projMatrix * mvMatrix * vertex;\n"
        "  c = color;\n"
        "}\n";


static const char *fragmentShaderSourceSimple =
        "#version 120\n"
        "varying vec4 c;\n"
        "void main() {\n"
        "  gl_FragColor = c;\n"
        "}\n";

