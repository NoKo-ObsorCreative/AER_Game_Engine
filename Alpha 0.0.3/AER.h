#pragma once
// ============================================================
// AER.h  –  Umbrella header for the AER Game Engine
// Alpha 0.3  |  Include this single file in your project.
//
// Modules:
//   AER_Math.h          Vec2, Vec3, Vec4, Color, Mat4, Quaternion,
//                       AABB, Frustum
//   AER_Input.h         Keyboard, mouse input state tracking
//   AER_Camera.h        FPS camera (yaw/pitch + WASD)
//   AER_ECS.h           Entity, Component, Transform (scene graph),
//                       Scene (entity manager)
//   AER_Physics.h       Rigidbody, Collider, PhysicsWorld (fixed-step)
//   AER_Assets.h        Texture (PPM, wrap/filter), Mesh, AER_ObjLoad(),
//                       FontAtlas (8×8 bitmap), AssetManager, AudioSystem
//   AER_Renderer.h      Software rasterizer: Blinn-Phong shading,
//                       smooth normals, full frustum clip, font HUD
//   AER_Serialization.h MeshRenderer component, SceneSerializer
//   AER_Window.h        SDL2 window, event loop, GameLoop
//
// Compile with SDL2:
//   g++ -std=c++17 -O2 main.cpp -lSDL2 -o game
//
// Quick-start example:
// -------------------------------------------------------
// #include "AER.h"
// using namespace AER;
//
// int main() {
//     Window   win;
//     win.create("My Game", 1280, 720);
//
//     Renderer rend;
//     rend.init(1280, 720);
//
//     InputSystem input;
//     Camera cam;
//     cam.updateVectors();
//     InputSystem::lockCursor(true);
//
//     AssetManager assets;
//     int meshID = assets.loadMesh("cube.obj");
//     int texID  = assets.loadTexture("crate.ppm");
//
//     Scene scene;
//     Entity* box = scene.createEntity("Box");
//     box->transform.position = {0, 0, -5};
//     auto* mr = box->addComponent<MeshRenderer>();
//     mr->meshID    = meshID;
//     mr->textureID = texID;
//
//     // Physics
//     PhysicsWorld physics;
//     auto* rb = box->addComponent<Rigidbody>();
//     auto* col = box->addComponent<Collider>();
//     col->localBox = AABB{{-0.5f,-0.5f,-0.5f},{0.5f,0.5f,0.5f}};
//
//     GameLoop loop;
//     loop.shouldQuit = &win.running;
//     loop.runSimple(win, input,
//         [&](float dt) {
//             cam.processFPS(input, dt);
//             physics.step(scene, dt);
//             scene.update(dt);
//         },
//         [&]() {
//             float aspect = 1280.f/720.f;
//             rend.clear();
//             rend.lightDir = {0.5f, 1.f, 0.3f};
//
//             scene.forEach([&](Entity& e) {
//                 auto* mr = e.getComponent<MeshRenderer>();
//                 if(!mr) return;
//                 const Mesh* mesh = assets.getMesh(mr->meshID);
//                 if(!mesh) return;
//                 rend.setTexture(assets.getTexture(mr->textureID));
//                 rend.setMatrices(
//                     e.transform.worldMatrix(),
//                     cam.viewMatrix(),
//                     cam.projMatrix(aspect));
//                 rend.drawMesh(*mesh);
//             });
//
//             rend.drawText("AER Engine v0.3", 8, 8, Color::white(), 2);
//             win.present(rend.colorBuffer(), 1280, 720);
//         });
//
//     win.destroy();
// }
// -------------------------------------------------------
// ============================================================
#ifndef AER_H
#define AER_H

#include "AER_Math.h"
#include "AER_Input.h"
#include "AER_Camera.h"
#include "AER_ECS.h"
#include "AER_Physics.h"
#include "AER_Assets.h"
#include "AER_Renderer.h"
#include "AER_Serialization.h"
#include "AER_Window.h"

// ============================================================
// Version info
// ============================================================
namespace AER {
    constexpr int VERSION_MAJOR = 0;
    constexpr int VERSION_MINOR = 3;
    constexpr int VERSION_PATCH = 0;

    inline const char* versionString() { return "AER 0.3.0"; }

    // Convenience: print engine info to stdout
    inline void printInfo() {
        printf("========================================\n");
        printf("  AER Game Engine  v%d.%d.%d\n",
               VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
        printf("  Modules: Math | Input | Camera\n");
        printf("           ECS  | Physics | Assets\n");
        printf("           Renderer | Serialization\n");
        printf("           Window (SDL2)\n");
        printf("========================================\n");
    }
}

#endif // AER_H