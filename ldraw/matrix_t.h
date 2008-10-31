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
#ifndef __MATRIX_T_H__
#define __MATRIX_T_H__

#include <cstring>
#include <cmath>

class matrix_t;

class vector_t {
public:
    vector_t();
    vector_t(const vector_t &v);
    vector_t(const float *f);
    vector_t(const double *d);
    vector_t(int x, int y, int z);
    vector_t(float x, float y, float z);
    vector_t(double x, double y, double z);
    
    vector_t &operator=(const vector_t &v);
    
    float &operator[](int i);
    const float &operator[](int i) const;
    
	bool operator==(const vector_t &v) const;
	bool operator!=(const vector_t &v) const;

	float operator*(const vector_t &v) const;
	vector_t operator*(float s) const;
	vector_t operator*(const matrix_t &mx) const;

	vector_t &operator*=(const matrix_t &mx);
	vector_t &operator*=(float s);

	vector_t operator%(const vector_t &v) const;  // % == cross product
	vector_t &operator%=(const vector_t &v);      // % == cross product
	
	vector_t operator/(float s) const;
	vector_t &operator/=(float s);
	
	vector_t operator+(const vector_t &v) const;
	vector_t &operator+=(const vector_t &v);

	vector_t operator-(const vector_t &v) const;
	vector_t &operator-=(const vector_t &v);

	vector_t operator-() const;

	operator const float*() const;

	float length() const;
	vector_t cross(const vector_t &v) const;
	float dot(const vector_t &v) const;

	static vector_t null;

private:
    float v[3];
    
    friend class matrix_t;
};



class matrix_t {
public:
	matrix_t();
	matrix_t(const matrix_t &m);
	matrix_t(const float *f);
	matrix_t(const double *d);

	matrix_t &operator=(const matrix_t &m);

    float *operator[](int i);
	const float *operator[](int i) const;

	bool operator==(const matrix_t &m) const;
	bool operator!=(const matrix_t &m) const;

	matrix_t operator*(const matrix_t &m) const;
	matrix_t &operator*=(const matrix_t &m);

	vector_t operator*(const vector_t &v) const;

	operator const float*() const;

	matrix_t inversed() const;
	matrix_t transposed() const;
	float det() const;

	static matrix_t id;

	static matrix_t fromGL(int param);

private:
    union {
        float m[4][4];
        float a[16];
    };
    
    friend class vector_t;
};


inline vector_t::vector_t()
{
    v[0] = v[1] = v[2] = 0;
}

inline vector_t::vector_t(const vector_t &vt)
{
    v[0] = vt.v[0]; v[1] = vt.v[1]; v[2] = vt.v[2];
}

inline vector_t::vector_t(const float *f)
{
    v[0] = f[0]; v[1] = f[1]; v[2] = f[2];
}

inline vector_t::vector_t(const double *f)
{
    v[0] = float(f[0]); v[1] = float(f[1]); v[2] = float(f[2]);
}

inline vector_t::vector_t(int x, int y, int z)
{
    v[0] = float(x); v[1] = float(y); v[2] = float(z);
}

inline vector_t::vector_t(float x, float y, float z)
{
    v[0] = x; v[1] = y; v[2] = z;
}

inline vector_t::vector_t(double x, double y, double z)
{
    v[0] = float(x); v[1] = float(y); v[2] = float(z);
}

inline vector_t &vector_t::operator=(const vector_t &vt)
{
    v[0] = vt.v[0]; v[1] = vt.v[1]; v[2] = vt.v[2];
    return *this;
}


inline float &vector_t::operator[](int i)
{
    return v[i];
}

inline const float &vector_t::operator[](int i) const
{
    return v[i];
}

inline bool vector_t::operator==(const vector_t &vt) const
{
    return !std::memcmp(v, vt.v, sizeof(v));
}

inline bool vector_t::operator!=(const vector_t &vt) const
{
    return !operator==(vt);
}

inline float vector_t::operator*(const vector_t &vt) const
{
     return v[0]*vt.v[0] + v[1]*vt.v[1] + v[2]*vt.v[2];   
}

inline vector_t vector_t::operator*(float s) const
{
    return vector_t(v[0] * s, v[1] * s, v[2] * s);
}

inline vector_t vector_t::operator*(const matrix_t &mx) const
{
    return mx*(*this);    
}

inline vector_t &vector_t::operator*=(const matrix_t &mx)
{
    return (*this) = mx*(*this);
}

inline vector_t &vector_t::operator*=(float s)
{
    v[0] *= s; v[1] *= s; v[2] *= s;
    return *this;
}

inline vector_t vector_t::operator/(float s) const
{
    return vector_t(v[0] / s, v[1] / s, v[2] / s);
}

inline vector_t &vector_t::operator/=(float s)
{
    v[0] /= s; v[1] /= s; v[2] /= s;
    return *this;
}

inline vector_t vector_t::operator+(const vector_t &vt) const
{
    return vector_t(v[0] + vt.v[0], v[1] + vt.v[1], v[2] + vt.v[2]);
}

inline vector_t &vector_t::operator+=(const vector_t &vt)
{
    v[0] += vt.v[0]; v[1] += vt.v[1]; v[2] += vt.v[2];
    return *this;
}

inline vector_t vector_t::operator-(const vector_t &vt) const
{
    return vector_t(v[0] - vt.v[0], v[1] - vt.v[1], v[2] - vt.v[2]);
}

inline vector_t &vector_t::operator-=(const vector_t &vt)
{
    v[0] -= vt.v[0]; v[1] -= vt.v[1]; v[2] -= vt.v[2];
    return *this;
}

inline vector_t vector_t::operator-() const
{
    return vector_t(-v[0], -v[1], -v[2]);
}

inline vector_t vector_t::operator%(const vector_t &vt) const
{
    return vector_t(v[1] * vt.v[2] - v[2] * vt.v[1],
                    v[2] * vt.v[0] - v[0] * vt.v[2],
                    v[0] * vt.v[1] - v[1] * vt.v[0]);
}

inline vector_t &vector_t::operator%=(const vector_t &vt)
{
    return (*this) = (*this) % vt;;
}

inline float vector_t::dot(const vector_t &vt) const
{
    return operator*(vt);
}

inline vector_t vector_t::cross(const vector_t &vt) const
{
    return operator%(vt);
}

inline vector_t::operator const float*() const
{
    return v;
}

inline float vector_t::length() const
{
    return std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}





inline matrix_t::matrix_t() 
{
    memcpy(a, id.a, sizeof(a));
}

inline matrix_t::matrix_t(const matrix_t &mx)
{
    memcpy(a, mx.a, sizeof(a));
}

inline matrix_t::matrix_t(const float *f)
{
    // let the compiler optimize this
    if (sizeof(float) == sizeof(float))
        memcpy(a, f, sizeof(a));
    else
        for (int i = 0; i < 16; ++i)
            a[i] = float(f[i]);
}

inline matrix_t::matrix_t(const double *d)
{
    // let the compiler optimize this
    if (sizeof(double) == sizeof(float))
        memcpy(a, d, sizeof(a));
    else
        for (int i = 0; i < 16; ++i)
            a[i] = float(d[i]);
}

inline matrix_t &matrix_t::operator=(const matrix_t &mx)
{
    memcpy(a, mx.a, sizeof(a));
    return *this;
}

inline float *matrix_t::operator[](int i)
{
    return m[i];
}

inline const float *matrix_t::operator[](int i) const
{
    return m[i];
}

inline bool matrix_t::operator==(const matrix_t &mx) const
{
    return !std::memcmp(a, mx.a, sizeof(a));
}

inline bool matrix_t::operator!=(const matrix_t &mx) const
{
    return !operator==(mx);
}

inline matrix_t matrix_t::operator*(const matrix_t &mx) const
{
    matrix_t x;

    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            x.m[c][r] = m[c][0] * mx.m[0][r] +
                        m[c][1] * mx.m[1][r] +
                        m[c][2] * mx.m[2][r] +
                        m[c][3] * mx.m[3][r];
        }
    }    
    return x;
}

inline matrix_t &matrix_t::operator*=(const matrix_t &mx)
{
    (*this) = operator*(mx);
    return *this;
}

inline vector_t matrix_t::operator*(const vector_t &v) const
{
    vector_t r;
    r.v[0] = m[0][0]*v[0] + m[1][0]*v[1] + m[2][0]*v[2] + m[3][0];
    r.v[1] = m[0][1]*v[0] + m[1][1]*v[1] + m[2][1]*v[2] + m[3][1];
    r.v[2] = m[0][2]*v[0] + m[1][2]*v[1] + m[2][2]*v[2] + m[3][2];

    return r;
}

inline matrix_t::operator const float*() const
{
    return a;
}

#endif
