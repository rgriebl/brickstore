

namespace LDraw {

class vector3 {

    operator
};

class vector4 {
    
};

class matrix {
public:
    static matrix fromGL(GLenum glenum);

    inline operator const qreal *() { return m; }

private:
    qreal m[16];
};

operator

}
