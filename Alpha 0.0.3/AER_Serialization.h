#pragma once
// ============================================================
// AER_Serialization.h  –  Scene save / load
// AER Game Engine  |  Alpha 0.3
// Simple human-readable text format (key=value blocks)
// ============================================================
#ifndef AER_SERIALIZATION_H
#define AER_SERIALIZATION_H

#include "AER_Math.h"
#include "AER_ECS.h"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <unordered_map>

namespace AER {

// ============================================================
// MeshRenderer Component (used for serialization / rendering)
// ============================================================
struct MeshRenderer : Component {
    int meshID    = -1;
    int textureID = -1;
    Color tint    {1,1,1,1};
    bool castShadow = true;
};

// ============================================================
// SceneSerializer
//
// File format (*.aer):
//   scene "MyScene"
//   entity "Player"
//     pos  0.0 1.0 0.0
//     rot  0.0 0.0 0.0 1.0   ; quaternion xyzw
//     scale 1.0 1.0 1.0
//     mesh "player.obj"
//     texture "player.ppm"
//     parent ""              ; empty = no parent
//   end
// ============================================================
struct SerializedEntity {
    std::string name;
    Vec3        position{0,0,0};
    Quaternion  rotation{0,0,0,1};
    Vec3        scale{1,1,1};
    std::string meshPath;
    std::string texturePath;
    std::string parentName;
    bool        active = true;
};

class SceneSerializer {
public:
    // Save scene to file
    static bool save(const Scene& scene, const std::string& path) {
        std::ofstream out(path);
        if(!out) { std::cerr << "AER_Serializer: cannot write '" << path << "'\n"; return false; }

        out << "scene \"" << scene.name << "\"\n\n";

        for(const auto& ep : scene.entities()) {
            const Entity& e = *ep;
            out << "entity \"" << e.name << "\"\n";
            out << "  active " << (e.active ? 1 : 0) << "\n";

            const Transform& t = e.transform;
            out << "  pos   " << t.position.x << " " << t.position.y << " " << t.position.z << "\n";
            out << "  rot   " << t.rotation.x << " " << t.rotation.y
                              << " " << t.rotation.z << " " << t.rotation.w << "\n";
            out << "  scale " << t.scale.x << " " << t.scale.y << " " << t.scale.z << "\n";

            // MeshRenderer
            const MeshRenderer* mr = const_cast<Entity&>(e).getComponent<MeshRenderer>();
            // We store paths – caller must have set mesh.name / texture.path
            out << "  mesh \"\"" << "\n";
            out << "  texture \"\"" << "\n";

            // Parent
            std::string parentName = "";
            if(t.parent) {
                // Find parent entity name by searching scene
                // (simplified: parent pointer→search)
                for(const auto& other : scene.entities()) {
                    if(&other->transform == t.parent) { parentName = other->name; break; }
                }
            }
            out << "  parent \"" << parentName << "\"\n";
            out << "end\n\n";
        }
        std::cout << "AER_Serializer: saved '" << path << "'\n";
        return true;
    }

    // Load scene – returns list of serialized entity descriptors
    // Caller should instantiate entities and load assets accordingly
    static std::vector<SerializedEntity> load(const std::string& path) {
        std::vector<SerializedEntity> result;
        std::ifstream in(path);
        if(!in) { std::cerr << "AER_Serializer: cannot open '" << path << "'\n"; return result; }

        std::string line;
        SerializedEntity* cur = nullptr;

        auto stripQuotes = [](const std::string& s) -> std::string {
            if(s.size()>=2 && s.front()=='"' && s.back()=='"')
                return s.substr(1, s.size()-2);
            return s;
        };

        while(std::getline(in, line)) {
            // Strip comments (;)
            size_t sc = line.find(';');
            if(sc != std::string::npos) line = line.substr(0, sc);
            // Trim
            while(!line.empty() && (line.front()==' '||line.front()=='\t')) line=line.substr(1);
            while(!line.empty() && (line.back() ==' '||line.back() =='\t')) line.pop_back();
            if(line.empty()) continue;

            std::istringstream ss(line);
            std::string token; ss >> token;

            if(token == "scene") {
                std::string sname; ss >> sname;
                // Scene name (ignored for now)
            } else if(token == "entity") {
                result.emplace_back();
                cur = &result.back();
                std::string ename; ss >> ename;
                cur->name = stripQuotes(ename);
            } else if(token == "end") {
                cur = nullptr;
            } else if(cur) {
                if(token == "active")  { int v; ss>>v; cur->active=(v!=0); }
                else if(token=="pos")  { ss>>cur->position.x>>cur->position.y>>cur->position.z; }
                else if(token=="rot")  { ss>>cur->rotation.x>>cur->rotation.y>>cur->rotation.z>>cur->rotation.w; }
                else if(token=="scale"){ ss>>cur->scale.x>>cur->scale.y>>cur->scale.z; }
                else if(token=="mesh") { std::string v; ss>>v; cur->meshPath=stripQuotes(v); }
                else if(token=="texture"){ std::string v; ss>>v; cur->texturePath=stripQuotes(v); }
                else if(token=="parent") { std::string v; ss>>v; cur->parentName=stripQuotes(v); }
            }
        }

        std::cout << "AER_Serializer: loaded " << result.size()
                  << " entities from '" << path << "'\n";
        return result;
    }

    // Convenience: populate a Scene from a list of SerializedEntity descriptors
    // Pass in an AssetManager to load meshes/textures
    static void populateScene(Scene& scene,
                              const std::vector<SerializedEntity>& descs)
    {
        // First pass: create all entities
        std::unordered_map<std::string, Entity*> nameMap;
        for(const auto& d : descs) {
            Entity* e = scene.createEntity(d.name);
            e->active = d.active;
            e->transform.position = d.position;
            e->transform.rotation = d.rotation;
            e->transform.scale    = d.scale;
            nameMap[d.name] = e;
        }
        // Second pass: set up parent-child
        for(size_t i=0; i<descs.size(); i++) {
            const auto& d = descs[i];
            if(!d.parentName.empty()) {
                auto pit = nameMap.find(d.parentName);
                auto cit = nameMap.find(d.name);
                if(pit != nameMap.end() && cit != nameMap.end()) {
                    pit->second->transform.addChild(&cit->second->transform);
                }
            }
        }
    }
};

} // namespace AER
#endif // AER_SERIALIZATION_H