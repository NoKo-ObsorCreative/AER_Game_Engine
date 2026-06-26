#pragma once
// ============================================================
// AER_Window.h  –  SDL2 window, game loop, fixed-timestep
// AER Game Engine  |  Alpha 0.3
// ============================================================
#ifndef AER_WINDOW_H
#define AER_WINDOW_H

#include "AER_Input.h"
#include "AER_Renderer.h"
#include <SDL2/SDL.h>
#include <string>
#include <functional>
#include <iostream>
#include <chrono>

namespace AER {

// ============================================================
// Window
// ============================================================
class Window {
public:
    bool vsync    = true;
    bool running  = false;

    bool create(const std::string& title, int w, int h, bool fullscreen=false) {
        m_w=w; m_h=h;
        if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS) != 0){
            std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
            return false;
        }
        Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
        if(fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

        m_window = SDL_CreateWindow(
            title.c_str(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            w, h, flags);
        if(!m_window){
            std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
            return false;
        }

        m_renderer = SDL_CreateRenderer(
            m_window, -1,
            SDL_RENDERER_ACCELERATED | (vsync ? SDL_RENDERER_PRESENTVSYNC : 0));
        if(!m_renderer){
            std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
            return false;
        }

        // Streaming texture we'll blit the software framebuffer into
        m_texture = SDL_CreateTexture(
            m_renderer,
            SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_STREAMING,
            w, h);
        if(!m_texture){
            std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << "\n";
            return false;
        }

        running = true;
        std::cout << "AER Window created: " << title
                  << "  " << w << "x" << h << "\n";
        return true;
    }

    void destroy() {
        if(m_texture)  { SDL_DestroyTexture(m_texture);   m_texture=nullptr; }
        if(m_renderer) { SDL_DestroyRenderer(m_renderer); m_renderer=nullptr; }
        if(m_window)   { SDL_DestroyWindow(m_window);     m_window=nullptr; }
        SDL_Quit();
    }

    // Upload a software framebuffer (RGBA float → RGBA8) and present
    void present(const std::vector<float>& colorBuf, int fbW, int fbH) {
        if(!m_texture) return;
        void*  pixels; int pitch;
        SDL_LockTexture(m_texture, nullptr, &pixels, &pitch);
        auto* dst = (uint8_t*)pixels;
        int pw = pitch/4; // pixels per row in texture

        // Software buffer is stored bottom-left origin (flip Y)
        for(int py=0; py<fbH; py++) {
            int srcRow = fbH-1-py; // flip
            for(int px=0; px<fbW; px++) {
                int si = (srcRow*fbW+px)*4;
                int di = (py*pw+px)*4;
                auto c8 = [](float v){ return (uint8_t)(std::min(std::max(v,0.f),1.f)*255); };
                dst[di+0]=c8(colorBuf[si+0]); // R
                dst[di+1]=c8(colorBuf[si+1]); // G
                dst[di+2]=c8(colorBuf[si+2]); // B
                dst[di+3]=c8(colorBuf[si+3]); // A
            }
        }
        SDL_UnlockTexture(m_texture);

        SDL_RenderClear(m_renderer);
        // Scale framebuffer to window
        SDL_Rect dst2{0,0,m_w,m_h};
        SDL_RenderCopy(m_renderer, m_texture, nullptr, &dst2);
        SDL_RenderPresent(m_renderer);
    }

    // Poll and dispatch SDL events
    // Returns false if the window should close
    bool pollEvents(InputSystem& input) {
        input.beginFrame();
        SDL_Event e;
        while(SDL_PollEvent(&e)) {
            input.processEvent(e);
            if(e.type == SDL_QUIT) { running=false; return false; }
            if(e.type == SDL_WINDOWEVENT &&
               e.window.event == SDL_WINDOWEVENT_RESIZED) {
                m_w = e.window.data1;
                m_h = e.window.data2;
            }
        }
        if(input.isKeyPressed(Key::Escape)) { running=false; return false; }
        return true;
    }

    int width()  const { return m_w; }
    int height() const { return m_h; }
    SDL_Window*   sdlWindow()   const { return m_window; }
    SDL_Renderer* sdlRenderer() const { return m_renderer; }

private:
    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture*  m_texture  = nullptr;
    int m_w=0, m_h=0;
};

// ============================================================
// GameLoop  –  Fixed-timestep update + variable-rate render
//
// Usage:
//   GameLoop loop;
//   loop.fixedStep = 1.0/60.0;       // physics Hz
//   loop.maxFrameTime = 0.1;         // spiral-of-death guard
//   loop.run(
//       [&](double dt){ /* fixed update */ },
//       [&](double alpha){ /* render (alpha = interpolation) */ }
//   );
// ============================================================
class GameLoop {
public:
    double fixedStep    = 1.0/60.0;
    double maxFrameTime = 0.1;
    bool*  shouldQuit   = nullptr;  // point to Window::running

    using FixedFn  = std::function<void(double dt)>;
    using RenderFn = std::function<void(double alpha)>;

    // Run until *shouldQuit becomes false.
    // fixedFn called at fixed rate; renderFn at maximum rate.
    void run(FixedFn fixedFn, RenderFn renderFn) {
        using Clock = std::chrono::high_resolution_clock;
        auto  t0    = Clock::now();
        double accumulator = 0.0;
        double currentTime = 0.0;

        while(!shouldQuit || *shouldQuit) {
            auto now = Clock::now();
            double newTime = std::chrono::duration<double>(now - t0).count();
            double frameTime = std::min(newTime - currentTime, maxFrameTime);
            currentTime = newTime;

            accumulator += frameTime;
            while(accumulator >= fixedStep) {
                fixedFn(fixedStep);
                accumulator -= fixedStep;
            }

            double alpha = accumulator / fixedStep; // [0,1) interpolation
            renderFn(alpha);
        }
    }

    // Simple version: just calls update+render once per iteration
    // with real delta time. Useful for quick prototypes.
    void runSimple(Window& win, InputSystem& input,
                   std::function<void(float dt)> update,
                   std::function<void()> render)
    {
        using Clock = std::chrono::high_resolution_clock;
        auto  prev  = Clock::now();

        while(win.running) {
            auto  now = Clock::now();
            float dt  = (float)std::chrono::duration<double>(now - prev).count();
            prev = now;
            dt = std::min(dt, 0.1f);  // cap

            if(!win.pollEvents(input)) break;
            update(dt);
            render();
        }
    }
};

} // namespace AER
#endif // AER_WINDOW_H