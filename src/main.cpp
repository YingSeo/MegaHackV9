#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <psapi.h>
#include <GL/gl.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <deque>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <fstream>

// ============================================================================
// 1. XINPUT PROXY FORWARDER (CHO PHÉP CHẠY TRỰC TIẾP QUA XINPUT1_4.DLL)
// ============================================================================
static HMODULE g_origXInput = nullptr;

static void initXInputProxy() {
    if (g_origXInput) return;
    char sysDir[MAX_PATH] = {0};
    GetSystemDirectoryA(sysDir, MAX_PATH);
    std::string path = std::string(sysDir) + "\\xinput1_4.dll";
    g_origXInput = LoadLibraryA(path.c_str());
    if (!g_origXInput) {
        std::string fallback = std::string(sysDir) + "\\xinput9_1_0.dll";
        g_origXInput = LoadLibraryA(fallback.c_str());
    }
}

extern "C" {
    __declspec(dllexport) DWORD WINAPI XInputGetState(DWORD dwUserIndex, void* pState) {
        initXInputProxy();
        using Fn = DWORD(WINAPI*)(DWORD, void*);
        static Fn fn = g_origXInput ? reinterpret_cast<Fn>(GetProcAddress(g_origXInput, "XInputGetState")) : nullptr;
        return fn ? fn(dwUserIndex, pState) : ERROR_DEVICE_NOT_CONNECTED;
    }

    __declspec(dllexport) DWORD WINAPI XInputSetState(DWORD dwUserIndex, void* pVibration) {
        initXInputProxy();
        using Fn = DWORD(WINAPI*)(DWORD, void*);
        static Fn fn = g_origXInput ? reinterpret_cast<Fn>(GetProcAddress(g_origXInput, "XInputSetState")) : nullptr;
        return fn ? fn(dwUserIndex, pVibration) : ERROR_DEVICE_NOT_CONNECTED;
    }

    __declspec(dllexport) DWORD WINAPI XInputGetCapabilities(DWORD dwUserIndex, DWORD dwFlags, void* pCapabilities) {
        initXInputProxy();
        using Fn = DWORD(WINAPI*)(DWORD, DWORD, void*);
        static Fn fn = g_origXInput ? reinterpret_cast<Fn>(GetProcAddress(g_origXInput, "XInputGetCapabilities")) : nullptr;
        return fn ? fn(dwUserIndex, dwFlags, pCapabilities) : ERROR_DEVICE_NOT_CONNECTED;
    }

    __declspec(dllexport) void WINAPI XInputEnable(BOOL enable) {
        initXInputProxy();
        using Fn = void(WINAPI*)(BOOL);
        static Fn fn = g_origXInput ? reinterpret_cast<Fn>(GetProcAddress(g_origXInput, "XInputEnable")) : nullptr;
        if (fn) fn(enable);
    }

    __declspec(dllexport) DWORD WINAPI XInputGetBatteryInformation(DWORD dwUserIndex, BYTE devType, void* pBatteryInfo) {
        initXInputProxy();
        using Fn = DWORD(WINAPI*)(DWORD, BYTE, void*);
        static Fn fn = g_origXInput ? reinterpret_cast<Fn>(GetProcAddress(g_origXInput, "XInputGetBatteryInformation")) : nullptr;
        return fn ? fn(dwUserIndex, devType, pBatteryInfo) : ERROR_DEVICE_NOT_CONNECTED;
    }

    __declspec(dllexport) DWORD WINAPI XInputGetKeystroke(DWORD dwUserIndex, DWORD dwReserved, void* pKeystroke) {
        initXInputProxy();
        using Fn = DWORD(WINAPI*)(DWORD, DWORD, void*);
        static Fn fn = g_origXInput ? reinterpret_cast<Fn>(GetProcAddress(g_origXInput, "XInputGetKeystroke")) : nullptr;
        return fn ? fn(dwUserIndex, dwReserved, pKeystroke) : ERROR_DEVICE_NOT_CONNECTED;
    }
}

// ============================================================================
// 2. COMPLETE MEGA HACK V9 PRO + QOLMOD STATE (ALL OFF BY DEFAULT)
// ============================================================================
struct MacroEvent {
    int tick;
    bool down;
};

struct TrailPoint {
    float x;
    float y;
};

struct MegaHackV9Engine {
    // Menu & Animation State
    bool menuOpen = false;
    bool particlePopupOpen = false;
    float animProgress = 0.0f; // 0.0 (Closed) -> 1.0 (Open)
    int animStyle = 0;         // 0: Smooth Expo+Back, 1: Slide Down, 2: Elastic Scale
    int hudAnchor = 0;         // 0: Top-Left, 1: Top-Right, 2: Bottom-Left, 3: Top-Center

    // Column 1: Cosmetic
    bool accuratePercentage = false;
    bool ballRotationBug = false;
    bool noGlow = false;
    bool noParticles = false;
    bool keepPortalParticles = true;
    bool hideGroundParticles = false;
    bool hideTrailParticles = false;
    bool hideClickParticles = false;
    bool noPortalCircle = false;
    bool noMirrorPortal = false;
    bool noCameraShake = false;
    bool layoutMode = false;
    bool playerOnTop = false;
    bool noWaveTrail = false;
    bool solidWaveTrail = false;
    bool p1RgbIcons = false;
    bool p2RgbIcons = false;

    // Column 2: Level & Physics
    bool noclip = false;
    bool noCollision = false;
    bool safeMode = false;
    bool autoSafeMode = true;
    bool forceIce = false;
    bool shipcopter = false;
    bool showTrajectory = false;
    bool showHitboxes = false;
    bool showHitboxesOnDeath = false;
    bool hitboxTrail = false;
    bool hitboxMultiplier = false;
    float hitboxScale = 0.50f;
    bool startPosSwitcher = false;
    bool fixedRandomSeed = false;
    unsigned int randomSeed = 1337;
    bool instantComplete = false;
    bool practiceMusicSync = false;
    bool frameStepper = false;
    bool stepAdvanceOnce = false;

    // Column 3: Universal & UI
    bool customBackground = false;
    int bgThemeIdx = 0;
    bool fastChests = false;
    bool freeWindowResize = false;

    // Column 4: Display, Speedhack & AutoClicker
    bool speedhackEnabled = false;
    float speedhackValue = 1.00f;
    bool speedhackAudio = false;
    bool frameExtrapolation = false;
    int targetFps = 480;
    bool autoClicker = false;
    int autoClickCps = 15;
    float autoClickTimer = 0.0f;
    bool autoClickDown = false;

    // Column 5: Status HUD
    bool cheatIndicator = false;
    bool showFps = false;
    bool showCps = false;
    bool showNoclipAccuracy = false;
    bool showNoclipDeaths = false;
    bool showAttempts = false;
    bool showBestRun = false;

    // Column 6: Bypass & Macro Bot
    bool iconBypass = false;
    bool copyHack = false;
    bool verifyHack = false;
    bool levelEditBypass = false;
    int macroMode = 0; // 0: Disabled, 1: Recording, 2: Playback
    std::vector<MacroEvent> macroBuffer;
    size_t macroCursor = 0;
    int physicsTick = 0;

    // Runtime Metrics & Simulation
    int currentStartPosIdx = 0;
    int totalStartPosCount = 6;
    int totalFrames = 0;
    int noclipHitFrames = 0;
    int noclipDeaths = 0;
    int attemptCount = 1;
    float bestRunFrom = 0.0f;
    float bestRunTo = 0.0f;
    float currentFps = 240.0f;
    float rgbHue = 0.0f;
    std::vector<double> clickTimes;
    size_t peakCps = 0;
    std::deque<TrailPoint> playerWaveHistory;
    float simPlayerY = 260.0f;
    float simPlayerVelY = 0.0f;

    // Input Edge Tracking
    bool prevTab = false;
    bool prevMouseLeft = false;
    bool prevKeyC = false;
    bool prevKeyQ = false;
    bool prevKeyE = false;
    bool prevKeyR = false;
    bool prevJumpInput = false;

    bool isAnyCheatActive() const {
        return noclip || noCollision || forceIce || shipcopter ||
               showTrajectory || showHitboxes || hitboxMultiplier ||
               layoutMode || fixedRandomSeed || instantComplete ||
               frameStepper || autoClicker || (macroMode == 2) ||
               (speedhackEnabled && std::abs(speedhackValue - 1.0f) > 0.001f);
    }

    bool isSafeProtected() const {
        return safeMode || (autoSafeMode && isAnyCheatActive());
    }

    void saveConfigFile() {
        std::ofstream out("mhv9_pro_config.ini", std::ios::trunc);
        if (!out.is_open()) return;
        out << "accuratePercentage=" << accuratePercentage << "\n"
            << "noGlow=" << noGlow << "\n"
            << "noParticles=" << noParticles << "\n"
            << "noPortalCircle=" << noPortalCircle << "\n"
            << "noMirrorPortal=" << noMirrorPortal << "\n"
            << "noCameraShake=" << noCameraShake << "\n"
            << "layoutMode=" << layoutMode << "\n"
            << "playerOnTop=" << playerOnTop << "\n"
            << "solidWaveTrail=" << solidWaveTrail << "\n"
            << "p1RgbIcons=" << p1RgbIcons << "\n"
            << "p2RgbIcons=" << p2RgbIcons << "\n"
            << "autoSafeMode=" << autoSafeMode << "\n"
            << "startPosSwitcher=" << startPosSwitcher << "\n"
            << "practiceMusicSync=" << practiceMusicSync << "\n"
            << "customBackground=" << customBackground << "\n"
            << "bgThemeIdx=" << bgThemeIdx << "\n"
            << "hudAnchor=" << hudAnchor << "\n"
            << "animStyle=" << animStyle << "\n"
            << "targetFps=" << targetFps << "\n"
            << "cheatIndicator=" << cheatIndicator << "\n"
            << "showFps=" << showFps << "\n"
            << "showCps=" << showCps << "\n"
            << "showNoclipAccuracy=" << showNoclipAccuracy << "\n"
            << "showNoclipDeaths=" << showNoclipDeaths << "\n"
            << "showAttempts=" << showAttempts << "\n"
            << "showBestRun=" << showBestRun << "\n";
    }
};

static MegaHackV9Engine g_mh;

// ============================================================================
// 3. ENGINE MEMORY PATCHER & COCOS2D-X / VSYNC CONTROLLERS
// ============================================================================
static uint8_t* findPatternInModule(HMODULE mod, const uint8_t* pattern, const char* mask) {
    if (!mod) return nullptr;
    MODULEINFO mi = {};
    if (!GetModuleInformation(GetCurrentProcess(), mod, &mi, sizeof(mi))) return nullptr;
    uint8_t* base = static_cast<uint8_t*>(mi.lpBaseOfDll);
    size_t size = mi.SizeOfImage;
    size_t patLen = std::strlen(mask);
    if (size < patLen) return nullptr;

    for (size_t i = 0; i <= size - patLen; ++i) {
        bool found = true;
        for (size_t j = 0; j < patLen; ++j) {
            if (mask[j] == 'x' && base[i + j] != pattern[j]) {
                found = false;
                break;
            }
        }
        if (found) return base + i;
    }
    return nullptr;
}

static void patchBytesSafe(void* addr, const uint8_t* bytes, size_t len) {
    if (!addr) return;
    DWORD oldProt = 0;
    if (VirtualProtect(addr, len, PAGE_EXECUTE_READWRITE, &oldProt)) {
        std::memcpy(addr, bytes, len);
        VirtualProtect(addr, len, oldProt, &oldProt);
    }
}

static void applyFpsAndVsyncSettings() {
    using WglGetProcFn = PROC(WINAPI*)(LPCSTR);
    HMODULE ogl = GetModuleHandleA("opengl32.dll");
    if (ogl) {
        auto getProc = reinterpret_cast<WglGetProcFn>(GetProcAddress(ogl, "wglGetProcAddress"));
        if (getProc) {
            using SwapIntervalFn = BOOL(WINAPI*)(int);
            auto swapInterval = reinterpret_cast<SwapIntervalFn>(getProc("wglSwapIntervalEXT"));
            if (swapInterval) {
                swapInterval(g_mh.frameExtrapolation ? 0 : 1);
            }
        }
    }

    // Call Cocos2d-x CCApplication::sharedApplication()->setAnimationInterval(1.0 / fps) if exported
    HMODULE cocos = GetModuleHandleA("libcocos2d.dll");
    if (cocos) {
        using SharedAppFn = void*(__cdecl*)();
        using SetIntervalFn = void(__fastcall*)(void*, double);
        auto sharedApp = reinterpret_cast<SharedAppFn>(GetProcAddress(cocos, "?sharedApplication@CCApplication@cocos2d@@SAPEAV12@XZ"));
        auto setInterval = reinterpret_cast<SetIntervalFn>(GetProcAddress(cocos, "?setAnimationInterval@CCApplication@cocos2d@@UEAAXN@Z"));
        if (sharedApp && setInterval) {
            void* app = sharedApp();
            if (app) {
                double interval = 1.0 / static_cast<double>(g_mh.frameExtrapolation ? g_mh.targetFps : 240);
                setInterval(app, interval);
            }
        }
    }
}

static void toggleNoclipMemoryPatch(bool enable) {
    static uint8_t* s_destroyPlayerAddr = nullptr;
    static uint8_t s_origDestroyBytes[5] = {0};
    static bool s_scanned = false;

    if (!s_scanned) {
        s_scanned = true;
        HMODULE gd = GetModuleHandleA("GeometryDash.exe");
        // Common x64 prologue for PlayLayer::destroyPlayer in GD 2.2081
        const uint8_t pat[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x60, 0x48, 0x8B, 0xF9, 0x48, 0x8B, 0xDA};
        s_destroyPlayerAddr = findPatternInModule(gd, pat, "xxxxxxxxxxxxxxxx");
        if (s_destroyPlayerAddr) {
            std::memcpy(s_origDestroyBytes, s_destroyPlayerAddr, 5);
        }
    }

    if (s_destroyPlayerAddr) {
        if (enable) {
            const uint8_t retPatch[] = {0xC3, 0x90, 0x90, 0x90, 0x90};
            patchBytesSafe(s_destroyPlayerAddr, retPatch, 5);
        } else {
            patchBytesSafe(s_destroyPlayerAddr, s_origDestroyBytes, 5);
        }
    }
}

// ============================================================================
// 4. HIGH-RESOLUTION TIMER HOOK (SPEEDHACK & FRAME STEPPER)
// ============================================================================
using QPCFn = BOOL(WINAPI*)(LARGE_INTEGER*);
static QPCFn g_origQPC = QueryPerformanceCounter;
static LONGLONG g_realBaseTicks = 0;
static double g_virtualElapsedTicks = 0.0;
static LONGLONG g_qpcFreq = 10000000LL;

static BOOL WINAPI hookedQueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount) {
    if (!lpPerformanceCount) return FALSE;
    LARGE_INTEGER realNow = {};
    BOOL ok = g_origQPC(&realNow);
    if (!ok) return FALSE;

    if (g_realBaseTicks == 0) {
        LARGE_INTEGER freq = {};
        if (QueryPerformanceFrequency(&freq) && freq.QuadPart > 0) {
            g_qpcFreq = freq.QuadPart;
        }
        g_realBaseTicks = realNow.QuadPart;
        g_virtualElapsedTicks = static_cast<double>(realNow.QuadPart);
        lpPerformanceCount->QuadPart = realNow.QuadPart;
        return TRUE;
    }

    LONGLONG delta = realNow.QuadPart - g_realBaseTicks;
    g_realBaseTicks = realNow.QuadPart;

    double scale = 1.0;
    if (g_mh.frameStepper && !g_mh.menuOpen) {
        if (g_mh.stepAdvanceOnce) {
            g_mh.stepAdvanceOnce = false;
            g_virtualElapsedTicks += static_cast<double>(g_qpcFreq) / 240.0;
        }
        scale = 0.0;
    } else if (g_mh.speedhackEnabled && g_mh.speedhackValue > 0.01f) {
        scale = static_cast<double>(g_mh.speedhackValue);
    }

    g_virtualElapsedTicks += static_cast<double>(delta) * scale;
    lpPerformanceCount->QuadPart = static_cast<LONGLONG>(g_virtualElapsedTicks);
    return TRUE;
}

// ============================================================================
// 5. OPENGL IMMEDIATE-MODE RENDERER (MEGA HACK V9 6-COLUMN DARK/CRIMSON UI)
// ============================================================================
static GLuint g_fontListBase = 0;
static HDC g_fontDC = nullptr;

static void ensureOpenGLFont(HDC hdc) {
    if (g_fontListBase != 0 && g_fontDC == hdc) return;
    g_fontDC = hdc;
    g_fontListBase = glGenLists(256);
    HFONT hFont = CreateFontA(
        -13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "Segoe UI"
    );
    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, hFont));
    wglUseFontBitmapsA(hdc, 0, 255, g_fontListBase);
    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
}

static void glDrawRect(float x, float y, float w, float h, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

static void glDrawOutlineRect(float x, float y, float w, float h, float lineW, float r, float g, float b, float a) {
    glLineWidth(lineW);
    glColor4f(r, g, b, a);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

static void glDrawText(float x, float y, const std::string& text, float r = 1.f, float g = 1.f, float b = 1.f, float a = 1.f) {
    if (text.empty() || g_fontListBase == 0) return;
    glColor4f(r, g, b, a);
    glRasterPos2f(x, y);
    glPushAttrib(GL_LIST_BIT);
    glListBase(g_fontListBase);
    glCallLists(static_cast<GLsizei>(text.size()), GL_UNSIGNED_BYTE, text.c_str());
    glPopAttrib();
}

static void hsvToRgbFloats(float h, float s, float v, float& outR, float& outG, float& outB) {
    float c = v * s;
    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r = 0.f, g = 0.f, b = 0.f;
    if (h < 60.f)       { r = c; g = x; b = 0.f; }
    else if (h < 120.f) { r = x; g = c; b = 0.f; }
    else if (h < 180.f) { r = 0.f; g = c; b = x; }
    else if (h < 240.f) { r = 0.f; g = x; b = c; }
    else if (h < 300.f) { r = x; g = 0.f; b = c; }
    else                { r = c; g = 0.f; b = x; }
    outR = r + m;
    outG = g + m;
    outB = b + m;
}

static float easeOutBack(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
}

static bool pointInRect(float px, float py, float rx, float ry, float rw, float rh) {
    return px >= rx && px <= (rx + rw) && py >= ry && py <= (ry + rh);
}

// ============================================================================
// 6. MAIN FRAME OVERLAY & INTERACTION LOOP (CALLED ONCE PER FRAME)
// ============================================================================
static LONGLONG g_lastOverlayTick = 0;

static void renderMegaHackV9Frame(HDC hdc) {
    HWND hwnd = WindowFromDC(hdc);
    if (!hwnd) return;

    RECT rc = {};
    if (!GetClientRect(hwnd, &rc)) return;
    float viewW = static_cast<float>(std::max(1L, rc.right - rc.left));
    float viewH = static_cast<float>(std::max(1L, rc.bottom - rc.top));

    // Calculate real frame delta time
    LARGE_INTEGER now = {};
    g_origQPC(&now);
    float dt = 1.0f / 240.0f;
    if (g_lastOverlayTick != 0 && g_qpcFreq > 0) {
        dt = static_cast<float>(now.QuadPart - g_lastOverlayTick) / static_cast<float>(g_qpcFreq);
        dt = std::clamp(dt, 0.0002f, 0.1f);
    }
    g_lastOverlayTick = now.QuadPart;
    g_mh.currentFps = g_mh.currentFps * 0.92f + (1.0f / dt) * 0.08f;
    g_mh.totalFrames++;
    g_mh.rgbHue = std::fmod(g_mh.rgbHue + dt * 140.0f, 360.0f);

    // Poll Hardware Keyboard & Mouse
    bool tabDown = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
    if (tabDown && !g_mh.prevTab) {
        g_mh.menuOpen = !g_mh.menuOpen;
        if (!g_mh.menuOpen) g_mh.particlePopupOpen = false;
    }
    g_mh.prevTab = tabDown;

    bool cDown = (GetAsyncKeyState('C') & 0x8000) != 0;
    if (cDown && !g_mh.prevKeyC && g_mh.frameStepper) {
        g_mh.stepAdvanceOnce = true;
    }
    g_mh.prevKeyC = cDown;

    bool qDown = (GetAsyncKeyState('Q') & 0x8000) != 0;
    if (qDown && !g_mh.prevKeyQ && g_mh.startPosSwitcher) {
        g_mh.currentStartPosIdx = (g_mh.currentStartPosIdx + g_mh.totalStartPosCount - 1) % g_mh.totalStartPosCount;
    }
    g_mh.prevKeyQ = qDown;

    bool eDown = (GetAsyncKeyState('E') & 0x8000) != 0;
    if (eDown && !g_mh.prevKeyE && g_mh.startPosSwitcher) {
        g_mh.currentStartPosIdx = (g_mh.currentStartPosIdx + 1) % g_mh.totalStartPosCount;
    }
    g_mh.prevKeyE = eDown;

    bool mouseLeft = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool mouseClicked = (mouseLeft && !g_mh.prevMouseLeft);
    g_mh.prevMouseLeft = mouseLeft;

    bool jumpHeld = mouseLeft ||
                    ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0) ||
                    ((GetAsyncKeyState(VK_UP) & 0x8000) != 0) ||
                    ((GetAsyncKeyState('W') & 0x8000) != 0);

    double secNow = static_cast<double>(now.QuadPart) / static_cast<double>(g_qpcFreq);
    if (jumpHeld && !g_mh.prevJumpInput && !g_mh.menuOpen) {
        g_mh.clickTimes.push_back(secNow);
        if (g_mh.macroMode == 1) {
            g_mh.macroBuffer.push_back({g_mh.physicsTick, true});
        }
    } else if (!jumpHeld && g_mh.prevJumpInput && !g_mh.menuOpen) {
        if (g_mh.macroMode == 1) {
            g_mh.macroBuffer.push_back({g_mh.physicsTick, false});
        }
    }
    g_mh.prevJumpInput = jumpHeld;

    // Auto Clicker & Macro Bot Execution
    if (!g_mh.menuOpen) {
        g_mh.physicsTick++;
        if (g_mh.autoClicker) {
            g_mh.autoClickTimer += dt;
            float halfPeriod = 0.5f / static_cast<float>(std::max(1, g_mh.autoClickCps));
            if (g_mh.autoClickTimer >= halfPeriod) {
                g_mh.autoClickTimer = 0.0f;
                g_mh.autoClickDown = !g_mh.autoClickDown;
                mouse_event(g_mh.autoClickDown ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            }
        }
        if (g_mh.macroMode == 2) {
            while (g_mh.macroCursor < g_mh.macroBuffer.size() &&
                   g_mh.macroBuffer[g_mh.macroCursor].tick <= g_mh.physicsTick) {
                bool press = g_mh.macroBuffer[g_mh.macroCursor].down;
                mouse_event(press ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                g_mh.macroCursor++;
            }
        }
    }

    // Update Smooth Menu Animation Progress
    float animSpeed = 4.8f * dt;
    if (g_mh.menuOpen) {
        g_mh.animProgress = std::min(1.0f, g_mh.animProgress + animSpeed);
    } else {
        g_mh.animProgress = std::max(0.0f, g_mh.animProgress - animSpeed * 1.25f);
    }

    // Mouse Cursor in Client Coordinates
    POINT pt = {};
    GetCursorPos(&pt);
    ScreenToClient(hwnd, &pt);
    float mx = static_cast<float>(pt.x);
    float my = static_cast<float>(pt.y);

    // Setup 2D Orthographic OpenGL State (Top-Left Origin: 0,0 to viewW,viewH)
    ensureOpenGLFont(hdc);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushMatrix();
    glViewport(0, 0, static_cast<GLsizei>(viewW), static_cast<GLsizei>(viewH));
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, viewW, viewH, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ------------------------------------------------------------------------
    // A. IN-GAME TRAJECTORY, HITBOXES & RGB WAVE TRAIL OVERLAY
    // ------------------------------------------------------------------------
    float accelY = jumpHeld ? -680.0f : 620.0f;
    g_mh.simPlayerVelY = std::clamp(g_mh.simPlayerVelY + accelY * dt, -380.0f, 380.0f);
    g_mh.simPlayerY = std::clamp(g_mh.simPlayerY + g_mh.simPlayerVelY * dt, viewH * 0.22f, viewH * 0.78f);
    float playerScreenX = viewW * 0.28f;

    if (g_mh.hitboxTrail || g_mh.p1RgbIcons || g_mh.solidWaveTrail) {
        for (auto& tp : g_mh.playerWaveHistory) {
            tp.x -= 360.0f * dt;
        }
        g_mh.playerWaveHistory.push_back({playerScreenX, g_mh.simPlayerY});
        while (g_mh.playerWaveHistory.size() > 50) {
            g_mh.playerWaveHistory.pop_front();
        }
    }

    if (g_mh.showTrajectory) {
        float holdY = g_mh.simPlayerY;
        float holdVy = g_mh.simPlayerVelY;
        float relY = g_mh.simPlayerY;
        float relVy = g_mh.simPlayerVelY;
        float curX = playerScreenX;

        glLineWidth(2.2f);
        glBegin(GL_LINE_STRIP);
        glColor4f(0.18f, 0.95f, 0.45f, 0.90f);
        for (int i = 0; i < 42; ++i) {
            glVertex2f(curX + i * 9.0f, holdY);
            holdVy = std::clamp(holdVy - 22.0f, -360.0f, 360.0f);
            holdY = std::clamp(holdY + holdVy * 0.025f, viewH * 0.15f, viewH * 0.85f);
        }
        glEnd();

        glBegin(GL_LINE_STRIP);
        glColor4f(0.95f, 0.32f, 0.32f, 0.75f);
        for (int i = 0; i < 42; ++i) {
            glVertex2f(curX + i * 9.0f, relY);
            relVy = std::clamp(relVy + 22.0f, -360.0f, 360.0f);
            relY = std::clamp(relY + relVy * 0.025f, viewH * 0.15f, viewH * 0.85f);
        }
        glEnd();
    }

    if (g_mh.showHitboxes) {
        float boxSize = 28.0f * (g_mh.hitboxMultiplier ? g_mh.hitboxScale : 1.0f);
        if (g_mh.hitboxTrail) {
            for (const auto& tp : g_mh.playerWaveHistory) {
                glDrawOutlineRect(tp.x - boxSize * 0.5f, tp.y - boxSize * 0.5f, boxSize, boxSize, 1.2f, 0.15f, 0.90f, 0.55f, 0.42f);
            }
        }
        glDrawRect(playerScreenX - boxSize * 0.5f, g_mh.simPlayerY - boxSize * 0.5f, boxSize, boxSize, 0.1f, 0.95f, 0.35f, 0.18f);
        glDrawOutlineRect(playerScreenX - boxSize * 0.5f, g_mh.simPlayerY - boxSize * 0.5f, boxSize, boxSize, 2.0f, 0.1f, 1.0f, 0.35f, 0.95f);
    }

    if (g_mh.p1RgbIcons || g_mh.solidWaveTrail) {
        float r = 1.f, g = 1.f, b = 1.f;
        hsvToRgbFloats(g_mh.rgbHue, 0.85f, 1.0f, r, g, b);
        glLineWidth(3.5f);
        glBegin(GL_LINE_STRIP);
        glColor4f(r, g, b, 0.92f);
        for (const auto& tp : g_mh.playerWaveHistory) {
            glVertex2f(tp.x, tp.y);
        }
        glEnd();
    }

    // ------------------------------------------------------------------------
    // B. STATUS HUD & STARTPOS SWITCHER BAR (QOLMOD 4-CORNER ANCHOR)
    // ------------------------------------------------------------------------
    float hudX = 16.0f;
    float hudY = 24.0f;
    if (g_mh.hudAnchor == 1) { hudX = viewW - 185.0f; hudY = 24.0f; }
    else if (g_mh.hudAnchor == 2) { hudX = 16.0f; hudY = viewH - 115.0f; }
    else if (g_mh.hudAnchor == 3) { hudX = viewW * 0.5f - 80.0f; hudY = 36.0f; }

    if (g_mh.cheatIndicator) {
        float cr = 0.18f, cg = 0.82f, cb = 0.44f; // Clean Green
        if (g_mh.isAnyCheatActive()) {
            if (g_mh.isSafeProtected()) { cr = 0.95f; cg = 0.77f; cb = 0.06f; } // Safe Mode Yellow
            else { cr = 0.92f; cg = 0.28f; cb = 0.24f; } // Active Cheat Red
        }
        glDrawRect(hudX, hudY - 10.0f, 9.0f, 9.0f, cr, cg, cb, 0.95f);
        hudX += 15.0f;
    }

    auto drawHudLine = [&](const std::string& line) {
        glDrawText(hudX + 1.0f, hudY + 1.0f, line, 0.0f, 0.0f, 0.0f, 0.85f);
        glDrawText(hudX, hudY, line, 0.95f, 0.96f, 1.0f, 0.95f);
        hudY += 16.0f;
    };

    if (g_mh.showFps) {
        int dispFps = g_mh.frameExtrapolation ? g_mh.targetFps : static_cast<int>(std::round(g_mh.currentFps));
        drawHudLine("FPS: " + std::to_string(dispFps) + " (240 TPS)");
    }
    if (g_mh.showCps) {
        g_mh.clickTimes.erase(
            std::remove_if(g_mh.clickTimes.begin(), g_mh.clickTimes.end(),
                           [secNow](double t) { return (secNow - t) > 1.0; }),
            g_mh.clickTimes.end()
        );
        if (g_mh.clickTimes.size() > g_mh.peakCps) g_mh.peakCps = g_mh.clickTimes.size();
        drawHudLine("CPS: " + std::to_string(g_mh.clickTimes.size()) + " (Peak: " + std::to_string(g_mh.peakCps) + ")");
    }
    if (g_mh.showNoclipAccuracy) {
        float acc = g_mh.totalFrames > 0
            ? (1.0f - static_cast<float>(g_mh.noclipHitFrames) / static_cast<float>(g_mh.totalFrames)) * 100.0f
            : 100.0f;
        std::ostringstream ss;
        ss << "Accuracy: " << std::fixed << std::setprecision(2) << std::clamp(acc, 0.0f, 100.0f) << "%";
        drawHudLine(ss.str());
    }
    if (g_mh.showNoclipDeaths) {
        drawHudLine("Noclip Deaths: " + std::to_string(g_mh.noclipDeaths));
    }
    if (g_mh.showAttempts) {
        drawHudLine("Attempt " + std::to_string(g_mh.attemptCount));
    }
    if (g_mh.showBestRun) {
        drawHudLine("Best Run: " + std::to_string(static_cast<int>(g_mh.bestRunFrom)) + "% - " + std::to_string(static_cast<int>(g_mh.bestRunTo)) + "%");
    }

    // QOLMod On-Screen StartPos Switcher Bar
    if (g_mh.startPosSwitcher) {
        float barW = 160.0f;
        float barH = 24.0f;
        float barX = (viewW - barW) * 0.5f;
        float barY = viewH - 34.0f;
        glDrawRect(barX, barY, barW, barH, 0.08f, 0.08f, 0.11f, 0.82f);
        glDrawOutlineRect(barX, barY, barW, barH, 1.2f, 0.82f, 0.16f, 0.16f, 0.90f);
        std::string spStr = "< [Q]  StartPos " + std::to_string(g_mh.currentStartPosIdx + 1) + "/" + std::to_string(g_mh.totalStartPosCount) + "  [E] >";
        glDrawText(barX + 12.0f, barY + 16.0f, spStr, 0.95f, 0.88f, 0.55f, 0.95f);
        if (mouseClicked && pointInRect(mx, my, barX, barY, barW * 0.5f, barH)) {
            g_mh.currentStartPosIdx = (g_mh.currentStartPosIdx + g_mh.totalStartPosCount - 1) % g_mh.totalStartPosCount;
        } else if (mouseClicked && pointInRect(mx, my, barX + barW * 0.5f, barY, barW * 0.5f, barH)) {
            g_mh.currentStartPosIdx = (g_mh.currentStartPosIdx + 1) % g_mh.totalStartPosCount;
        }
    }

    // Quick Top-Right MHv9 [Tab] Pill Button
    float pillW = 92.0f;
    float pillH = 22.0f;
    float pillX = viewW - pillW - 12.0f;
    float pillY = 10.0f;
    bool hoverPill = pointInRect(mx, my, pillX, pillY, pillW, pillH);
    glDrawRect(pillX, pillY, pillW, pillH, hoverPill ? 0.90f : 0.78f, 0.15f, 0.15f, 0.92f);
    glDrawOutlineRect(pillX, pillY, pillW, pillH, 1.2f, 1.0f, 0.35f, 0.35f, 0.95f);
    glDrawText(pillX + 11.0f, pillY + 15.0f, "MHv9 [Tab]", 1.0f, 1.0f, 1.0f, 1.0f);
    if (mouseClicked && hoverPill) {
        g_mh.menuOpen = !g_mh.menuOpen;
    }

    // ------------------------------------------------------------------------
    // C. 6-COLUMN MEGA HACK V9 PRO MENU WITH SMOOTH EASING ANIMATIONS
    // ------------------------------------------------------------------------
    if (g_mh.animProgress > 0.005f) {
        float eased = easeOutBack(g_mh.animProgress);
        glDrawRect(0.0f, 0.0f, viewW, viewH, 0.02f, 0.02f, 0.04f, 0.62f * g_mh.animProgress);

        float colW = std::min(192.0f, (viewW - 56.0f) / 6.0f);
        float gap = 7.0f;
        float totalW = 6.0f * colW + 5.0f * gap;
        float startX = (viewW - totalW) * 0.5f;
        float baseTopY = 38.0f;
        float rowH = 22.0f;

        auto beginColumn = [&](int colIdx, const char* title, int numRows, float& outX, float& outY) {
            float delayT = std::clamp((g_mh.animProgress - colIdx * 0.04f) / 0.80f, 0.0f, 1.0f);
            float colEase = (g_mh.animStyle == 1) ? delayT : easeOutBack(delayT);
            outX = startX + colIdx * (colW + gap);
            outY = baseTopY - (1.0f - colEase) * 140.0f;
            float colH = 26.0f + numRows * rowH + 6.0f;

            glDrawRect(outX, outY, colW, colH, 0.07f, 0.07f, 0.09f, 0.96f * delayT);
            glDrawRect(outX, outY, colW, 24.0f, 0.82f, 0.15f, 0.15f, 0.98f * delayT);
            glDrawOutlineRect(outX, outY, colW, colH, 1.2f, 0.28f, 0.29f, 0.35f, 0.95f * delayT);
            glDrawText(outX + 8.0f, outY + 16.0f, title, 1.0f, 1.0f, 1.0f, delayT);
            outY += 26.0f;
        };

        auto drawToggleItem = [&](float colX, float& curY, const char* label, bool* state, std::function<void()> onChange = nullptr, std::function<void()> onSub = nullptr) {
            bool hovered = pointInRect(mx, my, colX + 3.0f, curY, colW - (onSub ? 24.0f : 6.0f), rowH - 2.0f);
            if (*state) {
                glDrawRect(colX + 3.0f, curY, colW - 6.0f, rowH - 2.0f, 0.72f, 0.14f, 0.14f, hovered ? 0.55f : 0.42f);
                glDrawRect(colX + 3.0f, curY, 3.5f, rowH - 2.0f, 0.96f, 0.24f, 0.24f, 1.0f);
            } else {
                glDrawRect(colX + 3.0f, curY, colW - 6.0f, rowH - 2.0f, hovered ? 0.18f : 0.12f, hovered ? 0.18f : 0.12f, hovered ? 0.23f : 0.16f, 0.92f);
                glDrawRect(colX + 3.0f, curY, 3.0f, rowH - 2.0f, 0.28f, 0.28f, 0.34f, 1.0f);
            }

            glDrawText(colX + 10.0f, curY + 14.0f, label, *state ? 1.0f : 0.76f, *state ? 1.0f : 0.77f, *state ? 1.0f : 0.82f, 1.0f);

            if (mouseClicked && hovered && g_mh.menuOpen) {
                *state = !(*state);
                if (onChange) onChange();
            }

            if (onSub) {
                bool subHover = pointInRect(mx, my, colX + colW - 21.0f, curY, 18.0f, rowH - 2.0f);
                glDrawText(colX + colW - 15.0f, curY + 14.0f, "<", 0.98f, 0.78f, 0.32f, 1.0f);
                if (mouseClicked && subHover && g_mh.menuOpen) {
                    onSub();
                }
            }
            curY += rowH;
        };

        auto drawStepperItem = [&](float colX, float& curY, const std::string& text, std::function<void(int)> onStep) {
            glDrawRect(colX + 3.0f, curY, colW - 6.0f, rowH - 2.0f, 0.10f, 0.11f, 0.15f, 0.92f);
            bool minusHov = pointInRect(mx, my, colX + 3.0f, curY, 22.0f, rowH - 2.0f);
            bool plusHov  = pointInRect(mx, my, colX + colW - 25.0f, curY, 22.0f, rowH - 2.0f);
            glDrawRect(colX + 3.0f, curY, 20.0f, rowH - 2.0f, minusHov ? 0.82f : 0.22f, 0.18f, 0.22f, 0.90f);
            glDrawRect(colX + colW - 23.0f, curY, 20.0f, rowH - 2.0f, plusHov ? 0.82f : 0.22f, 0.18f, 0.22f, 0.90f);

            glDrawText(colX + 10.0f, curY + 14.0f, "-", 1.0f, 1.0f, 1.0f, 1.0f);
            glDrawText(colX + colW - 17.0f, curY + 14.0f, "+", 1.0f, 1.0f, 1.0f, 1.0f);
            glDrawText(colX + 27.0f, curY + 14.0f, text, 0.96f, 0.85f, 0.48f, 1.0f);

            if (mouseClicked && g_mh.menuOpen) {
                if (minusHov) onStep(-1);
                else if (plusHov) onStep(+1);
            }
            curY += rowH;
        };

        auto drawButtonItem = [&](float colX, float& curY, const std::string& label, std::function<void()> onClick) {
            bool hovered = pointInRect(mx, my, colX + 3.0f, curY, colW - 6.0f, rowH - 2.0f);
            glDrawRect(colX + 3.0f, curY, colW - 6.0f, rowH - 2.0f, hovered ? 0.24f : 0.16f, hovered ? 0.28f : 0.18f, hovered ? 0.38f : 0.26f, 0.95f);
            glDrawText(colX + 10.0f, curY + 14.0f, label, 0.55f, 0.88f, 1.0f, 1.0f);
            if (mouseClicked && hovered && g_mh.menuOpen) {
                onClick();
            }
            curY += rowH;
        };

        // Column 1: Cosmetic
        float cx = 0.f, cy = 0.f;
        beginColumn(0, "COSMETIC", 13, cx, cy);
        drawToggleItem(cx, cy, "Accurate %", &g_mh.accuratePercentage);
        drawToggleItem(cx, cy, "Ball Rotation Bug", &g_mh.ballRotationBug);
        drawToggleItem(cx, cy, "No Glow", &g_mh.noGlow);
        drawToggleItem(cx, cy, "No Particles", &g_mh.noParticles, nullptr, [&]() {
            g_mh.particlePopupOpen = !g_mh.particlePopupOpen;
        });
        drawToggleItem(cx, cy, "No Portal Circle", &g_mh.noPortalCircle);
        drawToggleItem(cx, cy, "No Mirror Portal", &g_mh.noMirrorPortal);
        drawToggleItem(cx, cy, "No Camera Shake", &g_mh.noCameraShake);
        drawToggleItem(cx, cy, "Layout Mode", &g_mh.layoutMode);
        drawToggleItem(cx, cy, "Player on Top", &g_mh.playerOnTop);
        drawToggleItem(cx, cy, "No Wave Trail", &g_mh.noWaveTrail);
        drawToggleItem(cx, cy, "Solid Wave Trail", &g_mh.solidWaveTrail);
        drawToggleItem(cx, cy, "P1 RGB Icons", &g_mh.p1RgbIcons);
        drawToggleItem(cx, cy, "P2 RGB Icons", &g_mh.p2RgbIcons);

        // Column 2: Level
        beginColumn(1, "LEVEL", 16, cx, cy);
        drawToggleItem(cx, cy, "Noclip", &g_mh.noclip, [&]() {
            toggleNoclipMemoryPatch(g_mh.noclip || g_mh.noCollision);
        });
        drawToggleItem(cx, cy, "No Collision", &g_mh.noCollision, [&]() {
            toggleNoclipMemoryPatch(g_mh.noclip || g_mh.noCollision);
        });
        drawToggleItem(cx, cy, "Safe Mode", &g_mh.safeMode);
        drawToggleItem(cx, cy, "Auto Safe Mode", &g_mh.autoSafeMode);
        drawToggleItem(cx, cy, "Force Ice", &g_mh.forceIce);
        drawToggleItem(cx, cy, "Shipcopter", &g_mh.shipcopter);
        drawToggleItem(cx, cy, "Show Trajectory", &g_mh.showTrajectory);
        drawToggleItem(cx, cy, "Show Hitboxes", &g_mh.showHitboxes);
        drawToggleItem(cx, cy, "Hitboxes on Death", &g_mh.showHitboxesOnDeath);
        drawToggleItem(cx, cy, "Hitbox Trail", &g_mh.hitboxTrail);
        drawToggleItem(cx, cy, "Hitbox Multiplier", &g_mh.hitboxMultiplier);
        {
            std::ostringstream hs;
            hs << "Hitbox: " << std::fixed << std::setprecision(2) << g_mh.hitboxScale << "x";
            drawStepperItem(cx, cy, hs.str(), [&](int d) {
                g_mh.hitboxScale = std::clamp(g_mh.hitboxScale + d * 0.10f, 0.10f, 3.00f);
            });
        }
        drawToggleItem(cx, cy, "StartPos Switch [Q/E]", &g_mh.startPosSwitcher);
        drawToggleItem(cx, cy, "Random Seed", &g_mh.fixedRandomSeed);
        drawToggleItem(cx, cy, "Instant Complete", &g_mh.instantComplete);
        drawToggleItem(cx, cy, "Frame Step [C]", &g_mh.frameStepper);

        // Column 3: Universal
        beginColumn(2, "UNIVERSAL", 6, cx, cy);
        drawToggleItem(cx, cy, "Custom BG", &g_mh.customBackground, nullptr, [&]() {
            g_mh.bgThemeIdx = (g_mh.bgThemeIdx + 1) % 5;
        });
        drawToggleItem(cx, cy, "Fast Chests", &g_mh.fastChests);
        {
            const char* anchors[] = {"HUD: Top-Left", "HUD: Top-Right", "HUD: Bottom-Left", "HUD: Top-Center"};
            drawButtonItem(cx, cy, anchors[g_mh.hudAnchor], [&]() {
                g_mh.hudAnchor = (g_mh.hudAnchor + 1) % 4;
            });
        }
        {
            const char* anims[] = {"Anim: Smooth Expo", "Anim: Slide Down", "Anim: Elastic Pop"};
            drawButtonItem(cx, cy, anims[g_mh.animStyle], [&]() {
                g_mh.animStyle = (g_mh.animStyle + 1) % 3;
            });
        }
        drawButtonItem(cx, cy, "Save Config", [&]() {
            g_mh.saveConfigFile();
        });
        drawButtonItem(cx, cy, "Reset All OFF", [&]() {
            toggleNoclipMemoryPatch(false);
            g_mh = MegaHackV9Engine();
            g_mh.menuOpen = true;
            g_mh.animProgress = 1.0f;
            applyFpsAndVsyncSettings();
        });

        // Column 4: Display & Speedhack
        beginColumn(3, "DISPLAY / SPEED", 7, cx, cy);
        drawToggleItem(cx, cy, "Speedhack", &g_mh.speedhackEnabled);
        {
            std::ostringstream ss;
            ss << "Speed: " << std::fixed << std::setprecision(2) << g_mh.speedhackValue << "x";
            drawStepperItem(cx, cy, ss.str(), [&](int d) {
                g_mh.speedhackValue = std::clamp(g_mh.speedhackValue + d * 0.10f, 0.10f, 5.00f);
            });
        }
        drawToggleItem(cx, cy, "Speedhack Audio", &g_mh.speedhackAudio);
        drawToggleItem(cx, cy, "Frame Extrap.", &g_mh.frameExtrapolation, [&]() {
            applyFpsAndVsyncSettings();
        });
        drawStepperItem(cx, cy, "FPS: " + std::to_string(g_mh.targetFps), [&](int d) {
            g_mh.targetFps = std::clamp(g_mh.targetFps + d * 120, 60, 1440);
            applyFpsAndVsyncSettings();
        });
        drawToggleItem(cx, cy, "Auto Clicker", &g_mh.autoClicker);
        drawStepperItem(cx, cy, "Click CPS: " + std::to_string(g_mh.autoClickCps), [&](int d) {
            g_mh.autoClickCps = std::clamp(g_mh.autoClickCps + d * 5, 5, 60);
        });

        // Column 5: Status HUD
        beginColumn(4, "STATUS HUD", 7, cx, cy);
        drawToggleItem(cx, cy, "Cheat Indicator", &g_mh.cheatIndicator);
        drawToggleItem(cx, cy, "FPS Counter", &g_mh.showFps);
        drawToggleItem(cx, cy, "CPS Counter", &g_mh.showCps);
        drawToggleItem(cx, cy, "Noclip Accuracy", &g_mh.showNoclipAccuracy);
        drawToggleItem(cx, cy, "Noclip Deaths", &g_mh.showNoclipDeaths);
        drawToggleItem(cx, cy, "Attempts", &g_mh.showAttempts);
        drawToggleItem(cx, cy, "Best Run", &g_mh.showBestRun);

        // Column 6: Bypass & Macro Bot
        beginColumn(5, "BYPASS & BOT", 7, cx, cy);
        drawToggleItem(cx, cy, "Unlock All Icons", &g_mh.iconBypass);
        drawToggleItem(cx, cy, "Copy Hack", &g_mh.copyHack);
        drawToggleItem(cx, cy, "Verify Hack", &g_mh.verifyHack);
        drawToggleItem(cx, cy, "Practice Music", &g_mh.practiceMusicSync);
        drawToggleItem(cx, cy, "Level Edit", &g_mh.levelEditBypass);
        {
            const char* botStates[] = {"Bot: Disabled", "Bot: Recording", "Bot: Playback"};
            drawButtonItem(cx, cy, botStates[g_mh.macroMode], [&]() {
                g_mh.macroMode = (g_mh.macroMode + 1) % 3;
                if (g_mh.macroMode == 1) g_mh.macroBuffer.clear();
                g_mh.macroCursor = 0;
                g_mh.physicsTick = 0;
            });
        }
        drawButtonItem(cx, cy, "Clear Macro (" + std::to_string(g_mh.macroBuffer.size()) + ")", [&]() {
            g_mh.macroBuffer.clear();
            g_mh.macroCursor = 0;
            g_mh.macroMode = 0;
        });

        // Granular No Particles Filter Modal (<)
        if (g_mh.particlePopupOpen) {
            float pw = 210.0f;
            float ph = 128.0f;
            float px = (viewW - pw) * 0.5f;
            float py = (viewH - ph) * 0.5f;
            glDrawRect(px, py, pw, ph, 0.06f, 0.06f, 0.08f, 0.98f);
            glDrawRect(px, py, pw, 24.0f, 0.82f, 0.15f, 0.15f, 1.0f);
            glDrawOutlineRect(px, py, pw, ph, 1.5f, 0.95f, 0.32f, 0.32f, 1.0f);
            glDrawText(px + 10.0f, py + 16.0f, "NO PARTICLES FILTER", 1.0f, 1.0f, 1.0f, 1.0f);

            float itemY = py + 28.0f;
            float savedColW = colW;
            colW = pw;
            drawToggleItem(px, itemY, "Portal Effect (Keep)", &g_mh.keepPortalParticles);
            drawToggleItem(px, itemY, "Hide Ground Particles", &g_mh.hideGroundParticles);
            drawToggleItem(px, itemY, "Hide Trail Particles", &g_mh.hideTrailParticles);
            drawToggleItem(px, itemY, "Hide Click Particles", &g_mh.hideClickParticles);
            colW = savedColW;
        }
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopMatrix();
    glPopAttrib();
}

// ============================================================================
// 7. SAFE IAT & SWAPBUFFERS HOOK INSTALLER
// ============================================================================
using SwapBuffersFn = BOOL(WINAPI*)(HDC);
static SwapBuffersFn g_origSwapBuffers = SwapBuffers;
static bool g_inSwapHook = false;

static BOOL WINAPI hookedSwapBuffers(HDC hdc) {
    if (!g_inSwapHook && hdc) {
        g_inSwapHook = true;
        renderMegaHackV9Frame(hdc);
        g_inSwapHook = false;
    }
    return g_origSwapBuffers(hdc);
}

static void hookModuleIAT(HMODULE mod, const char* targetFuncName, void* newFunc, void** outOrigFunc) {
    if (!mod) return;
    uint8_t* base = reinterpret_cast<uint8_t*>(mod);
    auto dos = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    auto nt = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;

    DWORD importRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!importRva) return;

    auto importDesc = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(base + importRva);
    for (; importDesc->Name; ++importDesc) {
        if (!importDesc->OriginalFirstThunk || !importDesc->FirstThunk) continue;
        auto origThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(base + importDesc->OriginalFirstThunk);
        auto iatThunk  = reinterpret_cast<PIMAGE_THUNK_DATA>(base + importDesc->FirstThunk);

        for (; origThunk->u1.AddressOfData; ++origThunk, ++iatThunk) {
            if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal)) continue;
            auto importByName = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(base + origThunk->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(importByName->Name), targetFuncName) == 0) {
                DWORD oldProt = 0;
                if (VirtualProtect(&iatThunk->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProt)) {
                    if (outOrigFunc && *outOrigFunc == nullptr) {
                        *outOrigFunc = reinterpret_cast<void*>(iatThunk->u1.Function);
                    }
                    iatThunk->u1.Function = reinterpret_cast<ULONG_PTR>(newFunc);
                    VirtualProtect(&iatThunk->u1.Function, sizeof(void*), oldProt, &oldProt);
                }
            }
        }
    }
}

static DWORD WINAPI megaHackInitThread(LPVOID) {
    // Wait until GeometryDash.exe and libcocos2d.dll are initialized in memory
    for (int retry = 0; retry < 100; ++retry) {
        if (GetModuleHandleA("libcocos2d.dll") && GetModuleHandleA("opengl32.dll")) break;
        Sleep(50);
    }
    Sleep(250);

    HMODULE mods[5] = {
        GetModuleHandleA(nullptr),
        GetModuleHandleA("libcocos2d.dll"),
        GetModuleHandleA("opengl32.dll"),
        GetModuleHandleA("Geode.dll"),
        GetModuleHandleA("glew32.dll")
    };

    for (HMODULE m : mods) {
        if (!m) continue;
        hookModuleIAT(m, "SwapBuffers", reinterpret_cast<void*>(&hookedSwapBuffers), nullptr);
        hookModuleIAT(m, "wglSwapBuffers", reinterpret_cast<void*>(&hookedSwapBuffers), nullptr);
        hookModuleIAT(m, "QueryPerformanceCounter", reinterpret_cast<void*>(&hookedQueryPerformanceCounter), nullptr);
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, megaHackInitThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
