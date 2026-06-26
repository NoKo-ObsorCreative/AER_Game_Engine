#pragma once
// ============================================================
// AER_Renderer.h  –  Software rasterizer
// Supports: per-pixel Blinn-Phong lighting (ambient+diffuse+specular),
//           smooth normal interpolation, full 6-plane frustum clipping,
//           perspective-correct UV, bilinear texture sampling,
//           alpha blending, font atlas HUD text.
// AER Game Engine  |  Alpha 0.3
// ============================================================
#ifndef AER_RENDERER_H
#define AER_RENDERER_H

#include "AER_Math.h"
#include "AER_Assets.h"
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <cmath>

namespace AER {

// ============================================================
// Internal clip-space vertex
// ============================================================
struct ClipVert {
    float cx,cy,cz,cw;    // clip space
    float vx,vy,vz;       // view space position (for lighting)
    float nx,ny,nz;       // view space normal
    float u,v;            // texture coords (pre-divided by w for perspective)
    Color color;
};

static ClipVert lerpCV(const ClipVert& a, const ClipVert& b, float t) {
    auto lf = [t](float a,float b){ return a+(b-a)*t; };
    auto lc = [&](const Color& a,const Color& b) -> Color {
        return {lf(a.r,b.r),lf(a.g,b.g),lf(a.b,b.b),lf(a.a,b.a)};
    };
    ClipVert r;
    r.cx=lf(a.cx,b.cx); r.cy=lf(a.cy,b.cy);
    r.cz=lf(a.cz,b.cz); r.cw=lf(a.cw,b.cw);
    r.vx=lf(a.vx,b.vx); r.vy=lf(a.vy,b.vy); r.vz=lf(a.vz,b.vz);
    r.nx=lf(a.nx,b.nx); r.ny=lf(a.ny,b.ny); r.nz=lf(a.nz,b.nz);
    r.u=lf(a.u,b.u);    r.v=lf(a.v,b.v);
    r.color=lc(a.color,b.color);
    return r;
}

// Clip polygon against a single plane: distance(v) = dot(n,pos)+d >= 0
// plane index encodes which clip plane (0-5 = l,r,b,t,n,f in clip space)
static std::vector<ClipVert> clipAgainstPlane(
    const std::vector<ClipVert>& poly, int plane)
{
    // Signed distance in clip space for each of 6 planes:
    // 0: w+x>=0  1: w-x>=0  2: w+y>=0  3: w-y>=0  4: w+z>=0  5: w-z>=0
    auto dist = [plane](const ClipVert& v) -> float {
        switch(plane){
        case 0: return v.cw+v.cx;
        case 1: return v.cw-v.cx;
        case 2: return v.cw+v.cy;
        case 3: return v.cw-v.cy;
        case 4: return v.cw+v.cz;
        case 5: return v.cw-v.cz;
        }
        return 0;
    };

    std::vector<ClipVert> out;
    if(poly.empty()) return out;
    size_t n = poly.size();
    for(size_t i=0; i<n; i++) {
        const ClipVert& cur  = poly[i];
        const ClipVert& next = poly[(i+1)%n];
        float dc = dist(cur), dn = dist(next);
        if(dc >= 0.f) out.push_back(cur);
        if((dc >= 0.f) != (dn >= 0.f)) {
            float t = dc / (dc - dn);
            out.push_back(lerpCV(cur, next, t));
        }
    }
    return out;
}

static std::vector<ClipVert> clipAllPlanes(std::vector<ClipVert> poly) {
    for(int p=0; p<6 && !poly.empty(); p++)
        poly = clipAgainstPlane(poly, p);
    return poly;
}

// ============================================================
// Renderer
// ============================================================
class Renderer {
public:
    // Lighting parameters
    Vec3  lightDir      {0.577f, 0.577f, 0.577f}; // world-space
    Color lightColor    {1,1,1,1};
    float ambientStr    = 0.15f;
    float diffuseStr    = 0.85f;
    float specularStr   = 0.5f;
    float shininess     = 32.f;
    Vec3  viewPos       {0,0,0}; // camera world pos (for specular)

    bool  wireframe     = false;
    bool  blendEnabled  = false;

    void init(int w, int h) {
        m_w = w; m_h = h;
        m_color.assign(w*h*4, 0.f);
        m_depth.assign(w*h, 1.f);
        m_vp_x=0; m_vp_y=0; m_vp_w=w; m_vp_h=h;
    }

    void setViewport(int x,int y,int w,int h) { m_vp_x=x; m_vp_y=y; m_vp_w=w; m_vp_h=h; }

    void setClearColor(Color c) { m_clearColor=c; }

    void clear() {
        for(int i=0;i<m_w*m_h;i++){
            m_color[i*4+0]=m_clearColor.r;
            m_color[i*4+1]=m_clearColor.g;
            m_color[i*4+2]=m_clearColor.b;
            m_color[i*4+3]=m_clearColor.a;
            m_depth[i]=1.f;
        }
    }

    // Set MVP matrices
    void setMatrices(const Mat4& model, const Mat4& view, const Mat4& proj) {
        m_model=model; m_view=view; m_proj=proj;
        m_mv   = m_view*m_model;
        m_mvp  = m_proj*m_mv;
    }

    // Set current texture (nullptr = vertex colors)
    void setTexture(const Texture* tex) { m_tex=tex; }

    // Draw a mesh (indexed or not)
    void drawMesh(const Mesh& mesh) {
        auto doTri = [&](uint32_t i0, uint32_t i1, uint32_t i2){
            submitTriangle(mesh.vertices[i0],
                           mesh.vertices[i1],
                           mesh.vertices[i2]);
        };
        if(mesh.indices.empty()) {
            for(size_t i=0; i+2<mesh.vertices.size(); i+=3) doTri(i,i+1,i+2);
        } else {
            for(size_t i=0; i+2<mesh.indices.size(); i+=3)
                doTri(mesh.indices[i],mesh.indices[i+1],mesh.indices[i+2]);
        }
    }

    // Draw HUD text at pixel position (x,y) – no depth test
    void drawText(const std::string& text, int x, int y, Color col, int scale=1) {
        int cx = x;
        for(char c : text) {
            if(c == '\n') { y += FontAtlas::GlyphH*scale; cx=x; continue; }
            drawGlyph(c, cx, y, col, scale);
            cx += FontAtlas::GlyphW * scale;
        }
    }

    // Write color buffer as PPM
    void flushPPM(const std::string& filename) {
        std::ofstream out(filename, std::ios::binary);
        if(!out) return;
        out << "P6\n" << m_w << " " << m_h << "\n255\n";
        for(int py=m_h-1; py>=0; py--)
            for(int px=0; px<m_w; px++){
                int i=(py*m_w+px)*4;
                auto clamp8=[](float v){ return (unsigned char)(std::min(v,1.f)*255); };
                out.put(clamp8(m_color[i+0]));
                out.put(clamp8(m_color[i+1]));
                out.put(clamp8(m_color[i+2]));
            }
    }

    // SDL surface upload (call after present)
    // Pass SDL_Surface* as void* to avoid SDL include requirement
    void uploadToSDLSurface(void* sdlSurface);

    int width()  const { return m_w; }
    int height() const { return m_h; }
    const std::vector<float>& colorBuffer() const { return m_color; }

private:
    int m_w=0, m_h=0;
    int m_vp_x=0, m_vp_y=0, m_vp_w=0, m_vp_h=0;
    Color m_clearColor{0.05f,0.05f,0.1f,1.f};
    std::vector<float> m_color;
    std::vector<float> m_depth;
    Mat4 m_model, m_view, m_proj, m_mv, m_mvp;
    const Texture* m_tex = nullptr;

    // ---- Pixel write ---
    void writePixel(int px,int py, float r,float g,float b,float a, float z) {
        if(px<0||px>=m_w||py<0||py>=m_h) return;
        int idx = py*m_w+px;
        if(z >= m_depth[idx]) return;
        if(!blendEnabled || a>=1.f){
            m_depth[idx]=z;
            m_color[idx*4+0]=r; m_color[idx*4+1]=g;
            m_color[idx*4+2]=b; m_color[idx*4+3]=a;
        } else {
            float oa=1.f-a;
            m_color[idx*4+0]=r*a+m_color[idx*4+0]*oa;
            m_color[idx*4+1]=g*a+m_color[idx*4+1]*oa;
            m_color[idx*4+2]=b*a+m_color[idx*4+2]*oa;
            m_color[idx*4+3]=a  +m_color[idx*4+3]*oa;
        }
    }

    // ---- NDC → window ---
    struct WinVert { float wx,wy,wz; };
    WinVert toWindow(float nx,float ny,float nz) const {
        return {
            m_vp_x + (nx*0.5f+0.5f)*m_vp_w,
            m_vp_y + (ny*0.5f+0.5f)*m_vp_h,
            nz*0.5f+0.5f
        };
    }

    // ---- Transform MeshVertex to ClipVert ---
    ClipVert transformVertex(const MeshVertex& mv) {
        // Model → view space
        Vec4 vs = m_mv * Vec4(mv.position,1.f);
        // Normal (use MV inverse-transpose – for uniform scale, MV suffices)
        Vec4 vn = m_mv * Vec4(mv.normal,0.f);
        // Clip space
        Vec4 cs = m_proj * vs;

        ClipVert cv;
        cv.cx=cs.x; cv.cy=cs.y; cv.cz=cs.z; cv.cw=cs.w;
        cv.vx=vs.x; cv.vy=vs.y; cv.vz=vs.z;
        cv.nx=vn.x; cv.ny=vn.y; cv.nz=vn.z;
        cv.u=mv.uv.x; cv.v=mv.uv.y;
        cv.color=mv.color;
        return cv;
    }

    void submitTriangle(const MeshVertex& a, const MeshVertex& b, const MeshVertex& c) {
        std::vector<ClipVert> poly = {
            transformVertex(a), transformVertex(b), transformVertex(c)
        };
        poly = clipAllPlanes(poly);
        if(poly.size()<3) return;

        // Perspective divide + fan triangulation
        for(size_t i=1; i+1<poly.size(); i++)
            rasterize(poly[0], poly[i], poly[i+1]);
    }

    // ---- Blinn-Phong shading in view space ---
    Color shade(float vx,float vy,float vz, float nx,float ny,float nz, Color texColor) {
        Vec3 N = Vec3{nx,ny,nz}.normalized();
        // Transform lightDir to view space (assume view matrix stored)
        Vec4 ld4 = m_view * Vec4{lightDir.x,lightDir.y,lightDir.z,0.f};
        Vec3 L = Vec3{ld4.x,ld4.y,ld4.z}.normalized();
        // View-space camera is at origin
        Vec3 V = (-Vec3{vx,vy,vz}).normalized();
        Vec3 H = (L+V).normalized();

        float diff = std::max(0.f, N.dot(L));
        float spec = std::pow(std::max(0.f, N.dot(H)), shininess);

        float ka = ambientStr, kd = diffuseStr * diff, ks = specularStr * spec;

        return Color{
            std::min(1.f, texColor.r*(ka+kd) + lightColor.r*ks),
            std::min(1.f, texColor.g*(ka+kd) + lightColor.g*ks),
            std::min(1.f, texColor.b*(ka+kd) + lightColor.b*ks),
            texColor.a
        };
    }

    // ---- Rasterize one clip-space triangle ---
    void rasterize(const ClipVert& cv0, const ClipVert& cv1, const ClipVert& cv2) {
        // Perspective divide
        auto pdiv = [](const ClipVert& cv) -> std::tuple<float,float,float,float> {
            float iw = (std::abs(cv.cw) > 1e-9f) ? 1.f/cv.cw : 0.f;
            return {cv.cx*iw, cv.cy*iw, cv.cz*iw, iw};
        };
        auto [x0,y0,z0,iw0] = pdiv(cv0);
        auto [x1,y1,z1,iw1] = pdiv(cv1);
        auto [x2,y2,z2,iw2] = pdiv(cv2);

        auto [wx0,wy0,wd0] = std::tuple<float,float,float>(toWindow(x0,y0,z0).wx, toWindow(x0,y0,z0).wy, toWindow(x0,y0,z0).wz);
        auto [wx1,wy1,wd1] = std::tuple<float,float,float>(toWindow(x1,y1,z1).wx, toWindow(x1,y1,z1).wy, toWindow(x1,y1,z1).wz);
        auto [wx2,wy2,wd2] = std::tuple<float,float,float>(toWindow(x2,y2,z2).wx, toWindow(x2,y2,z2).wy, toWindow(x2,y2,z2).wz);

        if(wireframe) {
            drawLine2D(wx0,wy0,wd0,cv0.color, wx1,wy1,wd1,cv1.color);
            drawLine2D(wx1,wy1,wd1,cv1.color, wx2,wy2,wd2,cv2.color);
            drawLine2D(wx2,wy2,wd2,cv2.color, wx0,wy0,wd0,cv0.color);
            return;
        }

        // AABB of triangle
        int ixMin = std::max(0,      (int)std::floor(std::min({wx0,wx1,wx2})));
        int ixMax = std::min(m_w-1,  (int)std::ceil (std::max({wx0,wx1,wx2})));
        int iyMin = std::max(0,      (int)std::floor(std::min({wy0,wy1,wy2})));
        int iyMax = std::min(m_h-1,  (int)std::ceil (std::max({wy0,wy1,wy2})));

        auto edge = [](float ax,float ay,float bx,float by,float px,float py) {
            return (px-ax)*(by-ay) - (py-ay)*(bx-ax);
        };

        float area = edge(wx0,wy0, wx1,wy1, wx2,wy2);
        if(std::abs(area) < 1e-6f) return;
        bool flip = area < 0.f;
        if(flip) area = -area;

        // Perspective-correct attribute storage (multiply by 1/w)
        // u,v,normals,viewpos,color premultiplied by iw
        auto pw = [](float val, float iw){ return val*iw; };

        float pu0=pw(cv0.u,iw0), pv0=pw(cv0.v,iw0);
        float pu1=pw(cv1.u,iw1), pv1=pw(cv1.v,iw1);
        float pu2=pw(cv2.u,iw2), pv2=pw(cv2.v,iw2);

        float pvx0=pw(cv0.vx,iw0), pvy0=pw(cv0.vy,iw0), pvz0=pw(cv0.vz,iw0);
        float pvx1=pw(cv1.vx,iw1), pvy1=pw(cv1.vy,iw1), pvz1=pw(cv1.vz,iw1);
        float pvx2=pw(cv2.vx,iw2), pvy2=pw(cv2.vy,iw2), pvz2=pw(cv2.vz,iw2);

        float pnx0=pw(cv0.nx,iw0), pny0=pw(cv0.ny,iw0), pnz0=pw(cv0.nz,iw0);
        float pnx1=pw(cv1.nx,iw1), pny1=pw(cv1.ny,iw1), pnz1=pw(cv1.nz,iw1);
        float pnx2=pw(cv2.nx,iw2), pny2=pw(cv2.ny,iw2), pnz2=pw(cv2.nz,iw2);

        float pr0=pw(cv0.color.r,iw0), pg0=pw(cv0.color.g,iw0), pb0=pw(cv0.color.b,iw0), pa0=pw(cv0.color.a,iw0);
        float pr1=pw(cv1.color.r,iw1), pg1=pw(cv1.color.g,iw1), pb1=pw(cv1.color.b,iw1), pa1=pw(cv1.color.a,iw1);
        float pr2=pw(cv2.color.r,iw2), pg2=pw(cv2.color.g,iw2), pb2=pw(cv2.color.b,iw2), pa2=pw(cv2.color.a,iw2);

        for(int py=iyMin; py<=iyMax; py++) {
            for(int px=ixMin; px<=ixMax; px++) {
                float pcx=px+.5f, pcy=py+.5f;
                float w0=edge(wx1,wy1,wx2,wy2,pcx,pcy);
                float w1=edge(wx2,wy2,wx0,wy0,pcx,pcy);
                float w2=edge(wx0,wy0,wx1,wy1,pcx,pcy);
                if(flip){w0=-w0;w1=-w1;w2=-w2;}
                if(w0<0||w1<0||w2<0) continue;
                w0/=area; w1/=area; w2/=area;

                float z   = w0*wd0 + w1*wd1 + w2*wd2;
                float piw = w0*iw0 + w1*iw1 + w2*iw2;
                float wc  = (std::abs(piw)>1e-12f) ? 1.f/piw : 0.f;

                // Recover perspective-correct attributes
                float u   = (w0*pu0+w1*pu1+w2*pu2)*wc;
                float v   = (w0*pv0+w1*pv1+w2*pv2)*wc;
                float vx  = (w0*pvx0+w1*pvx1+w2*pvx2)*wc;
                float vy  = (w0*pvy0+w1*pvy1+w2*pvy2)*wc;
                float vz  = (w0*pvz0+w1*pvz1+w2*pvz2)*wc;
                float nx  = (w0*pnx0+w1*pnx1+w2*pnx2)*wc;
                float ny  = (w0*pny0+w1*pny1+w2*pny2)*wc;
                float nz  = (w0*pnz0+w1*pnz1+w2*pnz2)*wc;
                float cr  = (w0*pr0+w1*pr1+w2*pr2)*wc;
                float cg  = (w0*pg0+w1*pg1+w2*pg2)*wc;
                float cb  = (w0*pb0+w1*pb1+w2*pb2)*wc;
                float ca  = (w0*pa0+w1*pa1+w2*pa2)*wc;

                Color texColor;
                if(m_tex && m_tex->valid())
                    texColor = m_tex->sample(u,v);
                else
                    texColor = {cr,cg,cb,ca};

                Color lit = shade(vx,vy,vz, nx,ny,nz, texColor);
                writePixel(px,py, lit.r,lit.g,lit.b,lit.a, z);
            }
        }
    }

    void drawLine2D(float x0,float y0,float d0,Color c0, float x1,float y1,float d1,Color c1) {
        int xi0=(int)std::round(x0), yi0=(int)std::round(y0);
        int xi1=(int)std::round(x1), yi1=(int)std::round(y1);
        int dx=std::abs(xi1-xi0), dy=std::abs(yi1-yi0);
        int sx=(xi0<xi1)?1:-1, sy=(yi0<yi1)?1:-1;
        int err=dx-dy, steps=std::max(dx,dy);
        for(int s=0;s<=steps;s++){
            float t=(steps>0)?(float)s/steps:0.f;
            float z=d0+(d1-d0)*t;
            float r=c0.r+(c1.r-c0.r)*t, g=c0.g+(c1.g-c0.g)*t,
                  b=c0.b+(c1.b-c0.b)*t, a=c0.a+(c1.a-c0.a)*t;
            writePixel(xi0,yi0,r,g,b,a,z);
            int e2=2*err;
            if(e2>-dy){err-=dy;xi0+=sx;}
            if(e2< dx){err+=dx;yi0+=sy;}
        }
    }

    void drawGlyph(char c, int x, int y, Color col, int scale) {
        for(int gy=0; gy<FontAtlas::GlyphH; gy++) {
            for(int gx=0; gx<FontAtlas::GlyphW; gx++) {
                if(!FontAtlas::pixel(c,gx,gy)) continue;
                for(int sy=0; sy<scale; sy++)
                    for(int sx=0; sx<scale; sx++) {
                        int px2 = x + gx*scale + sx;
                        int py2 = y + gy*scale + sy;
                        // HUD: write without depth test
                        if(px2<0||px2>=m_w||py2<0||py2>=m_h) continue;
                        int idx=py2*m_w+px2;
                        m_color[idx*4+0]=col.r;
                        m_color[idx*4+1]=col.g;
                        m_color[idx*4+2]=col.b;
                        m_color[idx*4+3]=col.a;
                    }
            }
        }
    }
};

} // namespace AER
#endif // AER_RENDERER_H