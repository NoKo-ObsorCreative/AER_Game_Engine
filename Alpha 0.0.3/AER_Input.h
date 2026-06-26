
#pragma once
// ============================================================
// AER_Input.h  –  Keyboard / Mouse input state tracking
// AER Game Engine  |  Alpha 0.3
// Requires SDL2 at link time.
// ============================================================
#ifndef AER_INPUT_H
#define AER_INPUT_H

#include "AER_Math.h"
#include <SDL2/SDL.h>
#include <unordered_map>
#include <string>

namespace AER {

// --------------------------------------------------------- Key codes --------
// Re-export SDL scancodes as AER keys for cleaner API
namespace Key {
    constexpr int W       = SDL_SCANCODE_W;
    constexpr int A       = SDL_SCANCODE_A;
    constexpr int S       = SDL_SCANCODE_S;
    constexpr int D       = SDL_SCANCODE_D;
    constexpr int Q       = SDL_SCANCODE_Q;
    constexpr int E       = SDL_SCANCODE_E;
    constexpr int Space   = SDL_SCANCODE_SPACE;
    constexpr int LShift  = SDL_SCANCODE_LSHIFT;
    constexpr int LCtrl   = SDL_SCANCODE_LCTRL;
    constexpr int Escape  = SDL_SCANCODE_ESCAPE;
    constexpr int Return  = SDL_SCANCODE_RETURN;
    constexpr int Tab     = SDL_SCANCODE_TAB;
    constexpr int F1      = SDL_SCANCODE_F1;
    constexpr int F2      = SDL_SCANCODE_F2;
    constexpr int F3      = SDL_SCANCODE_F3;
    constexpr int Up      = SDL_SCANCODE_UP;
    constexpr int Down    = SDL_SCANCODE_DOWN;
    constexpr int Left    = SDL_SCANCODE_LEFT;
    constexpr int Right   = SDL_SCANCODE_RIGHT;
    constexpr int Num0    = SDL_SCANCODE_0;
    constexpr int Num1    = SDL_SCANCODE_1;
    // Add more as needed – any SDL_SCANCODE_* is valid
}

namespace MouseButton {
    constexpr int Left   = 0;
    constexpr int Middle = 1;
    constexpr int Right  = 2;
}

// --------------------------------------------------------- InputSystem ------
class InputSystem {
public:
    void beginFrame() {
        // Roll current → previous
        m_prevKeys  = m_currKeys;
        m_prevMouse = m_currMouse;
        m_mouseDelta = {0,0};
        m_scrollDelta = 0.f;
    }

    // Call once per SDL event in the event loop
    void processEvent(const SDL_Event& e) {
        switch(e.type) {
        case SDL_KEYDOWN:
            m_currKeys[e.key.keysym.scancode] = true;
            break;
        case SDL_KEYUP:
            m_currKeys[e.key.keysym.scancode] = false;
            break;
        case SDL_MOUSEBUTTONDOWN:
            m_currMouse[e.button.button - 1] = true;
            break;
        case SDL_MOUSEBUTTONUP:
            m_currMouse[e.button.button - 1] = false;
            break;
        case SDL_MOUSEMOTION:
            m_mousePos   = { (float)e.motion.x, (float)e.motion.y };
            m_mouseDelta = m_mouseDelta + Vec2{(float)e.motion.xrel, (float)e.motion.yrel};
            break;
        case SDL_MOUSEWHEEL:
            m_scrollDelta += e.wheel.y;
            break;
        }
    }

    // ---- Keyboard ---
    bool isKeyDown(int scancode)    const { return keyState(m_currKeys, scancode); }
    bool isKeyUp(int scancode)      const { return !keyState(m_currKeys, scancode); }
    bool isKeyPressed(int scancode) const { // just went down this frame
        return  keyState(m_currKeys, scancode) && !keyState(m_prevKeys, scancode);
    }
    bool isKeyReleased(int scancode) const { // just went up this frame
        return !keyState(m_currKeys, scancode) &&  keyState(m_prevKeys, scancode);
    }

    // ---- Mouse ------
    bool isMouseDown(int btn)    const { return mouseState(m_currMouse, btn); }
    bool isMousePressed(int btn) const { return  mouseState(m_currMouse,btn) && !mouseState(m_prevMouse,btn); }
    bool isMouseReleased(int btn) const { return !mouseState(m_currMouse,btn) &&  mouseState(m_prevMouse,btn); }
    Vec2 mousePos()   const { return m_mousePos; }
    Vec2 mouseDelta() const { return m_mouseDelta; }
    float scrollDelta() const { return m_scrollDelta; }

    // Lock/unlock cursor
    static void lockCursor(bool lock) {
        SDL_SetRelativeMouseMode(lock ? SDL_TRUE : SDL_FALSE);
    }

private:
    std::unordered_map<int,bool> m_currKeys, m_prevKeys;
    bool m_currMouse[8]={}, m_prevMouse[8]={};
    Vec2 m_mousePos{}, m_mouseDelta{};
    float m_scrollDelta = 0.f;

    static bool keyState(const std::unordered_map<int,bool>& map, int sc) {
        auto it = map.find(sc);
        return it != map.end() && it->second;
    }
    static bool mouseState(const bool arr[8], int btn) {
        return (btn >= 0 && btn < 8) ? arr[btn] : false;
    }
};

} // namespace AER
#endif // AER_INPUT_H