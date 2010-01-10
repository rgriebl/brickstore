/* Copyright (C) 2004-2008 Robert Griebl. All rights reserved.
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

#include <cstdio>
#include "matrix_t.h"

static const float idval[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };


matrix_t matrix_t::id(idval);


matrix_t matrix_t::inversed() const
{
    std::fprintf(stderr, "matrix::inverse() is still not implemented!\n");
    return id;
}

matrix_t matrix_t::transposed() const
{
    matrix_t x;

    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r)
            x.m[c][r] = m[r][c];
    }
    return x;
}

float matrix_t::det() const
{
    if (!memcmp(a+12, id.a+12, sizeof(a)/4)) {
        // shortcut, effectvely we just have a 3x3 matrix
        // (we could do the same for transposed...)
        
        return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
               m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
               m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    }
  
    // calculate the determinants of all 2x2 sub-matrixes of the second column
    // det<row1><row<2>
    float det01 = (m[2][0] * m[3][1] - m[2][1] * m[3][0]);
    float det02 = (m[2][0] * m[3][2] - m[2][2] * m[3][0]);
    float det12 = (m[2][1] * m[3][2] - m[2][2] * m[3][1]);
    float det13 = (m[2][1] * m[3][3] - m[2][3] * m[3][1]);
    float det23 = (m[2][2] * m[3][3] - m[2][3] * m[3][2]);
    float det30 = (m[2][3] * m[3][0] - m[2][0] * m[3][3]);

    // calculate the determinants of all 3x3 sub-matrixes of the first column.
    // det<row>
    float det0 = m[1][1] * det23 - m[1][2] * det13 + m[1][3] * det12;
    float det1 = m[1][0] * det23 + m[1][2] * det30 + m[1][3] * det02;
    float det2 = m[1][0] * det13 + m[1][1] * det30 + m[1][3] * det01;
    float det3 = m[1][0] * det12 - m[1][1] * det02 + m[1][2] * det01;
       
    return m[0][0] * det0 - m[0][1] * det1 + m[0][2] * det2 - m[0][3] * det3;
}

#include <qglobal.h>

#if !defined(QT_NO_OPENGL)

#include <qgl.h>

matrix_t matrix_t::fromGL(int param)
{
    matrix_t m;
    glGetFloatv(param, m.a);
    return m;
}

#endif // !QT_NO_OPENGL
