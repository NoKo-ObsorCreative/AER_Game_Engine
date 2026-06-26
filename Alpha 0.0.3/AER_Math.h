#pragma once
// ============================================================
// AER_Math.h  –  Vectors, Matrices, Quaternions
// AER Game Engine  |  Alpha 0.3
// ============================================================
#ifndef AER_MATH_H
#define AER_MATH_H

#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace AER {

// ---------------------------------------------------------------- Vec2 -----
struct Vec2 {
    float x = 0, y = 0;
    Vec2() = default;
    Vec2(float x, float y) : x(x), y(y) {}
    Vec2 operator+(const Vec2& o) const { return {x+o.x, y+o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x-o.x, y-o.y}; }
    Vec2 operator*(float s) const { return {x*s, y*s}; }
    Vec2& operator+=(const Vec2& o) { x+=o.x; y+=o.y; return *this; }
};

// ---------------------------------------------------------------- Vec3 -----
struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3 operator*(float s)        const { return {x*s,   y*s,   z*s  }; }
    Vec3 operator/(float s)        const { float inv=1.f/s; return {x*inv,y*inv,z*inv}; }
    Vec3 operator-()               const { return {-x,-y,-z}; }
    Vec3& operator+=(const Vec3& o){ x+=o.x; y+=o.y; z+=o.z; return *this; }
    Vec3& operator-=(const Vec3& o){ x-=o.x; y-=o.y; z-=o.z; return *this; }
    Vec3& operator*=(float s)      { x*=s; y*=s; z*=s; return *this; }

    float dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }
    Vec3  cross(const Vec3& o) const {
        return { y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x };
    }
    float lengthSq() const { return x*x + y*y + z*z; }
    float length()   const { return std::sqrt(lengthSq()); }
    Vec3  normalized() const {
        float l = length();
        return (l > 1e-8f) ? (*this / l) : Vec3{0,0,0};
    }
    Vec3& normalize() { *this = normalized(); return *this; }

    static Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
        return a + (b - a) * t;
    }
    static Vec3 zero()    { return {0,0,0}; }
    static Vec3 one()     { return {1,1,1}; }
    static Vec3 up()      { return {0,1,0}; }
    static Vec3 forward() { return {0,0,-1}; }
    static Vec3 right()   { return {1,0,0}; }
};

// ---------------------------------------------------------------- Vec4 -----
struct Vec4 {
    float x=0, y=0, z=0, w=1;
    Vec4() = default;
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    Vec4(const Vec3& v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}
    Vec3 xyz() const { return {x,y,z}; }
};

// ---------------------------------------------------------------- Color ----
struct Color {
    float r=1, g=1, b=1, a=1;
    Color() = default;
    Color(float r,float g,float b,float a=1.f) : r(r),g(g),b(b),a(a) {}
    Color operator*(float s) const { return {r*s,g*s,b*s,a}; }
    Color operator+(const Color& o) const { return {r+o.r,g+o.g,b+o.b,a+o.a}; }
    static Color white()  { return {1,1,1,1}; }
    static Color black()  { return {0,0,0,1}; }
    static Color red()    { return {1,0,0,1}; }
    static Color green()  { return {0,1,0,1}; }
    static Color blue()   { return {0,0,1,1}; }
};

// --------------------------------------------------------------- Mat4 ------
struct Mat4 {
    float m[16]; // column-major: m[col*4 + row]
    Mat4() { for(int i=0;i<16;i++) m[i]=0.f; }

    static Mat4 identity() {
        Mat4 M;
        M.m[0]=M.m[5]=M.m[10]=M.m[15]=1.f;
        return M;
    }

    static Mat4 translation(const Vec3& t) {
        Mat4 M = identity();
        M.m[12]=t.x; M.m[13]=t.y; M.m[14]=t.z;
        return M;
    }

    static Mat4 scale(const Vec3& s) {
        Mat4 M = identity();
        M.m[0]=s.x; M.m[5]=s.y; M.m[10]=s.z;
        return M;
    }

    static Mat4 rotationX(float rad) {
        Mat4 M = identity();
        float c=std::cos(rad), s=std::sin(rad);
        M.m[5]=c; M.m[9]=-s; M.m[6]=s; M.m[10]=c;
        return M;
    }
    static Mat4 rotationY(float rad) {
        Mat4 M = identity();
        float c=std::cos(rad), s=std::sin(rad);
        M.m[0]=c; M.m[8]=s; M.m[2]=-s; M.m[10]=c;
        return M;
    }
    static Mat4 rotationZ(float rad) {
        Mat4 M = identity();
        float c=std::cos(rad), s=std::sin(rad);
        M.m[0]=c; M.m[4]=-s; M.m[1]=s; M.m[5]=c;
        return M;
    }

    Mat4 operator*(const Mat4& B) const {
        Mat4 C;
        for(int col=0;col<4;col++)
            for(int row=0;row<4;row++)
                for(int k=0;k<4;k++)
                    C.m[row+col*4] += m[row+k*4]*B.m[k+col*4];
        return C;
    }

    Vec4 operator*(const Vec4& v) const {
        return {
            m[0]*v.x + m[4]*v.y + m[ 8]*v.z + m[12]*v.w,
            m[1]*v.x + m[5]*v.y + m[ 9]*v.z + m[13]*v.w,
            m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]*v.w,
            m[3]*v.x + m[7]*v.y + m[11]*v.z + m[15]*v.w
        };
    }

    Vec3 transformPoint(const Vec3& p) const {
        Vec4 r = *this * Vec4(p,1.f);
        float iw = (std::abs(r.w) > 1e-8f) ? 1.f/r.w : 1.f;
        return r.xyz() * iw;
    }

    Vec3 transformDir(const Vec3& d) const {
        Vec4 r = *this * Vec4(d,0.f);
        return r.xyz();
    }

    // Perspective (OpenGL-style)
    static Mat4 perspective(float fovY_deg, float aspect, float zn, float zf) {
        float f = 1.f / std::tan(fovY_deg * (float)M_PI / 360.f);
        Mat4 M;
        M.m[ 0] = f / aspect;
        M.m[ 5] = f;
        M.m[10] = (zf+zn)/(zn-zf);
        M.m[11] = -1.f;
        M.m[14] = (2.f*zf*zn)/(zn-zf);
        return M;
    }

    // LookAt
    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        Vec3 z = (eye - center).normalized();
        Vec3 x = up.cross(z).normalized();
        Vec3 y = z.cross(x);
        Mat4 M;
        M.m[ 0]=x.x; M.m[ 1]=y.x; M.m[ 2]=z.x; M.m[ 3]=0;
        M.m[ 4]=x.y; M.m[ 5]=y.y; M.m[ 6]=z.y; M.m[ 7]=0;
        M.m[ 8]=x.z; M.m[ 9]=y.z; M.m[10]=z.z; M.m[11]=0;
        M.m[12]=-x.dot(eye); M.m[13]=-y.dot(eye); M.m[14]=-z.dot(eye); M.m[15]=1;
        return M;
    }

    const float* ptr() const { return m; }
};

// --------------------------------------------------------- Quaternion ------
struct Quaternion {
    float x=0, y=0, z=0, w=1;
    Quaternion() = default;
    Quaternion(float x,float y,float z,float w) : x(x),y(y),z(z),w(w) {}

    static Quaternion identity() { return {0,0,0,1}; }

    static Quaternion fromAxisAngle(const Vec3& axis, float rad) {
        float s = std::sin(rad*0.5f);
        Vec3  a = axis.normalized();
        return { a.x*s, a.y*s, a.z*s, std::cos(rad*0.5f) };
    }

    // Euler angles (radians) – applied YXZ order
    static Quaternion fromEuler(float pitch, float yaw, float roll) {
        Quaternion qx = fromAxisAngle({1,0,0}, pitch);
        Quaternion qy = fromAxisAngle({0,1,0}, yaw);
        Quaternion qz = fromAxisAngle({0,0,1}, roll);
        return qy * qx * qz;
    }

    Quaternion operator*(const Quaternion& o) const {
        return {
            w*o.x + x*o.w + y*o.z - z*o.y,
            w*o.y - x*o.z + y*o.w + z*o.x,
            w*o.z + x*o.y - y*o.x + z*o.w,
            w*o.w - x*o.x - y*o.y - z*o.z
        };
    }

    float norm() const { return std::sqrt(x*x+y*y+z*z+w*w); }
    Quaternion normalized() const {
        float n = norm();
        return (n>1e-8f) ? Quaternion{x/n,y/n,z/n,w/n} : identity();
    }
    Quaternion conjugate() const { return {-x,-y,-z,w}; }

    Vec3 rotate(const Vec3& v) const {
        // q * (0,v) * q^-1
        Quaternion qv{v.x,v.y,v.z,0};
        Quaternion r = (*this) * qv * conjugate();
        return {r.x,r.y,r.z};
    }

    Mat4 toMat4() const {
        float xx=x*x, yy=y*y, zz=z*z;
        float xy=x*y, xz=x*z, yz=y*z;
        float wx=w*x, wy=w*y, wz=w*z;
        Mat4 M = Mat4::identity();
        M.m[ 0]=1-2*(yy+zz); M.m[ 4]=2*(xy-wz);   M.m[ 8]=2*(xz+wy);
        M.m[ 1]=2*(xy+wz);   M.m[ 5]=1-2*(xx+zz); M.m[ 9]=2*(yz-wx);
        M.m[ 2]=2*(xz-wy);   M.m[ 6]=2*(yz+wx);   M.m[10]=1-2*(xx+yy);
        return M;
    }

    static Quaternion slerp(const Quaternion& a, const Quaternion& b, float t) {
        float dot = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
        Quaternion bb = b;
        if(dot < 0.f) { bb = {-b.x,-b.y,-b.z,-b.w}; dot=-dot; }
        if(dot > 0.9995f) {
            return Quaternion{ a.x+(bb.x-a.x)*t, a.y+(bb.y-a.y)*t,
                               a.z+(bb.z-a.z)*t, a.w+(bb.w-a.w)*t }.normalized();
        }
        float theta0 = std::acos(dot);
        float theta  = theta0 * t;
        float s0 = std::cos(theta) - dot*std::sin(theta)/std::sin(theta0);
        float s1 = std::sin(theta) / std::sin(theta0);
        return Quaternion{
            s0*a.x + s1*bb.x, s0*a.y + s1*bb.y,
            s0*a.z + s1*bb.z, s0*a.w + s1*bb.w
        }.normalized();
    }
};

// ---------------------------------------------------------------- AABB -----
struct AABB {
    Vec3 min{0,0,0}, max{0,0,0};
    AABB() = default;
    AABB(const Vec3& mn, const Vec3& mx) : min(mn), max(mx) {}

    bool intersects(const AABB& o) const {
        return !(max.x<o.min.x || min.x>o.max.x ||
                 max.y<o.min.y || min.y>o.max.y ||
                 max.z<o.min.z || min.z>o.max.z);
    }

    Vec3 center()  const { return (min+max)*0.5f; }
    Vec3 extents() const { return (max-min)*0.5f; }

    // Returns MTV (minimum translation vector) to resolve overlap
    // Returns zero if no overlap
    Vec3 resolve(const AABB& other) const {
        if(!intersects(other)) return Vec3::zero();
        Vec3 c1 = center(), c2 = other.center();
        Vec3 e1 = extents(), e2 = other.extents();
        Vec3 diff = c2 - c1;
        float ox = (e1.x+e2.x) - std::abs(diff.x);
        float oy = (e1.y+e2.y) - std::abs(diff.y);
        float oz = (e1.z+e2.z) - std::abs(diff.z);
        if(ox < oy && ox < oz)
            return Vec3{ (diff.x<0?-1.f:1.f)*ox, 0, 0 };
        else if(oy < oz)
            return Vec3{ 0, (diff.y<0?-1.f:1.f)*oy, 0 };
        else
            return Vec3{ 0, 0, (diff.z<0?-1.f:1.f)*oz };
    }

    // Expand AABB to include point
    void expand(const Vec3& p) {
        min.x=std::min(min.x,p.x); min.y=std::min(min.y,p.y); min.z=std::min(min.z,p.z);
        max.x=std::max(max.x,p.x); max.y=std::max(max.y,p.y); max.z=std::max(max.z,p.z);
    }
};

// --------------------------------------------------------- Frustum ----------
struct Plane {
    Vec3  normal;
    float d; // ax+by+cz+d=0
    float distance(const Vec3& p) const { return normal.dot(p) + d; }
};

struct Frustum {
    Plane planes[6]; // left,right,bottom,top,near,far

    // Build from combined VP matrix
    static Frustum fromMatrix(const Mat4& vp) {
        Frustum f;
        auto& m = vp.m;
        // Left
        f.planes[0].normal = { m[3]+m[0], m[7]+m[4], m[11]+m[8] };
        f.planes[0].d      =   m[15]+m[12];
        // Right
        f.planes[1].normal = { m[3]-m[0], m[7]-m[4], m[11]-m[8] };
        f.planes[1].d      =   m[15]-m[12];
        // Bottom
        f.planes[2].normal = { m[3]+m[1], m[7]+m[5], m[11]+m[9] };
        f.planes[2].d      =   m[15]+m[13];
        // Top
        f.planes[3].normal = { m[3]-m[1], m[7]-m[5], m[11]-m[9] };
        f.planes[3].d      =   m[15]-m[13];
        // Near
        f.planes[4].normal = { m[3]+m[2], m[7]+m[6], m[11]+m[10] };
        f.planes[4].d      =   m[15]+m[14];
        // Far
        f.planes[5].normal = { m[3]-m[2], m[7]-m[6], m[11]-m[10] };
        f.planes[5].d      =   m[15]-m[14];
        // Normalize
        for(auto& p : f.planes) {
            float l = p.normal.length();
            if(l > 1e-8f) { p.normal = p.normal * (1.f/l); p.d /= l; }
        }
        return f;
    }

    // Test AABB — true if visible (intersects or inside frustum)
    bool testAABB(const AABB& box) const {
        for(const auto& plane : planes) {
            Vec3 pv{
                plane.normal.x > 0 ? box.max.x : box.min.x,
                plane.normal.y > 0 ? box.max.y : box.min.y,
                plane.normal.z > 0 ? box.max.z : box.min.z
            };
            if(plane.distance(pv) < 0.f) return false;
        }
        return true;
    }
};

} // namespace AER
#endif // AER_MATH_H