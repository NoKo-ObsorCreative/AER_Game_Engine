
#pragma once
// ============================================================
// AER_Camera.h  –  Camera system (FPS + Orbit)
// AER Game Engine  |  Alpha 0.3
// ============================================================
#ifndef AER_CAMERA_H
#define AER_CAMERA_H

#include "AER_Math.h"
#include "AER_Input.h"
#include <algorithm>

namespace AER {

struct Camera {
    // --- transform ---
    Vec3  position  {0, 2, 5};
    float yaw       = 0.f;   // radians, left/right
    float pitch     = 0.f;   // radians, up/down  (clamped ±89°)

    // --- projection ---
    float fovY      = 60.f;
    float nearPlane = 0.05f;
    float farPlane  = 1000.f;

    // --- movement ---
    float moveSpeed = 5.f;
    float lookSpeed = 0.002f;  // radians per pixel

    // Derived directions (call updateVectors() after changing yaw/pitch)
    Vec3 forward{0,0,-1}, right{1,0,0}, up{0,1,0};

    void updateVectors() {
        float cp = std::cos(pitch), sp = std::sin(pitch);
        float cy = std::cos(yaw),   sy = std::sin(yaw);
        forward = Vec3{ sy*cp, sp, -cy*cp }.normalized();
        right   = Vec3{ cy, 0.f, sy }.normalized();
        up      = right.cross(forward).normalized();
    }

    Mat4 viewMatrix() const {
        return Mat4::lookAt(position, position + forward, {0,1,0});
    }

    Mat4 projMatrix(float aspect) const {
        return Mat4::perspective(fovY, aspect, nearPlane, farPlane);
    }

    // Call each frame; mouseSensitivity scales look speed
    void processFPS(const InputSystem& input, float dt) {
        // Look
        Vec2 md = input.mouseDelta();
        yaw   += md.x * lookSpeed;
        pitch -= md.y * lookSpeed;
        pitch  = std::clamp(pitch, -1.55f, 1.55f); // ~±89°
        updateVectors();

        // Move (WASD + Space/Shift for up/down)
        Vec3 move;
        float speed = moveSpeed * dt;
        if(input.isKeyDown(Key::W)) move += forward * speed;
        if(input.isKeyDown(Key::S)) move -= forward * speed;
        if(input.isKeyDown(Key::A)) move -= right * speed;
        if(input.isKeyDown(Key::D)) move += right * speed;
        if(input.isKeyDown(Key::Space))  move.y += speed;
        if(input.isKeyDown(Key::LShift)) move.y -= speed;
        position += move;

        // Speed modifier
        if(input.isKeyDown(Key::LCtrl)) speed *= 3.f;
    }
};

} // namespace AER
#endif // AER_CAMERA_H