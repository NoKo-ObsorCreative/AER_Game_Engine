
#pragma once
// ============================================================
// AER_Physics.h  –  Rigid body physics + AABB collision
// AER Game Engine  |  Alpha 0.3
// ============================================================
#ifndef AER_PHYSICS_H
#define AER_PHYSICS_H

#include "AER_Math.h"
#include "AER_ECS.h"
#include <vector>
#include <functional>

namespace AER {

// ============================================================
// Rigidbody Component
// ============================================================
struct Rigidbody : Component {
    Vec3  velocity     {0,0,0};
    Vec3  acceleration {0,0,0};    // per-frame forces / mass
    Vec3  gravity      {0,-9.81f,0};
    float mass         = 1.f;
    float drag         = 0.01f;    // linear damping (0 = no drag)
    bool  isKinematic  = false;    // if true, not moved by physics
    bool  useGravity   = true;
    bool  isGrounded   = false;

    // Accumulated per-step forces (reset each step)
    Vec3 m_force{};

    void addForce(const Vec3& f) { m_force += f; }
    void addImpulse(const Vec3& impulse) { velocity += impulse * (1.f/mass); }

    void update(float dt) override {
        if(isKinematic) return;
        Vec3 accel = m_force * (1.f/mass);
        if(useGravity) accel += gravity;
        velocity   += accel * dt;
        velocity   *= (1.f - drag * dt);   // damping
        m_force     = Vec3::zero();
    }
};

// ============================================================
// Collider  (AABB based)
// ============================================================
struct Collider : Component {
    AABB   localBox;     // in local space
    bool   isTrigger = false;      // trigger = overlap event, no physics response
    bool   m_hit     = false;

    using CollisionCallback = std::function<void(Entity*, Entity*)>;
    CollisionCallback onCollision;
    CollisionCallback onTrigger;

    // World AABB given the entity's world transform
    AABB worldBox(const Transform& t) const {
        Vec3 wp = t.worldPosition();
        Vec3 s  = t.scale;
        return AABB{
            wp + Vec3{localBox.min.x*s.x, localBox.min.y*s.y, localBox.min.z*s.z},
            wp + Vec3{localBox.max.x*s.x, localBox.max.y*s.y, localBox.max.z*s.z}
        };
    }
};

// ============================================================
// PhysicsWorld  –  Fixed-timestep simulation + collision
// ============================================================
class PhysicsWorld {
public:
    float fixedStep   = 1.f/60.f;
    float maxStepTime = 0.1f;      // prevent spiral of death
    int   maxSubSteps = 8;

    // Call once per render frame with the real elapsed time
    void step(Scene& scene, float realDt) {
        realDt = std::min(realDt, maxStepTime);
        m_accumulator += realDt;

        int steps = 0;
        while(m_accumulator >= fixedStep && steps < maxSubSteps) {
            fixedUpdate(scene, fixedStep);
            m_accumulator -= fixedStep;
            ++steps;
        }
    }

private:
    float m_accumulator = 0.f;

    void fixedUpdate(Scene& scene, float dt) {
        // 1. Integrate velocities
        auto rbs = scene.entitiesWith<Rigidbody>();
        for(Entity* e : rbs) {
            auto* rb = e->getComponent<Rigidbody>();
            if(!rb || rb->isKinematic) continue;
            rb->update(dt);
            e->transform.position += rb->velocity * dt;
        }

        // 2. Broad phase – collect colliders
        struct ColEntry { Entity* entity; Collider* col; };
        std::vector<ColEntry> cols;
        scene.forEach([&](Entity& e){
            auto* c = e.getComponent<Collider>();
            if(c) cols.push_back({&e, c});
        });

        // 3. Narrow phase – test all pairs
        for(size_t i = 0; i < cols.size(); i++) {
            for(size_t j = i+1; j < cols.size(); j++) {
                Entity*   ea = cols[i].entity; Collider* ca = cols[i].col;
                Entity*   eb = cols[j].entity; Collider* cb = cols[j].col;

                AABB wa = ca->worldBox(ea->transform);
                AABB wb = cb->worldBox(eb->transform);

                if(!wa.intersects(wb)) continue;

                // Trigger events
                if(ca->isTrigger || cb->isTrigger) {
                    if(ca->onTrigger) ca->onTrigger(ea, eb);
                    if(cb->onTrigger) cb->onTrigger(eb, ea);
                    continue;
                }

                // Physics response
                Vec3 mtv = wa.resolve(wb);
                auto* rba = ea->getComponent<Rigidbody>();
                auto* rbb = eb->getComponent<Rigidbody>();

                bool ka = !rba || rba->isKinematic;
                bool kb = !rbb || rbb->isKinematic;

                if(!ka && !kb) {
                    // Both dynamic – split MTV
                    ea->transform.position -= mtv * 0.5f;
                    eb->transform.position += mtv * 0.5f;
                    // Reflect velocities along collision normal
                    Vec3 n = mtv.normalized();
                    if(rba) { float vn = rba->velocity.dot(n); if(vn < 0) rba->velocity -= n*(vn*1.0f); }
                    if(rbb) { float vn = rbb->velocity.dot(-n); if(vn < 0) rbb->velocity += n*(vn*1.0f); }
                } else if(!ka) {
                    // A is dynamic, B is static
                    ea->transform.position -= mtv;
                    Vec3 n = mtv.normalized();
                    if(rba) {
                        float vn = rba->velocity.dot(n);
                        if(vn < 0) rba->velocity -= n*vn;
                        // Simple ground detection
                        if(n.y > 0.7f) rba->isGrounded = true;
                    }
                } else if(!kb) {
                    // B is dynamic, A is static
                    eb->transform.position += mtv;
                    Vec3 n = (-mtv).normalized();
                    if(rbb) {
                        float vn = rbb->velocity.dot(n);
                        if(vn < 0) rbb->velocity -= n*vn;
                        if(n.y > 0.7f) rbb->isGrounded = true;
                    }
                }

                if(ca->onCollision) ca->onCollision(ea, eb);
                if(cb->onCollision) cb->onCollision(eb, ea);
            }
        }

        // Reset grounded each step (re-set when collision detected)
        for(Entity* e : rbs) {
            auto* rb = e->getComponent<Rigidbody>();
            if(rb) rb->isGrounded = false;
        }
    }
};

} // namespace AER
#endif // AER_PHYSICS_H