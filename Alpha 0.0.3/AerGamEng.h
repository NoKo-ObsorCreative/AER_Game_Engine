#ifndef AER_H
#define AER_H
// ============================================================
// AER - A simple software rasterizer
// Alpha 0.2 + Interactive Extensions (Game Engine Foundations)
// 
// Changes from 0.2:
// - Added basic Entity / Transform system (Scene Graph foundation)
// - Added simple delta-time tracking (for real-time game loop)
// - Added basic AABB collision helpers
// - Added placeholder for windowing/input (SDL2/GLFW style API)
// - Added simple .obj loader stub (Asset Pipeline)
// - Added basic audio stub and UI text drawing placeholder
// - Kept core rasterizer intact with all previous fixes
// ============================================================

#include <vector>
#include <string>
#include <fstream>
#include <cmath>
#include <limits>
#include <algorithm>
#include <tuple>
#include <iostream>
#include <array>
#include <chrono>   // for delta time
#include <memory>   // for entity pointers if needed

namespace AER {

// ------------------------------------------------------------------ types ---
struct Color {
    float r, g, b, a;
};

// Vertex carrying NDC position, view-space position, UVs, color, and 1/w
struct Vertex {
    float x, y, z; // NDC after projection + perspective divide
    float vx, vy, vz; // view-space position (for lighting)
    float u, v; // texture coordinates [0..1]
    Color color;
    float invW; // 1/clip_w (for perspective-correct interpolation)
};

enum PrimitiveType { AER_POINTS, AER_LINES, AER_TRIANGLES };

// ----------------------------------------------------------------- Mat4 ----
struct Mat4 { float m[16]; }; // column-major

static Mat4 mat4Identity() {
    Mat4 M{};
    for(int i = 0; i < 16; i++) M.m[i] = 0.0f;
    M.m[0] = M.m[5] = M.m[10] = M.m[15] = 1.0f;
    return M;
}

static Mat4 MatMul(const Mat4& A, const Mat4& B) {
    Mat4 C{};
    for(int col = 0; col < 4; col++)
        for(int row = 0; row < 4; row++)
            for(int k = 0; k < 4; k++)
                C.m[row + col*4] += A.m[row + k*4] * B.m[k + col*4];
    return C;
}

static std::tuple<float,float,float,float>
MatVecMul(const Mat4& M, float x, float y, float z, float w) {
    float rx = M.m[0]*x + M.m[4]*y + M.m[ 8]*z + M.m[12]*w;
    float ry = M.m[1]*x + M.m[5]*y + M.m[ 9]*z + M.m[13]*w;
    float rz = M.m[2]*x + M.m[6]*y + M.m[10]*z + M.m[14]*w;
    float rw = M.m[3]*x + M.m[7]*y + M.m[11]*z + M.m[15]*w;
    return {rx, ry, rz, rw};
}

// ---------------------------------------------------------- internal state --
static int g_width = 0, g_height = 0;
static int g_viewport_x = 0, g_viewport_y = 0;
static int g_viewport_width = 0, g_viewport_height = 0;
static Color g_clearColor = {0, 0, 0, 1};
static std::vector<float> g_colorBuffer; // RGBA per pixel
static std::vector<float> g_depthBuffer; // depth per pixel
static std::vector<Vertex> g_vertices;
static PrimitiveType g_currentPrim = AER_TRIANGLES;
static Color g_currentColor = {1, 1, 1, 1};
static float g_currentU = 0, g_currentV = 0;
static Mat4 g_modelMatrix, g_viewMatrix, g_projMatrix;

struct Texture {
    int width = 0, height = 0;
    std::vector<Color> data;
};
static std::vector<Texture> g_textures;
static int g_currentTexture = -1;

// Directional light in view-space
static float g_lightDirX = 0, g_lightDirY = 0, g_lightDirZ = -1;
static float g_ambientStrength = 0.2f;
static float g_diffuseStrength = 0.8f;
static bool g_blendEnabled = false;

// --------------------------- NEW: Delta Time & Game Loop Helpers ------------
static auto g_lastFrameTime = std::chrono::high_resolution_clock::now();
static float g_deltaTime = 0.016f; // default ~60fps

float AER_GetDeltaTime() { return g_deltaTime; }

void AER_UpdateDeltaTime() {
    auto now = std::chrono::high_resolution_clock::now();
    g_deltaTime = std::chrono::duration<float>(now - g_lastFrameTime).count();
    g_lastFrameTime = now;
}

// --------------------------- NEW: Entity / Transform (Scene Graph) ----------
struct Vec3 { float x=0, y=0, z=0; };
struct Quaternion { float x=0, y=0, z=0, w=1; }; // simple, can be expanded

struct Transform {
    Vec3 position{0,0,0};
    Quaternion rotation{0,0,0,1};
    Vec3 scale{1,1,1};
    Mat4 GetModelMatrix() const {
        // Very basic: translate * rotate * scale (placeholder - improve with proper quat->mat)
        Mat4 T = mat4Identity();
        T.m[12] = position.x; T.m[13] = position.y; T.m[14] = position.z;
        
        Mat4 S = mat4Identity();
        S.m[0] = scale.x; S.m[5] = scale.y; S.m[10] = scale.z;
        
        return MatMul(T, S); // TODO: add proper rotation matrix
    }
};

struct Mesh {
    std::vector<float> vertices;  // x,y,z, u,v, r,g,b,a  (interleaved example)
    int textureId = -1;
};

struct Entity {
    Transform transform;
    std::shared_ptr<Mesh> mesh;
    // Can add more components later (physics, script, etc.)
};

static std::vector<Entity> g_sceneEntities;

// Draw all entities in scene (call this in your game loop)
void AER_DrawScene() {
    for(auto& entity : g_sceneEntities) {
        AER_SetModelMatrix(entity.transform.GetModelMatrix().m); // assumes public access or setter
        // TODO: bind texture, then emit vertices from mesh
        // For now, placeholder - user should implement mesh rendering
    }
}

// --------------------------- NEW: Simple Collision (AABB) -------------------
struct AABB {
    Vec3 min, max;
};

bool AER_AABBIntersect(const AABB& a, const AABB& b) {
    return !(a.max.x < b.min.x || a.min.x > b.max.x ||
             a.max.y < b.min.y || a.min.y > b.max.y ||
             a.max.z < b.min.z || a.min.z > b.max.z);
}

Vec3 AER_ResolveCollision(const AABB& player, const AABB& obstacle, Vec3 velocity) {
    // Simple push-back (expand as needed)
    return velocity; // placeholder
}

// --------------------------- NEW: Asset Loading Stub (.obj) ----------------
Mesh AER_LoadOBJ(const std::string& filename) {
    Mesh mesh;
    std::ifstream file(filename);
    if(!file) { std::cerr << "Failed to load OBJ: " << filename << "\n"; return mesh; }
    // Very basic stub - expand with full parser (or integrate tinyobjloader)
    std::string line;
    while(std::getline(file, line)) {
        // Parse v, vt, vn, f etc. - left as exercise
    }
    std::cout << "Loaded OBJ stub: " << filename << "\n";
    return mesh;
}

// --------------------------- NEW: Windowing & Input Placeholder ------------
/* 
   In a real implementation you would #include <SDL2/SDL.h> or GLFW and 
   implement these. Here we provide a clean API you can hook into.
*/
void AER_CreateWindow(const char* title, int w, int h) {
    // SDL_Init / glfwInit placeholder
    std::cout << "AER: Window created (stub) - " << title << " " << w << "x" << h << "\n";
    AER_Init(w, h);
}

bool AER_PollEvents() {
    // Return false to quit
    // Implement keyboard/mouse here (WASD, mouse look, etc.)
    return true;
}

void AER_SwapBuffers() {
    // In real version: upload g_colorBuffer to texture and present
    std::cout << "AER: Frame swapped (stub)\n";
}

// --------------------------- NEW: Audio Stub --------------------------------
void AER_PlaySound(const std::string& soundFile, bool loop = false) {
    std::cout << "AER: Playing sound (stub): " << soundFile << "\n";
}

// --------------------------- NEW: Simple UI Text (2D overlay) --------------
void AER_DrawText(const std::string& text, int x, int y, Color col) {
    // Placeholder: in real version rasterize font atlas or use library
    std::cout << "AER: Draw text '" << text << "' at (" << x << "," << y << ")\n";
}

// ------------------------------------------------------------------- init ---
void AER_Init(int width, int height) {
    g_width = width;
    g_height = height;
    g_viewport_x = 0; g_viewport_y = 0;
    g_viewport_width = width; g_viewport_height = height;
    g_colorBuffer.assign(width * height * 4, 0.0f);
    g_depthBuffer.assign(width * height, 1.0f);
    g_modelMatrix = mat4Identity();
    g_viewMatrix = mat4Identity();
    g_projMatrix = mat4Identity();
}

void AER_ClearColor(float r, float g, float b, float a) {
    g_clearColor = {r, g, b, a};
}

void AER_Clear() {
    for(int i = 0; i < g_width * g_height; i++) {
        g_colorBuffer[i*4+0] = g_clearColor.r;
        g_colorBuffer[i*4+1] = g_clearColor.g;
        g_colorBuffer[i*4+2] = g_clearColor.b;
        g_colorBuffer[i*4+3] = g_clearColor.a;
        g_depthBuffer[i] = 1.0f;
    }
}

void AER_Viewport(int x, int y, int width, int height) {
    g_viewport_x = x; g_viewport_y = y;
    g_viewport_width = width; g_viewport_height = height;
}

// ---------------------------------------------------------- matrix setters --
void AER_SetModelMatrix(const float* m16) {
    for(int i = 0; i < 16; i++) g_modelMatrix.m[i] = m16[i];
}

void AER_ResetModelMatrix() {
    g_modelMatrix = mat4Identity();
}

void AER_SetPerspective(float fovY_deg, float aspect, float znear, float zfar) {
    float fovY = fovY_deg * (float)M_PI / 180.0f;
    float f = 1.0f / std::tan(fovY * 0.5f);
    for(int i = 0; i < 16; i++) g_projMatrix.m[i] = 0.0f;
    g_projMatrix.m[0] = f / aspect;
    g_projMatrix.m[5] = f;
    g_projMatrix.m[10] = (zfar + znear) / (znear - zfar);
    g_projMatrix.m[11] = -1.0f;
    g_projMatrix.m[14] = (2.0f * zfar * znear) / (znear - zfar);
}

void AER_SetLookAt(float ex, float ey, float ez,
                   float cx, float cy, float cz,
                   float ux, float uy, float uz) {
    float zx = ex-cx, zy = ey-cy, zz = ez-cz;
    float zl = std::sqrt(zx*zx + zy*zy + zz*zz);
    if(zl > 1e-6f) { zx /= zl; zy /= zl; zz /= zl; }
    float rx = uy*zz - uz*zy, ry = uz*zx - ux*zz, rz = ux*zy - uy*zx;
    float rl = std::sqrt(rx*rx + ry*ry + rz*rz);
    if(rl > 1e-6f) { rx /= rl; ry /= rl; rz /= rl; }
    float ux2 = zy*rz - zz*ry, uy2 = zz*rx - zx*rz, uz2 = zx*ry - zy*rx;
    
    for(int i = 0; i < 16; i++) g_viewMatrix.m[i] = 0.0f;
    g_viewMatrix.m[0] = rx; g_viewMatrix.m[1] = ux2; g_viewMatrix.m[2] = zx;
    g_viewMatrix.m[4] = ry; g_viewMatrix.m[5] = uy2; g_viewMatrix.m[6] = zy;
    g_viewMatrix.m[8] = rz; g_viewMatrix.m[9] = uz2; g_viewMatrix.m[10] = zz;
    g_viewMatrix.m[12] = -(rx*ex + ry*ey + rz*ez);
    g_viewMatrix.m[13] = -(ux2*ex + uy2*ey + uz2*ez);
    g_viewMatrix.m[14] = -(zx*ex + zy*ey + zz*ez);
    g_viewMatrix.m[15] = 1.0f;
}

// ----------------------------------------------------------- light config ---
void AER_SetLightDir(float x, float y, float z) {
    float l = std::sqrt(x*x + y*y + z*z);
    if(l > 1e-6f) { g_lightDirX = x/l; g_lightDirY = y/l; g_lightDirZ = z/l; }
}

void AER_SetAmbient(float strength) { g_ambientStrength = std::max(0.0f, std::min(1.0f, strength)); }
void AER_SetDiffuse(float strength) { g_diffuseStrength = std::max(0.0f, std::min(1.0f, strength)); }

// ---------------------------------------------------------- alpha blending --
void AER_EnableBlend() { g_blendEnabled = true; }
void AER_DisableBlend() { g_blendEnabled = false; }

// ------------------------------------------------------------ textures ------
int AER_LoadTexture(const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);
    if(!in) { std::cerr << "AER: texture load failed: " << filename << "\n"; return -1; }
    std::string magic; in >> magic;
    if(magic != "P6") { std::cerr << "AER: only PPM P6 supported\n"; return -1; }
    int w, h, maxv; in >> w >> h >> maxv; in.get();
    Texture tex; tex.width = w; tex.height = h;
    tex.data.resize(w * h);
    for(int i = 0; i < w * h; i++) {
        unsigned char r = (unsigned char)in.get();
        unsigned char g = (unsigned char)in.get();
        unsigned char b = (unsigned char)in.get();
        tex.data[i] = { r/255.0f, g/255.0f, b/255.0f, 1.0f };
    }
    g_textures.push_back(tex);
    return (int)g_textures.size() - 1;
}

void AER_BindTexture(int id) {
    g_currentTexture = (id >= 0 && id < (int)g_textures.size()) ? id : -1;
}

// --------------------------------------------------------- immediate mode ---
void AER_Begin(PrimitiveType prim) {
    g_currentPrim = prim;
    g_vertices.clear();
}

void AER_Color3f(float r, float g, float b) { g_currentColor = {r, g, b, 1.0f}; }
void AER_Color4f(float r, float g, float b, float a) { g_currentColor = {r, g, b, a}; }
void AER_TexCoord2f(float u, float v) { g_currentU = u; g_currentV = v; }

void AER_Vertex3f(float x, float y, float z) {
    auto [mx, my, mz, mw] = MatVecMul(g_modelMatrix, x, y, z, 1.0f);
    auto [vx, vy, vz, vw] = MatVecMul(g_viewMatrix, mx, my, mz, mw);
    auto [cx, cy, cz, cw] = MatVecMul(g_projMatrix, vx, vy, vz, vw);
    Vertex v;
    v.vx = vx; v.vy = vy; v.vz = vz;
    v.u = g_currentU; v.v = g_currentV;
    v.color = g_currentColor;
    v.x = cx; v.y = cy; v.z = cz; v.invW = cw;
    g_vertices.push_back(v);
}

// ------------------------------------------------------- near-plane clip ----
static const float CLIP_EPS = 1e-5f;

struct ClipVertex {
    float cx, cy, cz, cw;
    float vx, vy, vz;
    float u, v;
    Color color;
};

static ClipVertex makeClipVertex(const Vertex& v) {
    return { v.x, v.y, v.z, v.invW, v.vx, v.vy, v.vz, v.u, v.v, v.color };
}

static ClipVertex lerpCV(const ClipVertex& a, const ClipVertex& b, float t) {
    auto lerp = [](float a, float b, float t){ return a + (b-a)*t; };
    ClipVertex r;
    r.cx = lerp(a.cx, b.cx, t); r.cy = lerp(a.cy, b.cy, t);
    r.cz = lerp(a.cz, b.cz, t); r.cw = lerp(a.cw, b.cw, t);
    r.vx = lerp(a.vx, b.vx, t); r.vy = lerp(a.vy, b.vy, t); r.vz = lerp(a.vz, b.vz, t);
    r.u = lerp(a.u, b.u, t); r.v = lerp(a.v, b.v, t);
    r.color.r = lerp(a.color.r, b.color.r, t);
    r.color.g = lerp(a.color.g, b.color.g, t);
    r.color.b = lerp(a.color.b, b.color.b, t);
    r.color.a = lerp(a.color.a, b.color.a, t);
    return r;
}

static std::vector<ClipVertex> clipNearPlane(const std::vector<ClipVertex>& poly) {
    std::vector<ClipVertex> out;
    if(poly.empty()) return out;
    for(size_t i = 0; i < poly.size(); i++) {
        const ClipVertex& cur = poly[i];
        const ClipVertex& next = poly[(i+1) % poly.size()];
        bool curIn = cur.cw > CLIP_EPS;
        bool nextIn = next.cw > CLIP_EPS;
        if(curIn) out.push_back(cur);
        if(curIn != nextIn) {
            float t = (cur.cw - CLIP_EPS) / (cur.cw - next.cw);
            out.push_back(lerpCV(cur, next, t));
        }
    }
    return out;
}

static Vertex clipToVertex(const ClipVertex& cv) {
    float invW = (std::abs(cv.cw) > CLIP_EPS) ? (1.0f / cv.cw) : 0.0f;
    Vertex v;
    v.x = cv.cx * invW; v.y = cv.cy * invW; v.z = cv.cz * invW;
    v.vx = cv.vx; v.vy = cv.vy; v.vz = cv.vz;
    v.u = cv.u * invW; v.v = cv.v * invW;
    v.invW = invW;
    v.color.r = cv.color.r * invW;
    v.color.g = cv.color.g * invW;
    v.color.b = cv.color.b * invW;
    v.color.a = cv.color.a * invW;
    return v;
}

// -------------------------------------------------------- window transform --
static std::tuple<float,float,float> toWin(const Vertex& v) {
    float xw = g_viewport_x + (v.x * 0.5f + 0.5f) * g_viewport_width;
    float yw = g_viewport_y + (v.y * 0.5f + 0.5f) * g_viewport_height;
    float d = v.z * 0.5f + 0.5f;
    return {xw, yw, d};
}

// -------------------------------------------------------- pixel write --------
static void writePixel(int px, int py, float r, float g, float b, float a, float z) {
    if(px < 0 || px >= g_width || py < 0 || py >= g_height) return;
    int idx = py * g_width + px;
    if(z >= g_depthBuffer[idx]) return;
    if(!g_blendEnabled || a >= 1.0f) {
        g_depthBuffer[idx] = z;
        g_colorBuffer[idx*4+0] = r;
        g_colorBuffer[idx*4+1] = g;
        g_colorBuffer[idx*4+2] = b;
        g_colorBuffer[idx*4+3] = a;
    } else {
        float dr = g_colorBuffer[idx*4+0], dg = g_colorBuffer[idx*4+1],
              db = g_colorBuffer[idx*4+2], da = g_colorBuffer[idx*4+3];
        float oa = 1.0f - a;
        g_colorBuffer[idx*4+0] = r * a + dr * oa;
        g_colorBuffer[idx*4+1] = g * a + dg * oa;
        g_colorBuffer[idx*4+2] = b * a + db * oa;
        g_colorBuffer[idx*4+3] = a + da * oa;
    }
}

// ------------------------------------------------------- rasterizers --------
static void rasterizeTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2) {
    auto [x0,y0,d0] = toWin(v0);
    auto [x1,y1,d1] = toWin(v1);
    auto [x2,y2,d2] = toWin(v2);

    int ix0 = std::max(0, (int)std::floor(std::min({x0,x1,x2})));
    int ix1 = std::min(g_width-1, (int)std::ceil(std::max({x0,x1,x2})));
    int iy0 = std::max(0, (int)std::floor(std::min({y0,y1,y2})));
    int iy1 = std::min(g_height-1, (int)std::ceil(std::max({y0,y1,y2})));

    auto edge = [](float ax,float ay,float bx,float by,float px,float py) -> float {
        return (px-ax)*(by-ay) - (py-ay)*(bx-ax);
    };

    float area = edge(x0,y0, x1,y1, x2,y2);
    if(std::abs(area) < 1e-6f) return;
    float signedArea = area;
    if(area < 0.0f) area = -area;

    // Flat lighting
    float ex1 = v1.vx-v0.vx, ey1 = v1.vy-v0.vy, ez1 = v1.vz-v0.vz;
    float ex2 = v2.vx-v0.vx, ey2 = v2.vy-v0.vy, ez2 = v2.vz-v0.vz;
    float nx = ey1*ez2 - ez1*ey2;
    float ny = ez1*ex2 - ex1*ez2;
    float nz = ex1*ey2 - ey1*ex2;
    float nlen = std::sqrt(nx*nx + ny*ny + nz*nz);
    float diffuse = 0.0f;
    if(nlen > 1e-6f) {
        nx /= nlen; ny /= nlen; nz /= nlen;
        float dot = nx*g_lightDirX + ny*g_lightDirY + nz*g_lightDirZ;
        diffuse = std::max(0.0f, dot);
    }
    float lightFactor = g_ambientStrength + g_diffuseStrength * diffuse;
    lightFactor = std::min(lightFactor, 1.0f);

    int texID = g_currentTexture;
    for(int py = iy0; py <= iy1; py++) {
        for(int px = ix0; px <= ix1; px++) {
            float pcx = px + 0.5f, pcy = py + 0.5f;
            float w0 = edge(x1,y1, x2,y2, pcx, pcy);
            float w1 = edge(x2,y2, x0,y0, pcx, pcy);
            float w2 = edge(x0,y0, x1,y1, pcx, pcy);
            if(signedArea < 0.0f) { w0=-w0; w1=-w1; w2=-w2; }
            if(w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
            w0 /= area; w1 /= area; w2 /= area;

            float z = w0*d0 + w1*d1 + w2*d2;
            float invW = w0*v0.invW + w1*v1.invW + w2*v2.invW;
            float wCorr = (std::abs(invW) > 1e-10f) ? (1.0f / invW) : 0.0f;

            float r, g, b, a;
            if(texID >= 0 && texID < (int)g_textures.size()) {
                float pu = (w0*v0.u + w1*v1.u + w2*v2.u) * wCorr;
                float pv = (w0*v0.v + w1*v1.v + w2*v2.v) * wCorr;
                pu = std::clamp(pu, 0.0f, 1.0f);
                pv = std::clamp(pv, 0.0f, 1.0f);
                const Texture& tex = g_textures[texID];
                int tx = std::min((int)(pu * (tex.width -1)), tex.width -1);
                int ty = std::min((int)(pv * (tex.height-1)), tex.height-1);
                Color tc = tex.data[ty * tex.width + tx];
                r = tc.r; g = tc.g; b = tc.b; a = tc.a;
            } else {
                r = (w0*v0.color.r + w1*v1.color.r + w2*v2.color.r) * wCorr;
                g = (w0*v0.color.g + w1*v1.color.g + w2*v2.color.g) * wCorr;
                b = (w0*v0.color.b + w1*v1.color.b + w2*v2.color.b) * wCorr;
                a = (w0*v0.color.a + w1*v1.color.a + w2*v2.color.a) * wCorr;
            }
            r *= lightFactor; g *= lightFactor; b *= lightFactor;
            writePixel(px, py, r, g, b, a, z);
        }
    }
}

static void rasterizeLine(const Vertex& v0, const Vertex& v1) {
    auto [x0f, y0f, d0] = toWin(v0);
    auto [x1f, y1f, d1] = toWin(v1);
    int x0 = (int)std::round(x0f), y0 = (int)std::round(y0f);
    int x1 = (int)std::round(x1f), y1 = (int)std::round(y1f);
    int dx = std::abs(x1-x0), dy = std::abs(y1-y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    int steps = std::max(dx, dy);
    for(int s = 0; s <= steps; s++) {
        float t = (steps > 0) ? (float)s / steps : 0.0f;
        float z = d0 + (d1-d0)*t;
        float r = v0.color.r + (v1.color.r - v0.color.r)*t;
        float g = v0.color.g + (v1.color.g - v0.color.g)*t;
        float b = v0.color.b + (v1.color.b - v0.color.b)*t;
        float a = v0.color.a + (v1.color.a - v0.color.a)*t;
        writePixel(x0, y0, r, g, b, a, z);
        int e2 = 2 * err;
        if(e2 > -dy) { err -= dy; x0 += sx; }
        if(e2 < dx) { err += dx; y0 += sy; }
    }
}

static void rasterizePoint(const Vertex& v) {
    auto [xf, yf, d] = toWin(v);
    int px = (int)std::round(xf);
    int py = (int)std::round(yf);
    writePixel(px, py, v.color.r, v.color.g, v.color.b, v.color.a, d);
}

// ------------------------------------------------------------ AER_End -------
void AER_End() {
    if(g_currentPrim == AER_POINTS) {
        for(auto& raw : g_vertices) {
            float invW = (std::abs(raw.invW) > CLIP_EPS) ? (1.0f / raw.invW) : 0.0f;
            Vertex v = raw;
            v.x = raw.x * invW; v.y = raw.y * invW; v.z = raw.z * invW;
            v.invW = invW;
            rasterizePoint(v);
        }
        return;
    }
    if(g_currentPrim == AER_LINES) {
        for(size_t i = 0; i + 1 < g_vertices.size(); i += 2) {
            std::vector<ClipVertex> seg = {makeClipVertex(g_vertices[i]), makeClipVertex(g_vertices[i+1])};
            seg = clipNearPlane(seg);
            if(seg.size() == 2) {
                Vertex a = clipToVertex(seg[0]);
                Vertex b = clipToVertex(seg[1]);
                rasterizeLine(a, b);
            }
        }
        return;
    }
    // AER_TRIANGLES
    for(size_t i = 0; i + 2 < g_vertices.size(); i += 3) {
        std::vector<ClipVertex> poly = {
            makeClipVertex(g_vertices[i]),
            makeClipVertex(g_vertices[i+1]),
            makeClipVertex(g_vertices[i+2])
        };
        poly = clipNearPlane(poly);
        if(poly.size() < 3) continue;
        Vertex v0 = clipToVertex(poly[0]);
        for(size_t j = 1; j + 1 < poly.size(); j++) {
            Vertex va = clipToVertex(poly[j]);
            Vertex vb = clipToVertex(poly[j+1]);
            rasterizeTriangle(v0, va, vb);
        }
    }
}

// Write color buffer to PPM (for debugging / offline)
void AER_Flush(const std::string& filename) {
    std::ofstream out(filename, std::ios::binary);
    if(!out) return;
    out << "P6\n" << g_width << " " << g_height << "\n255\n";
    for(int py = g_height-1; py >= 0; py--) {
        for(int px = 0; px < g_width; px++) {
            int idx = (py * g_width + px) * 4;
            out.put((unsigned char)(std::min(g_colorBuffer[idx+0], 1.0f) * 255));
            out.put((unsigned char)(std::min(g_colorBuffer[idx+1], 1.0f) * 255));
            out.put((unsigned char)(std::min(g_colorBuffer[idx+2], 1.0f) * 255));
        }
    }
}

} // namespace AER
#endif // AER_H