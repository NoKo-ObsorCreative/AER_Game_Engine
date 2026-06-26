#pragma once
// ============================================================
// AER_ECS.h  –  Entity / Component System + Scene Graph
// AER Game Engine  |  Alpha 0.3
// ============================================================
#ifndef AER_ECS_H
#define AER_ECS_H

#include "AER_Math.h"
#include <vector>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <algorithm>
#include <functional>

namespace AER {

// ============================================================
// Component base
// ============================================================
struct Component {
    virtual ~Component() = default;
    virtual void update(float dt) {}
    bool enabled = true;
};

// ============================================================
// Transform  (position, rotation, scale + parent/child links)
// ============================================================
struct Transform {
    Vec3       position {0,0,0};
    Quaternion rotation {};
    Vec3       scale    {1,1,1};

    // Parent-child
    Transform*              parent   = nullptr;
    std::vector<Transform*> children;

    void addChild(Transform* child) {
        if(!child || child == this) return;
        child->parent = this;
        children.push_back(child);
    }
    void removeChild(Transform* child) {
        auto it = std::find(children.begin(), children.end(), child);
        if(it != children.end()) {
            (*it)->parent = nullptr;
            children.erase(it);
        }
    }

    Mat4 localMatrix() const {
        Mat4 T = Mat4::translation(position);
        Mat4 R = rotation.normalized().toMat4();
        Mat4 S = Mat4::scale(scale);
        return T * R * S;
    }

    // World matrix = parent's world * local
    Mat4 worldMatrix() const {
        if(parent) return parent->worldMatrix() * localMatrix();
        return localMatrix();
    }

    Vec3 worldPosition() const {
        Mat4 wm = worldMatrix();
        return { wm.m[12], wm.m[13], wm.m[14] };
    }

    Vec3 forward() const {
        return rotation.rotate({0,0,-1});
    }
    Vec3 right() const {
        return rotation.rotate({1,0,0});
    }
    Vec3 up() const {
        return rotation.rotate({0,1,0});
    }
};

// ============================================================
// Entity
// ============================================================
using EntityID = uint32_t;

class Entity {
public:
    EntityID    id     = 0;
    std::string name   = "Entity";
    bool        active = true;
    Transform   transform;

    // ---- Component API ----
    template<typename T, typename... Args>
    T* addComponent(Args&&... args) {
        auto comp = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = comp.get();
        m_components[std::type_index(typeid(T))].push_back(std::move(comp));
        return ptr;
    }

    template<typename T>
    T* getComponent() {
        auto it = m_components.find(std::type_index(typeid(T)));
        if(it == m_components.end() || it->second.empty()) return nullptr;
        return static_cast<T*>(it->second[0].get());
    }

    template<typename T>
    std::vector<T*> getComponents() {
        std::vector<T*> out;
        auto it = m_components.find(std::type_index(typeid(T)));
        if(it == m_components.end()) return out;
        for(auto& c : it->second) out.push_back(static_cast<T*>(c.get()));
        return out;
    }

    template<typename T>
    bool hasComponent() { return getComponent<T>() != nullptr; }

    template<typename T>
    void removeComponent() {
        m_components.erase(std::type_index(typeid(T)));
    }

    void update(float dt) {
        if(!active) return;
        for(auto& [type, vec] : m_components)
            for(auto& c : vec)
                if(c->enabled) c->update(dt);
    }

private:
    std::unordered_map<
        std::type_index,
        std::vector<std::unique_ptr<Component>>
    > m_components;
};

// ============================================================
// Scene  –  owns all entities, manages hierarchy
// ============================================================
class Scene {
public:
    std::string name = "Scene";

    Entity* createEntity(const std::string& n = "Entity") {
        auto e = std::make_unique<Entity>();
        e->id   = m_nextID++;
        e->name = n;
        Entity* ptr = e.get();
        m_entities.push_back(std::move(e));
        return ptr;
    }

    void destroyEntity(EntityID id) {
        m_entities.erase(
            std::remove_if(m_entities.begin(), m_entities.end(),
                [id](const std::unique_ptr<Entity>& e){ return e->id == id; }),
            m_entities.end());
    }

    Entity* findByName(const std::string& n) {
        for(auto& e : m_entities)
            if(e->name == n) return e.get();
        return nullptr;
    }

    Entity* findByID(EntityID id) {
        for(auto& e : m_entities)
            if(e->id == id) return e.get();
        return nullptr;
    }

    void update(float dt) {
        for(auto& e : m_entities)
            e->update(dt);
    }

    // Iterate all entities
    void forEach(std::function<void(Entity&)> fn) {
        for(auto& e : m_entities) fn(*e);
    }

    // Return all entities with a given component type
    template<typename T>
    std::vector<Entity*> entitiesWith() {
        std::vector<Entity*> out;
        for(auto& e : m_entities)
            if(e->hasComponent<T>()) out.push_back(e.get());
        return out;
    }

    const std::vector<std::unique_ptr<Entity>>& entities() const { return m_entities; }

private:
    std::vector<std::unique_ptr<Entity>> m_entities;
    EntityID m_nextID = 1;
};

} // namespace AER
#endif // AER_ECS_H