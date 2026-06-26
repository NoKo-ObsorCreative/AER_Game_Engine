#pragma once
// ============================================================
// AER_Assets.h  –  Mesh/OBJ loader, Texture, Font Atlas, Audio
// AER Game Engine  |  Alpha 0.3
// ============================================================
#ifndef AER_ASSETS_H
#define AER_ASSETS_H

#include "AER_Math.h"
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <algorithm>
#include <cstring>

namespace AER {

// ============================================================
// Texture  (PPM P6 or raw RGBA)
// ============================================================
enum class WrapMode   { Clamp, Repeat, Mirror };
enum class FilterMode { Nearest, Bilinear };

struct Texture {
    int    width  = 0;
    int    height = 0;
    std::vector<Color> data;   // [y * width + x]
    WrapMode   wrapU  = WrapMode::Repeat;
    WrapMode   wrapV  = WrapMode::Repeat;
    FilterMode filter = FilterMode::Bilinear;
    std::string path;

    bool valid() const { return width > 0 && height > 0 && !data.empty(); }

    static float applyWrap(float t, WrapMode mode) {
        switch(mode) {
        case WrapMode::Clamp:
            return std::clamp(t, 0.f, 1.f);
        case WrapMode::Repeat:
            t -= std::floor(t); return t < 0.f ? t+1.f : t;
        case WrapMode::Mirror: {
            int   it = (int)std::floor(t);
            float ft = t - it;
            return (it & 1) ? 1.f - ft : ft;
        }
        }
        return t;
    }

    Color sample(float u, float v) const {
        if(!valid()) return Color::white();
        u = applyWrap(u, wrapU);
        v = applyWrap(v, wrapV);

        if(filter == FilterMode::Nearest) {
            int tx = std::min((int)(u*(width -1)), width -1);
            int ty = std::min((int)(v*(height-1)), height-1);
            return data[ty*width+tx];
        }
        // Bilinear
        float fx = u*(width -1);
        float fy = v*(height-1);
        int x0 = std::max(0, std::min((int)fx,   width -1));
        int x1 = std::max(0, std::min(x0+1,       width -1));
        int y0 = std::max(0, std::min((int)fy,   height-1));
        int y1 = std::max(0, std::min(y0+1,       height-1));
        float tx = fx - x0, ty = fy - y0;
        Color c00 = data[y0*width+x0], c10 = data[y0*width+x1];
        Color c01 = data[y1*width+x0], c11 = data[y1*width+x1];
        auto lerp = [](Color a, Color b, float t) {
            return Color{a.r+(b.r-a.r)*t, a.g+(b.g-a.g)*t,
                         a.b+(b.b-a.b)*t, a.a+(b.a-a.a)*t};
        };
        return lerp(lerp(c00,c10,tx), lerp(c01,c11,tx), ty);
    }

    // Load PPM P6
    static Texture loadPPM(const std::string& path) {
        Texture tex; tex.path = path;
        std::ifstream in(path, std::ios::binary);
        if(!in) { std::cerr << "AER: texture not found: " << path << "\n"; return tex; }
        std::string magic; in >> magic;
        if(magic != "P6") { std::cerr << "AER: only PPM P6 supported\n"; return tex; }
        // Skip comments
        char c; in.get(c);
        while(c == '#') { std::string line; std::getline(in,line); in.get(c); }
        in.putback(c);
        int w,h,maxv; in >> w >> h >> maxv; in.get();
        tex.width=w; tex.height=h; tex.data.resize(w*h);
        float scale = 1.f / maxv;
        for(int i=0; i<w*h; i++) {
            unsigned char r=(unsigned char)in.get(), g=(unsigned char)in.get(), b=(unsigned char)in.get();
            tex.data[i] = { r*scale, g*scale, b*scale, 1.f };
        }
        return tex;
    }

    // Create a solid-color texture (useful as defaults)
    static Texture solid(int w, int h, Color col) {
        Texture t; t.width=w; t.height=h; t.data.assign(w*h, col);
        return t;
    }

    // Create a simple checkerboard
    static Texture checkerboard(int w=64, int h=64, Color c0={0,0,0,1}, Color c1={1,1,1,1}) {
        Texture t; t.width=w; t.height=h; t.data.resize(w*h);
        for(int y=0; y<h; y++)
            for(int x=0; x<w; x++)
                t.data[y*w+x] = ((x/8+y/8)&1) ? c1 : c0;
        return t;
    }
};

// ============================================================
// Vertex (interleaved: pos, normal, uv, color, tangent)
// ============================================================
struct MeshVertex {
    Vec3  position {0,0,0};
    Vec3  normal   {0,1,0};
    Vec2  uv       {0,0};
    Color color    {1,1,1,1};
    Vec3  tangent  {1,0,0};
};

// ============================================================
// Mesh
// ============================================================
struct Mesh {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t>   indices;   // if empty, use sequential
    int  textureID = -1;
    AABB bounds;
    std::string name;

    void computeBounds() {
        if(vertices.empty()) return;
        bounds = AABB{vertices[0].position, vertices[0].position};
        for(auto& v : vertices) bounds.expand(v.position);
    }

    // Compute smooth normals from face normals
    void computeNormals() {
        // Zero normals
        for(auto& v : vertices) v.normal = Vec3::zero();
        // Accumulate face normals
        auto doTri = [&](uint32_t i0, uint32_t i1, uint32_t i2) {
            Vec3& p0 = vertices[i0].position;
            Vec3& p1 = vertices[i1].position;
            Vec3& p2 = vertices[i2].position;
            Vec3 n = (p1-p0).cross(p2-p0);
            vertices[i0].normal += n;
            vertices[i1].normal += n;
            vertices[i2].normal += n;
        };
        if(indices.empty()) {
            for(size_t i=0; i+2<vertices.size(); i+=3) doTri(i,i+1,i+2);
        } else {
            for(size_t i=0; i+2<indices.size(); i+=3) doTri(indices[i],indices[i+1],indices[i+2]);
        }
        for(auto& v : vertices) v.normal = v.normal.normalized();
    }
};

// ============================================================
// OBJ Loader  –  AER_ObjLoad("file.obj")
// ============================================================
// Supports: v, vt, vn, f (triangles and quads), usemtl (ignored)
// Returns a Mesh ready to use.

inline Mesh AER_ObjLoad(const std::string& filename) {
    Mesh mesh;
    mesh.name = filename;

    std::ifstream file(filename);
    if(!file) {
        std::cerr << "AER_ObjLoad: cannot open '" << filename << "'\n";
        return mesh;
    }

    std::vector<Vec3> positions;
    std::vector<Vec2> uvs;
    std::vector<Vec3> normals;

    // Maps "pos/uv/norm" string → index in mesh.vertices
    std::unordered_map<std::string,uint32_t> vertCache;

    auto getOrAdd = [&](const std::string& key, int pi, int ti, int ni) -> uint32_t {
        auto it = vertCache.find(key);
        if(it != vertCache.end()) return it->second;
        MeshVertex v;
        if(pi > 0 && pi <= (int)positions.size()) v.position = positions[pi-1];
        else if(pi < 0 && -pi <= (int)positions.size()) v.position = positions[positions.size()+pi];
        if(ti > 0 && ti <= (int)uvs.size()) v.uv = uvs[ti-1];
        else if(ti < 0 && -ti <= (int)uvs.size()) v.uv = uvs[uvs.size()+ti];
        if(ni > 0 && ni <= (int)normals.size()) v.normal = normals[ni-1];
        else if(ni < 0 && -ni <= (int)normals.size()) v.normal = normals[normals.size()+ni];
        uint32_t idx = (uint32_t)mesh.vertices.size();
        mesh.vertices.push_back(v);
        vertCache[key] = idx;
        return idx;
    };

    auto parseFaceVert = [](const std::string& tok, int& pi, int& ti, int& ni) {
        pi=0; ti=0; ni=0;
        // Formats: v,  v/t,  v//n,  v/t/n
        size_t a = tok.find('/');
        if(a == std::string::npos) { pi = std::stoi(tok); return; }
        pi = std::stoi(tok.substr(0, a));
        size_t b = tok.find('/', a+1);
        if(b == std::string::npos) { if(a+1<tok.size()) ti=std::stoi(tok.substr(a+1)); return; }
        if(b > a+1) ti = std::stoi(tok.substr(a+1, b-a-1));
        if(b+1 < tok.size()) ni = std::stoi(tok.substr(b+1));
    };

    std::string line;
    while(std::getline(file, line)) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream ss(line);
        std::string token; ss >> token;

        if(token == "v") {
            Vec3 p; ss >> p.x >> p.y >> p.z;
            positions.push_back(p);
        } else if(token == "vt") {
            Vec2 uv; ss >> uv.x >> uv.y;
            uv.y = 1.f - uv.y; // OBJ UV origin is bottom-left
            uvs.push_back(uv);
        } else if(token == "vn") {
            Vec3 n; ss >> n.x >> n.y >> n.z;
            normals.push_back(n.normalized());
        } else if(token == "f") {
            // Read all face vertices (handles triangles and polygons)
            std::vector<uint32_t> faceVerts;
            std::string tok;
            while(ss >> tok) {
                int pi,ti,ni;
                parseFaceVert(tok, pi,ti,ni);
                faceVerts.push_back(getOrAdd(tok, pi,ti,ni));
            }
            // Fan triangulation
            for(size_t i=1; i+1<faceVerts.size(); i++) {
                mesh.indices.push_back(faceVerts[0]);
                mesh.indices.push_back(faceVerts[i]);
                mesh.indices.push_back(faceVerts[i+1]);
            }
        }
        // mtllib / usemtl / o / g / s – ignored (no material system yet)
    }

    // If no normals were in the file, compute them
    bool hasNormals = false;
    for(auto& v : mesh.vertices)
        if(v.normal.lengthSq() > 0.1f) { hasNormals=true; break; }
    if(!hasNormals) mesh.computeNormals();

    mesh.computeBounds();
    std::cout << "AER_ObjLoad: '" << filename
              << "'  verts=" << mesh.vertices.size()
              << "  tris=" << mesh.indices.size()/3 << "\n";
    return mesh;
}

// ============================================================
// Font Atlas  –  8x8 bitmap font for HUD text
// Built-in ASCII printable characters (32-126)
// ============================================================
// A minimal 8×8 pixel font stored as bitfields
// (1 bit per pixel, 8 bytes per glyph, 96 glyphs)
namespace _FontData {
    // Very compact ASCII 8x8 font (standard IBM PC font subset)
    // Each entry is 8 bytes = 8 rows of 8 pixels, MSB = leftmost pixel
    static const uint8_t glyphs[96][8] = {
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // ' '
        {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // '!'
        {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // '"'
        {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, // '#'
        {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, // '$'
        {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, // '%'
        {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, // '&'
        {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, // '\''
        {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, // '('
        {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, // ')'
        {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // '*'
        {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, // '+'
        {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, // ','
        {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, // '-'
        {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, // '.'
        {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, // '/'
        {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, // '0'
        {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, // '1'
        {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, // '2'
        {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, // '3'
        {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, // '4'
        {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, // '5'
        {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, // '6'
        {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, // '7'
        {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, // '8'
        {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, // '9'
        {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, // ':'
        {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06}, // ';'
        {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, // '<'
        {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, // '='
        {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // '>'
        {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, // '?'
        {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, // '@'
        {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, // 'A'
        {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, // 'B'
        {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, // 'C'
        {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, // 'D'
        {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, // 'E'
        {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, // 'F'
        {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, // 'G'
        {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, // 'H'
        {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 'I'
        {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, // 'J'
        {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, // 'K'
        {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, // 'L'
        {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, // 'M'
        {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, // 'N'
        {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, // 'O'
        {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, // 'P'
        {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, // 'Q'
        {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, // 'R'
        {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, // 'S'
        {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 'T'
        {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, // 'U'
        {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, // 'V'
        {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // 'W'
        {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, // 'X'
        {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, // 'Y'
        {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, // 'Z'
        {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00}, // '['
        {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, // '\\'
        {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, // ']'
        {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, // '^'
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // '_'
        {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, // '`'
        {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, // 'a'
        {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, // 'b'
        {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, // 'c'
        {0x38,0x30,0x30,0x3e,0x33,0x33,0x6E,0x00}, // 'd'
        {0x00,0x00,0x1E,0x33,0x3f,0x03,0x1E,0x00}, // 'e'
        {0x1C,0x36,0x06,0x0f,0x06,0x06,0x0F,0x00}, // 'f'
        {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, // 'g'
        {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, // 'h'
        {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, // 'i'
        {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, // 'j'
        {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, // 'k'
        {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 'l'
        {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, // 'm'
        {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, // 'n'
        {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, // 'o'
        {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, // 'p'
        {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, // 'q'
        {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, // 'r'
        {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, // 's'
        {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, // 't'
        {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, // 'u'
        {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, // 'v'
        {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, // 'w'
        {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, // 'x'
        {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, // 'y'
        {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, // 'z'
        {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, // '{'
        {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // '|'
        {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, // '}'
        {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, // '~'
        {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, // DEL (filled block)
    };
} // namespace _FontData

struct FontAtlas {
    static constexpr int GlyphW = 8;
    static constexpr int GlyphH = 8;

    // Returns true/false for pixel (x,y) in glyph for char c
    static bool pixel(char c, int x, int y) {
        int idx = (int)c - 32;
        if(idx < 0 || idx >= 96) idx = 0;
        if(x < 0 || x >= 8 || y < 0 || y >= 8) return false;
        return (_FontData::glyphs[idx][y] >> (7-x)) & 1;
    }
};

// ============================================================
// Asset Manager (cache of textures and meshes)
// ============================================================
class AssetManager {
public:
    // Returns texture ID (-1 on failure)
    int loadTexture(const std::string& path) {
        auto it = m_texCache.find(path);
        if(it != m_texCache.end()) return it->second;
        Texture tex = Texture::loadPPM(path);
        if(!tex.valid()) return -1;
        int id = (int)m_textures.size();
        m_textures.push_back(std::move(tex));
        m_texCache[path] = id;
        return id;
    }

    int addTexture(Texture tex) {
        int id = (int)m_textures.size();
        if(!tex.path.empty()) m_texCache[tex.path] = id;
        m_textures.push_back(std::move(tex));
        return id;
    }

    const Texture* getTexture(int id) const {
        if(id < 0 || id >= (int)m_textures.size()) return nullptr;
        return &m_textures[id];
    }
    Texture* getTexture(int id) {
        if(id < 0 || id >= (int)m_textures.size()) return nullptr;
        return &m_textures[id];
    }

    int textureCount() const { return (int)m_textures.size(); }

    int loadMesh(const std::string& path) {
        auto it = m_meshCache.find(path);
        if(it != m_meshCache.end()) return it->second;
        Mesh m = AER_ObjLoad(path);
        if(m.vertices.empty()) return -1;
        int id = (int)m_meshes.size();
        m_meshes.push_back(std::move(m));
        m_meshCache[path] = id;
        return id;
    }

    int addMesh(Mesh mesh) {
        int id = (int)m_meshes.size();
        if(!mesh.name.empty()) m_meshCache[mesh.name] = id;
        m_meshes.push_back(std::move(mesh));
        return id;
    }

    const Mesh* getMesh(int id) const {
        if(id < 0 || id >= (int)m_meshes.size()) return nullptr;
        return &m_meshes[id];
    }
    Mesh* getMesh(int id) {
        if(id < 0 || id >= (int)m_meshes.size()) return nullptr;
        return &m_meshes[id];
    }

private:
    std::vector<Texture> m_textures;
    std::vector<Mesh>    m_meshes;
    std::unordered_map<std::string,int> m_texCache;
    std::unordered_map<std::string,int> m_meshCache;
};

// ============================================================
// Audio Stub (expand with SDL_mixer or miniaudio)
// ============================================================
class AudioSystem {
public:
    void playSound(const std::string& file, bool loop=false, float volume=1.f) {
        // TODO: Integrate SDL_mixer:
        // Mix_Chunk* chunk = Mix_LoadWAV(file.c_str());
        // Mix_VolumeChunk(chunk, (int)(volume*MIX_MAX_VOLUME));
        // Mix_PlayChannel(-1, chunk, loop ? -1 : 0);
        std::cout << "AER Audio: play '" << file << "' loop=" << loop << " vol=" << volume << "\n";
    }
    void stopAll() {
        // Mix_HaltChannel(-1);
    }
    void setMasterVolume(float v) { m_masterVol = v; }
    float masterVolume() const { return m_masterVol; }
private:
    float m_masterVol = 1.f;
};

} // namespace AER
#endif // AER_ASSETS_H