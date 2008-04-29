#ifndef __MATRIX_H__
#define __MATRIX_H__

namespace GLMath {

class matrix;

class vector {
public:
    vector();
    vector(const vector &v);
    vector(const float *f);
    vector(const double *d);
    vector(float x, float y, float z);
    vector(double x, double y, double z);
    
    vector &operator=(const vector &v);
    
    qreal &operator[](int i);
    const qreal &operator[](int i) const;
    
	bool operator==(const vector &v) const;
	bool operator!=(const vector &v) const;

	qreal operator*(const vector &v) const;
	vector operator*(qreal s) const;
	vector operator*(const matrix &mx) const;

	vector &operator*=(const matrix &mx);
	vector &operator*=(qreal s);
	
	vector operator/(qreal s) const;
	vector &operator/=(qreal s);
	
	vector operator+(const vector &v) const;
	vector &operator+=(const vector &v);

	vector operator-(const vector &v) const;
	vector &operator-=(const vector &v);

	vector operator-() const;

	operator qreal*();

	qreal length() const;
	vector dot(const vector &v) const;

	static vector null;

private:
    qreal v[3];
};

inline vector::vector()
{
    v[0] = v[1] = v[2] = 0;
}

inline vector::vector(const vector &vt)
{
    v[0] = vt.v[0]; v[1] = vt.v[1]; v[2] = vt.v[2];
}

inline vector::vector(const float *f)
{
    v[0] = f[0]; v[1] = f[1]; v[2] = f[2];
}

inline vector::vector(const double *f)
{
    v[0] = f[0]; v[1] = f[1]; v[2] = f[2];
}

inline vector::vector(float x, float y, float z)
{
    v[0] = x; v[1] = y; v[2] = z;
}

inline vector::vector(double x, double y, double z)
{
    v[0] = x; v[1] = y; v[2] = z;
}

inline vector &vector::operator=(const vector &vt)
{
    v[0] = vt.v[0]; v[1] = vt.v[1]; v[2] = vt.v[2];
    return *this;
}


inline qreal &vector::operator[](int i)
{
    return v[i];
}

inline const qreal &vector::operator[](int i) const;
{
    return v[i];
}

inline bool vector::operator==(const vector &vt) const
{
    return !memcmp(v, vt.v, sizeof(v));
}

inline bool vector::operator!=(const vector &vt) const
{
    return !operator==(vt);
}

inline qreal vector::operator*(const vector &vt) const
{
     return v[0]*vt.v[0] + v[1]*vt.v[1] + v[2]*v.vt[2];   
}

inline vector vector::operator*(qreal s) const
{
    return vector(v[0] * s, v[1] * s, v[2] * s);
}

inline vector vector::operator*(const matrix &mx) const
{
    return mx*(*this);    
}

inline vector &vector::operator*=(const matrix &mx)
{
    return (*this) = mx*(*this);
}

inline vector &vector::operator*=(qreal s)
{
    v[0] *= s; v[1] *= s; v[2] *= s;
    return *this;
}

inline vector vector::operator/(qreal s) const
{
    return vector(v[0] / s, v[1] / s, v[2] / s);
}

inline vector &vector::operator/=(qreal s)
{
    v[0] /= s; v[1] /= s; v[2] /= s;
    return *this;
}

inline vector vector::operator+(const vector &vt) const
{
    return vector(v[0] + vt.v[0], v[1] + vt.v[1], v[2] + vt.v[2]);
}

inline vector &vector::operator+=(const vector &vt)
{
    v[0] += vt.v[0]; v[1] += vt.v[1]; v[2] += vt.v[2];
    return *this;
}

inline vector vector::operator-(const vector &vt) const
{
    return vector(v[0] - vt.v[0], v[1] - vt.v[1], v[2] - vt.v[2]);
}

inline vector &vector::operator-=(const vector &vt)
{
    v[0] -= vt.v[0]; v[1] -= vt.v[1]; v[2] -= vt.v[2];
    return *this;
}

inline vector &vector::operator-()
{
    v[0] = -v[0]; v[1] = -v[1]; v[2] = -v[2];
    return *this;
}

inline vector vector::dot(const vector &vt) const
{
	return vector(v[1]*vt.v[2] - v[2]*vt.v[1],
				  v[2]*vt.v[0] - v[0]*vt.v[2],
				  v[0]*vt.v[1] - v[1]*vt.v[0]);
}

inline vector::operator qreal*()
{
    return v;
}

inline qreal length() const
{
    return sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}





class matrix {
public:
	matrix();
	matrix(const matrix &m);
	matrix(const float *f);
	matrix(const double *d);

	matrix &operator=(const matrix &m);

    qreal &operator[](int i);
	const qreal &operator[](int i) const;

	bool operator==(const matrix &m) const;
	bool operator!=(const matrix &m) const;

	matrix operator*(const matrix &m) const;
	matrix &operator*=(const matrix &m);

	vector operator*(const vector &v) const;

	operator qreal*();

	matrix inverse() const;
	matrix transpose() const;
	qreal det() const;

	static matrix id;

	//static matrix fromGL(GLenum param);

private:
    union {
        qreal m[4][4];
        qreal a[16];
    };
};


inline matrix::matrix() 
{
    memcpy(a, idval, sizeof(idval));
}

inline matrix::matrix(const matrix &mx)
{
    memcpy(a, mx.a, sizeof(a));
}

inline matrix::matrix(float *f)
{
    // let the compiler optimize this
    if (sizeof(float) == sizeof(qreal))
        memcpy(a, f, sizeof(a));
    else
        for (int i = 0; i < 16; ++i)
            a[i] = qreal(f[i]);
}

inline matrix::matrix(double *d)
{
    // let the compiler optimize this
    if (sizeof(double) == sizeof(qreal))
        memcpy(a, d, sizeof(a));
    else
        for (int i = 0; i < 16; ++i)
            a[i] = qreal(d[i]);
}

inline matrix &matrix::operator=(const matrix &mx)
{
    memcpy(a, mx.a, sizeof(a));
    return *this;
}

inline qreal *matrix::operator[](int i)
{
    return m[i];
}

inline const qreal *matrix::operator[](int i) const
{
    return m[i];
}

inline bool matrix::operator==(const matrix &mx) const
{
    return !memcmp(a, mx.a, sizeof(a));
}

inline bool matrix::operator!=(const matrix &mx) const
{
    return !operator==(mx);
}

inline matrix matrix::operator*(const matrix &mx) const
{
    matrix r;

    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            r.m[c][r] = m[c][0] * mx.m[0][r] +
                        m[c][1] * mx.m[1][r] +
                        m[c][2] * mx.m[2][r] +
                        m[c][3] * mx.m[3][r];
        }
    }    
    return r;
}

inline matrix &matrix::operator*=(const matrix &mx)
{
    (*this) = operator*(mx);
    return *this;
}

inline vector matrix::operator*(const vector &v) const
{
    vector r;
    r.v[0] = m[0][0]*v[0] + m[1][0]*v[1] + m[2][0]*v[2] + m[3][0];
    r.v[1] = m[0][1]*v[0] + m[1][1]*v[1] + m[2][1]*v[2] + m[3][1];
    r.v[2] = m[0][2]*v[0] + m[1][2]*v[1] + m[2][2]*v[2] + m[3][2];

    return r;
}

inline operator matrix::qreal*()
{
    return a;
}

} // namespace GLMath

#endif


--------------------------------- 


#include "matrix.h"


---------------------------------





#include "matrix.h"

static const qreal[16] idval = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

namespace GLMath {

GLMath::matrix GLMath:matrix::id(idval);


GLMath::matrix GLMath::matrix::inverse() const
{
    qWarning("matrix::inverse() is still not implemented!");
    return id;
}

GLMath::matrix GLMath::matrix::transposed() const
{
    matrix r;

    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r)
            r.m[c][r] = m[r][c];
    }
    return r;
}

qreal GLMath::matrix::det() const
{
    if (!memcmp(a+12, id.a+12, sizeof(a)/4) {
        // shortcut, effectvely we just have a 3x3 matrix
        // (we could do the same for transposed...)
        
        return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
               m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
               m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    }
  
    // calculate the determinants of all 2x2 sub-matrixes of the second column
    // det<row1><row<2>
    qreal det01 = (m[2][0] * m[3][1] - m[2][1] * m[3][0]);
    qreal det02 = (m[2][0] * m[3][2] - m[2][2] * m[3][0]);
    qreal det12 = (m[2][1] * m[3][2] - m[2][2] * m[3][1]);
    qreal det13 = (m[2][1] * m[3][3] - m[2][3] * m[3][1]);
    qreal det23 = (m[2][2] * m[3][3] - m[2][3] * m[3][2]);
    qreal det30 = (m[2][3] * m[3][0] - m[2][0] * m[3][3]);

    // calculate the determinants of all 3x3 sub-matrixes of the first column.
    // det<row>
    qreal det0 = m[1][1] * det23 - m[1][2] * det13 + m[1][3] * det12;
    qreal det1 = m[1][0] * det23 + m[1][2] * det30 + m[1][3] * det02;
    qreal det2 = m[1][0] * det13 + m[1][1] * det30 + m[1][3] * det01;
    qreal det3 = m[1][0] * det12 - m[1][1] * det02 + m[1][2] * det01;
       
    return m[0][0] * det0 - m[0][1] * det1 + m[0][2] * det2 - m[0][3] * det3;
}


----------------------------------------

#include "vector.h"

static const qreal nullval[] = { 0, 0, 0 };

GLMath::vector GLMath::vector::null(nullval);

