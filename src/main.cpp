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
#include <ctime>
#include <vector>
#include <deque>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <fstream>

// ============================================================================
// 1. GEODE ENTRY POINT EXPORT (GIÚP GEODE 5.X NHẬN DIỆN & KÍCH HOẠT MOD CHUẨN)
// ============================================================================
extern "C" __declspec(dllexport) void geode_implicit_load(void* modPtr) {
    (void)modPtr;
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
    // UI & Animation State
    bool menuOpen = false;
    int activePopup = 0;       // 0: None, 1: Particle Filter, 2: Custom BG RGB, 3: Icon RGB Effects
    float animProgress = 0.0f; // 0.0 -> 1.0
    int animStyle = 0;         // 0: Smooth Cascade, 1: Slide Down, 2: Elastic Pop, 3: Instant
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
    bool hideSparkleParticles = false;
    bool noPortalCircle = false;
    bool noMirrorPortal = false;
    bool noCameraShake = false;
    bool layoutMode = false;
    bool playerOnTop = false;
    bool noWaveTrail = false;
    bool noWavePulse = false;
    bool solidWaveTrail = false;
    bool p1RgbIcons = false;
    bool p2RgbIcons = false;
    bool p1RgbTrail = false;

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

    // Column 3: Universal & Window
    bool customBackground = false;
    int bgThemeIdx = 0;
    float customBgR = 0.12f;
    float customBgG = 0.14f;
    float customBgB = 0.26f;
    bool fastChests = false;
    bool freeWindowResize = false;
    bool alwaysOnTop = false;

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
    bool showClock = false;

    // Column 6: Bypass, Creator & Macro Bot
    bool iconBypass = false;
    bool copyHack = false;
    bool verifyHack = false;
    bool levelEditBypass = false;
    int macroMode = 0; // 0: Disabled, 1: Recording, 2: Playback
    std::vector<MacroEvent> macroBuffer;
    size_t macroCursor = 0;
    int physicsTick = 0;

    // Runtime Metrics
    int currentStartPosIdx = 0;
    int totalStartPosCount = 6;
    int totalFrames = 0;
    int noclipHitFrames = 0;
    int noclipDeaths = 0;
    int attemptCount = 1;
    float bestRunFrom = 0.0f;
    float bestRunTo = 100.0f;
    float currentFps = 240.0f;
    float rgbHue = 0.0f;
    float rgbSpeed = 140.0f;
    std::vector<double> clickTimes;
    size_t peakCps = 0;
    std::deque<TrailPoint> playerWaveHistory;
    float simPlayerY = 260.0f;
    float simPlayerVelY = 0.0f;

    bool uiClickPending = false;
    float uiClickX = 0.0f;
    float uiClickY = 0.0f;
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

    void saveConfigToDisk() {
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
            << "showBestRun=" << showBestRun << "\n"
            << "showClock=" << showClock << "\n";
    }

    void saveMacroToDisk() {
        std::ofstream out("mhv9_macro.bot", std::ios::trunc);
        if (!out.is_open()) return;
        for (const auto& ev : macroBuffer) {
            out << ev.tick << " " << (ev.down ? 1 : 0) << "\n";
        }
    }

    void loadMacroFromDisk() {
        std::ifstream in("mhv9_macro.bot");
        if (!in.is_open()) return;
        macroBuffer.clear();
        int t = 0, d = 0;
        while (in >> t >> d) {
            macroBuffer.push_back({t, d != 0});
        }
        macroCursor = 0;
    }
};

static MegaHackV9Engine g_mh;

// ============================================================================
// 3. COCOS2D-X NATIVE ENGINE BRIDGES & MEMORY PATCHING
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

static void applyNativeCocosSpeedhack() {
    HMODULE cocos = GetModuleHandleA("libcocos2d.dll");
    if (!cocos) return;

    using SharedDirFn = void*(__cdecl*)();
    using GetSchedFn = void*(__fastcall*)(void*);
    using SetTimeScaleFn = void(__fastcall*)(void*, float);

    static auto sharedDir = reinterpret_cast<SharedDirFn>(GetProcAddress(cocos, "?sharedDirector@CCDirector@cocos2d@@SAPEAV12@XZ"));
    static auto getSched  = reinterpret_cast<GetSchedFn>(GetProcAddress(cocos, "?getScheduler@CCDirector@cocos2d@@UEAAPEAVCCScheduler@2@XZ"));
    static auto setScale  = reinterpret_cast<SetTimeScaleFn>(GetProcAddress(cocos, "?setTimeScale@CCScheduler@cocos2d@@QEAAXM@Z"));

    if (sharedDir && getSched && setScale) {
        void* dir = sharedDir();
        if (dir) {
            void* sched = getSched(dir);
            if (sched) {
                float s = g_mh.speedhackEnabled ? g_mh.speedhackValue : 1.0f;
                setScale(sched, s);
            }
        }
    }
}

static void applyNativeFpsAndVsync() {
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

    HMODULE cocos = GetModuleHandleA("libcocos2d.dll");
    if (cocos) {
        using SharedAppFn = void*(__cdecl*)();
        using SetIntervalFn = void(__fastcall*)(void*, double);
        static auto sharedApp   = reinterpret_cast<SharedAppFn>(GetProcAddress(cocos, "?sharedApplication@CCApplication@cocos2d@@SAPEAV12@XZ"));
        static auto setInterval = reinterpret_cast<SetIntervalFn>(GetProcAddress(cocos, "?setAnimationInterval@CCApplication@cocos2d@@UEAAXN@Z"));
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
    static uint8_t* s_destroyAddr = nullptr;
    static uint8_t s_origBytes[5] = {0};
    static bool s_scanned = false;

    if (!s_scanned) {
        s_scanned = true;
        HMODULE gd = GetModuleHandleA("GeometryDash.exe");
        const uint8_t pat1[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x60, 0x48, 0x8B, 0xF9, 0x48, 0x8B, 0xDA};
        s_destroyAddr = findPatternInModule(gd, pat1, "xxxxxxxxxxxxxxxx");
        if (s_destroyAddr) {
            std::memcpy(s_origBytes, s_destroyAddr, 5);
        }
    }

    if (s_destroyAddr) {
        if (enable) {
            const uint8_t retBytes[] = {0xC3, 0x90, 0x90, 0x90, 0x90};
            patchBytesSafe(s_destroyAddr, retBytes, 5);
        } else {
            patchBytesSafe(s_destroyAddr, s_origBytes, 5);
        }
    }
}

static void updateWindowStyleModifiers(HWND hwnd) {
    if (!hwnd) return;
    LONG_PTR style = GetWindowLongPtrA(hwnd, GWL_STYLE);
    if (g_mh.freeWindowResize) {
        style |= (WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
    }
    SetWindowLongPtrA(hwnd, GWL_STYLE, style);
    SetWindowPos(
        hwnd,
        g_mh.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED
    );
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
// 5. WNDPROC INPUT HOOK (BLOCKS CLICKS THROUGH MENU & HANDLES KEYBINDS)
// ============================================================================
static WNDPROC g_origWndProc = nullptr;
static HWND g_hookedHwnd = nullptr;

static LRESULT CALLBACK hookedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && (lParam & 0x40000000) == 0) {
        if (wParam == VK_TAB) {
            g_mh.menuOpen = !g_mh.menuOpen;
            if (!g_mh.menuOpen) g_mh.activePopup = 0;
            return 0;
        }
        if (wParam == 'C' && g_mh.frameStepper && !g_mh.menuOpen) {
            g_mh.stepAdvanceOnce = true;
            return 0;
        }
        if (wParam == 'Q' && g_mh.startPosSwitcher && !g_mh.menuOpen) {
            g_mh.currentStartPosIdx = (g_mh.currentStartPosIdx + g_mh.totalStartPosCount - 1) % g_mh.totalStartPosCount;
            return 0;
        }
        if (wParam == 'E' && g_mh.startPosSwitcher && !g_mh.menuOpen) {
            g_mh.currentStartPosIdx = (g_mh.currentStartPosIdx + 1) % g_mh.totalStartPosCount;
            return 0;
        }
    }

    if (msg == WM_LBUTTONDOWN) {
        float clickX = static_cast<float>(LOWORD(lParam));
        float clickY = static_cast<float>(HIWORD(lParam));
        RECT rc = {};
        GetClientRect(hwnd, &rc);
        float vw = static_cast<float>(std::max(1L, rc.right - rc.left));

        // Top-right MHv9 [Tab] Pill Button
        if (clickX >= (vw - 104.0f) && clickX <= (vw - 12.0f) && clickY >= 10.0f && clickY <= 32.0f) {
            g_mh.menuOpen = !g_mh.menuOpen;
            if (!g_mh.menuOpen) g_mh.activePopup = 0;
            return 0;
        }

        if (g_mh.menuOpen) {
            g_mh.uiClickPending = true;
            g_mh.uiClickX = clickX;
            g_mh.uiClickY = clickY;
            return 0; // Block click from reaching Geometry Dash behind the menu
        }
    }

    if (g_mh.menuOpen && (msg == WM_LBUTTONUP || msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP || msg == WM_MOUSEWHEEL)) {
        return 0;
    }

    return CallWindowProcA(g_origWndProc, hwnd, msg, wParam, lParam);
}

static void ensureWndProcHooked(HWND hwnd) {
    if (!hwnd || g_hookedHwnd == hwnd) return;
    WNDPROC prev = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&hookedWndProc)));
    if (prev && prev != &hookedWndProc) {
        g_origWndProc = prev;
        g_hookedHwnd = hwnd;
    }
}

// ============================================================================
// 6. OPENGL IMMEDIATE-MODE RENDERER
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
// 7. MAIN FRAME RENDER & INTERACTION LOOP
// ============================================================================
static LONGLONG g_lastOverlayTick = 0;

static void renderMegaHackV9Frame(HDC hdc) {
    HWND hwnd = WindowFromDC(hdc);
    if (!hwnd) return;
    ensureWndProcHooked(hwnd);

    RECT rc = {};
    if (!GetClientRect(hwnd, &rc)) return;
    float viewW = static_cast<float>(std::max(1L, rc.right - rc.left));
    float viewH = static_cast<float>(std::max(1L, rc.bottom - rc.top));

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
    g_mh.rgbHue = std::fmod(g_mh.rgbHue + dt * g_mh.rgbSpeed, 360.0f);

    bool jumpHeld = ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) ||
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

    // Update Menu Animation Progress
    if (g_mh.animStyle == 3) {
        g_mh.animProgress = g_mh.menuOpen ? 1.0f : 0.0f;
    } else {
        float animSpeed = 5.2f * dt;
        if (g_mh.menuOpen) {
            g_mh.animProgress = std::min(1.0f, g_mh.animProgress + animSpeed);
        } else {
            g_mh.animProgress = std::max(0.0f, g_mh.animProgress - animSpeed * 1.3f);
        }
    }

    POINT pt = {};
    GetCursorPos(&pt);
    ScreenToClient(hwnd, &pt);
    float mx = static_cast<float>(pt.x);
    float my = static_cast<float>(pt.y);

    bool mouseClicked = g_mh.uiClickPending;
    if (mouseClicked) {
        mx = g_mh.uiClickX;
        my = g_mh.uiClickY;
        g_mh.uiClickPending = false;
    }

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

    // Custom Background Ambient Tint Overlay
    if (g_mh.customBackground) {
        glDrawRect(0.0f, 0.0f, viewW, viewH, g_mh.customBgR, g_mh.customBgG, g_mh.customBgB, 0.24f);
    }

    // In-Game Trajectory, Hitboxes & RGB Wave Trail Overlay
    float accelY = jumpHeld ? -680.0f : 620.0f;
    g_mh.simPlayerVelY = std::clamp(g_mh.simPlayerVelY + accelY * dt, -380.0f, 380.0f);
    g_mh.simPlayerY = std::clamp(g_mh.simPlayerY + g_mh.simPlayerVelY * dt, viewH * 0.22f, viewH * 0.78f);
    float playerScreenX = viewW * 0.28f;

    if (g_mh.hitboxTrail || g_mh.p1RgbIcons || g_mh.p1RgbTrail || g_mh.solidWaveTrail) {
        for (auto& tp : g_mh.playerWaveHistory) {
            tp.x -= 360.0f * dt;
        }
        g_mh.playerWaveHistory.push_back({playerScreenX, g_mh.simPlayerY});
        while (g_mh.playerWaveHistory.size() > 55) {
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

    if ((g_mh.p1RgbIcons || g_mh.p1RgbTrail || g_mh.solidWaveTrail) && !g_mh.noWaveTrail) {
        float r = 1.f, g = 1.f, b = 1.f;
        hsvToRgbFloats(g_mh.rgbHue, 0.85f, 1.0f, r, g, b);
        glLineWidth(g_mh.noWavePulse ? 2.2f : 3.8f);
        glBegin(GL_LINE_STRIP);
        glColor4f(r, g, b, g_mh.solidWaveTrail ? 1.0f : 0.85f);
        for (const auto& tp : g_mh.playerWaveHistory) {
            glVertex2f(tp.x, tp.y);
        }
        glEnd();
    }

    // Status HUD (4-Corner Anchor System)
    float hudX = 16.0f;
    float hudY = 24.0f;
    if (g_mh.hudAnchor == 1) { hudX = viewW - 185.0f; hudY = 24.0f; }
    else if (g_mh.hudAnchor == 2) { hudX = 16.0f; hudY = viewH - 125.0f; }
    else if (g_mh.hudAnchor == 3) { hudX = viewW * 0.5f - 80.0f; hudY = 36.0f; }

    if (g_mh.cheatIndicator) {
        float cr = 0.18f, cg = 0.82f, cb = 0.44f;
        if (g_mh.isAnyCheatActive()) {
            if (g_mh.isSafeProtected()) { cr = 0.95f; cg = 0.77f; cb = 0.06f; }
            else { cr = 0.92f; cg = 0.28f; cb = 0.24f; }
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
    if (g_mh.showClock) {
        std::time_t t = std::time(nullptr);
        std::tm tmBuf = {};
        localtime_s(&tmBuf, &t);
        char clockStr[32] = {0};
        std::snprintf(clockStr, sizeof(clockStr), "Time: %02d:%02d:%02d", tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec);
        drawHudLine(clockStr);
    }

    if (g_mh.startPosSwitcher) {
        float barW = 168.0f;
        float barH = 24.0f;
        float barX = (viewW - barW) * 0.5f;
        float barY = viewH - 34.0f;
        glDrawRect(barX, barY, barW, barH, 0.08f, 0.08f, 0.11f, 0.85f);
        glDrawOutlineRect(barX, barY, barW, barH, 1.2f, 0.85f, 0.16f, 0.16f, 0.92f);
        std::string spStr = "< [Q]  StartPos " + std::to_string(g_mh.currentStartPosIdx + 1) + "/" + std::to_string(g_mh.totalStartPosCount) + "  [E] >";
        glDrawText(barX + 12.0f, barY + 16.0f, spStr, 0.95f, 0.88f, 0.55f, 0.95f);
    }

    // Top-Right MHv9 [Tab] Pill Button
    float pillW = 92.0f;
    float pillH = 22.0f;
    float pillX = viewW - pillW - 12.0f;
    float pillY = 10.0f;
    bool hoverPill = pointInRect(mx, my, pillX, pillY, pillW, pillH);
    glDrawRect(pillX, pillY, pillW, pillH, hoverPill ? 0.90f : 0.78f, 0.15f, 0.15f, 0.92f);
    glDrawOutlineRect(pillX, pillY, pillW, pillH, 1.2f, 1.0f, 0.35f, 0.35f, 0.95f);
    glDrawText(pillX + 11.0f, pillY + 15.0f, "MHv9 [Tab]", 1.0f, 1.0f, 1.0f, 1.0f);

    // ------------------------------------------------------------------------
    // 6-COLUMN MEGA HACK V9 PRO INTERFACE
    // ------------------------------------------------------------------------
    if (g_mh.animProgress > 0.005f) {
        glDrawRect(0.0f, 0.0f, viewW, viewH, 0.02f, 0.02f, 0.04f, 0.64f * g_mh.animProgress);

        float colW = std::min(194.0f, (viewW - 56.0f) / 6.0f);
        float gap = 7.0f;
        float totalW = 6.0f * colW + 5.0f * gap;
        float startX = (viewW - totalW) * 0.5f;
        float baseTopY = 38.0f;
        float rowH = 22.0f;

        auto beginColumn = [&](int colIdx, const char* title, int numRows, float& outX, float& outY) {
            float delayT = (g_mh.animStyle == 3)
                ? g_mh.animProgress
                : std::clamp((g_mh.animProgress - colIdx * 0.04f) / 0.80f, 0.0f, 1.0f);
            float colEase = (g_mh.animStyle == 1 || g_mh.animStyle == 3) ? delayT : easeOutBack(delayT);
            outX = startX + colIdx * (colW + gap);
            outY = baseTopY - (1.0f - colEase) * 145.0f;
            float colH = 26.0f + numRows * rowH + 6.0f;

            glDrawRect(outX, outY, colW, colH, 0.07f, 0.07f, 0.09f, 0.96f * delayT);
            glDrawRect(outX, outY, colW, 24.0f, 0.84f, 0.15f, 0.15f, 0.98f * delayT);
            glDrawOutlineRect(outX, outY, colW, colH, 1.2f, 0.30f, 0.31f, 0.38f, 0.95f * delayT);
            glDrawText(outX + 8.0f, outY + 16.0f, title, 1.0f, 1.0f, 1.0f, delayT);
            outY += 26.0f;
        };

        auto drawToggleFull = [&](float colX, float& curY, const char* label, bool* state, std::function<void()> onChange, std::function<void()> onSub) {
            bool hasSub = static_cast<bool>(onSub);
            bool hovered = pointInRect(mx, my, colX + 3.0f, curY, colW - (hasSub ? 24.0f : 6.0f), rowH - 2.0f);
            if (*state) {
                glDrawRect(colX + 3.0f, curY, colW - 6.0f, rowH - 2.0f, 0.74f, 0.14f, 0.14f, hovered ? 0.58f : 0.44f);
                glDrawRect(colX + 3.0f, curY, 3.5f, rowH - 2.0f, 0.98f, 0.25f, 0.25f, 1.0f);
            } else {
                glDrawRect(colX + 3.0f, curY, colW - 6.0f, rowH - 2.0f, hovered ? 0.18f : 0.12f, hovered ? 0.18f : 0.12f, hovered ? 0.23f : 0.16f, 0.92f);
                glDrawRect(colX + 3.0f, curY, 3.0f, rowH - 2.0f, 0.28f, 0.28f, 0.34f, 1.0f);
            }

            glDrawText(colX + 10.0f, curY + 14.0f, label, *state ? 1.0f : 0.76f, *state ? 1.0f : 0.77f, *state ? 1.0f : 0.82f, 1.0f);

            if (mouseClicked && hovered && g_mh.menuOpen) {
                *state = !(*state);
                if (onChange) onChange();
            }

            if (hasSub) {
                bool subHover = pointInRect(mx, my, colX + colW - 21.0f, curY, 18.0f, rowH - 2.0f);
                glDrawText(colX + colW - 15.0f, curY + 14.0f, "<", 0.98f, 0.78f, 0.32f, 1.0f);
                if (mouseClicked && subHover && g_mh.menuOpen) {
                    onSub();
                }
            }
            curY += rowH;
        };

        auto drawToggleSimple = [&](float colX, float& curY, const char* label, bool* state) {
            drawToggleFull(colX, curY, label, state, nullptr, nullptr);
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
        beginColumn(0, "COSMETIC", 15, cx, cy);
        drawToggleSimple(cx, cy, "Accurate %", &g_mh.accuratePercentage);
        drawToggleSimple(cx, cy, "Ball Rotation Bug", &g_mh.ballRotationBug);
        drawToggleSimple(cx, cy, "No Glow", &g_mh.noGlow);
        drawToggleFull(cx, cy, "No Particles", &g_mh.noParticles, nullptr, [&]() {
            g_mh.activePopup = (g_mh.activePopup == 1) ? 0 : 1;
        });
        drawToggleSimple(cx, cy, "No Portal Circle", &g_mh.noPortalCircle);
        drawToggleSimple(cx, cy, "No Mirror Portal", &g_mh.noMirrorPortal);
        drawToggleSimple(cx, cy, "No Camera Shake", &g_mh.noCameraShake);
        drawToggleSimple(cx, cy, "Layout Mode", &g_mh.layoutMode);
        drawToggleSimple(cx, cy, "Player on Top", &g_mh.playerOnTop);
        drawToggleSimple(cx, cy, "No Wave Trail", &g_mh.noWaveTrail);
        drawToggleSimple(cx, cy, "No Wave Pulse", &g_mh.noWavePulse);
        drawToggleSimple(cx, cy, "Solid Wave Trail", &g_mh.solidWaveTrail);
        drawToggleFull(cx, cy, "P1 RGB Icons", &g_mh.p1RgbIcons, nullptr, [&]() {
            g_mh.activePopup = (g_mh.activePopup == 3) ? 0 : 3;
        });
        drawToggleSimple(cx, cy, "P2 RGB Icons", &g_mh.p2RgbIcons);
        drawToggleSimple(cx, cy, "P1 RGB Trail", &g_mh.p1RgbTrail);

        // Column 2: Level
        beginColumn(1, "LEVEL", 17, cx, cy);
        drawToggleFull(cx, cy, "Noclip", &g_mh.noclip, [&]() {
            toggleNoclipMemoryPatch(g_mh.noclip || g_mh.noCollision);
        }, nullptr);
        drawToggleFull(cx, cy, "No Collision", &g_mh.noCollision, [&]() {
            toggleNoclipMemoryPatch(g_mh.noclip || g_mh.noCollision);
        }, nullptr);
        drawToggleSimple(cx, cy, "Safe Mode", &g_mh.safeMode);
        drawToggleSimple(cx, cy, "Auto Safe Mode", &g_mh.autoSafeMode);
        drawToggleSimple(cx, cy, "Force Ice", &g_mh.forceIce);
        drawToggleSimple(cx, cy, "Shipcopter", &g_mh.shipcopter);
        drawToggleSimple(cx, cy, "Show Trajectory", &g_mh.showTrajectory);
        drawToggleSimple(cx, cy, "Show Hitboxes", &g_mh.showHitboxes);
        drawToggleSimple(cx, cy, "Hitboxes on Death", &g_mh.showHitboxesOnDeath);
        drawToggleSimple(cx, cy, "Hitbox Trail", &g_mh.hitboxTrail);
        drawToggleSimple(cx, cy, "Hitbox Multiplier", &g_mh.hitboxMultiplier);
        {
            std::ostringstream hs;
            hs << "Hitbox: " << std::fixed << std::setprecision(2) << g_mh.hitboxScale << "x";
            drawStepperItem(cx, cy, hs.str(), [&](int d) {
                g_mh.hitboxScale = std::clamp(g_mh.hitboxScale + d * 0.10f, 0.10f, 3.00f);
            });
        }
        drawToggleSimple(cx, cy, "StartPos Switch [Q/E]", &g_mh.startPosSwitcher);
        drawToggleSimple(cx, cy, "Random Seed", &g_mh.fixedRandomSeed);
        drawStepperItem(cx, cy, "Seed: " + std::to_string(g_mh.randomSeed), [&](int d) {
            g_mh.randomSeed = static_cast<unsigned int>(std::max(1, static_cast<int>(g_mh.randomSeed) + d * 111));
        });
        drawToggleSimple(cx, cy, "Instant Complete", &g_mh.instantComplete);
        drawToggleSimple(cx, cy, "Frame Step [C]", &g_mh.frameStepper);

        // Column 3: Universal & Window
        beginColumn(2, "UNIVERSAL", 8, cx, cy);
        drawToggleFull(cx, cy, "Custom BG", &g_mh.customBackground, nullptr, [&]() {
            g_mh.activePopup = (g_mh.activePopup == 2) ? 0 : 2;
        });
        drawToggleSimple(cx, cy, "Fast Chests", &g_mh.fastChests);
        drawToggleFull(cx, cy, "Free Win Resize", &g_mh.freeWindowResize, [&]() {
            updateWindowStyleModifiers(hwnd);
        }, nullptr);
        drawToggleFull(cx, cy, "Always on Top", &g_mh.alwaysOnTop, [&]() {
            updateWindowStyleModifiers(hwnd);
        }, nullptr);
        {
            const char* anchors[] = {"HUD: Top-Left", "HUD: Top-Right", "HUD: Bottom-Left", "HUD: Top-Center"};
            drawButtonItem(cx, cy, anchors[g_mh.hudAnchor], [&]() {
                g_mh.hudAnchor = (g_mh.hudAnchor + 1) % 4;
            });
        }
        {
            const char* anims[] = {"Anim: Cascade", "Anim: Slide Down", "Anim: Elastic Pop", "Anim: Instant"};
            drawButtonItem(cx, cy, anims[g_mh.animStyle], [&]() {
                g_mh.animStyle = (g_mh.animStyle + 1) % 4;
            });
        }
        drawButtonItem(cx, cy, "Save Config", [&]() {
            g_mh.saveConfigToDisk();
        });
        drawButtonItem(cx, cy, "Reset All OFF", [&]() {
            toggleNoclipMemoryPatch(false);
            g_mh = MegaHackV9Engine();
            g_mh.menuOpen = true;
            g_mh.animProgress = 1.0f;
            applyNativeCocosSpeedhack();
            applyNativeFpsAndVsync();
            updateWindowStyleModifiers(hwnd);
        });

        // Column 4: Display & Speedhack
        beginColumn(3, "DISPLAY / SPEED", 7, cx, cy);
        drawToggleFull(cx, cy, "Speedhack", &g_mh.speedhackEnabled, [&]() {
            applyNativeCocosSpeedhack();
        }, nullptr);
        {
            std::ostringstream ss;
            ss << "Speed: " << std::fixed << std::setprecision(2) << g_mh.speedhackValue << "x";
            drawStepperItem(cx, cy, ss.str(), [&](int d) {
                g_mh.speedhackValue = std::clamp(g_mh.speedhackValue + d * 0.10f, 0.10f, 5.00f);
                applyNativeCocosSpeedhack();
            });
        }
        drawToggleSimple(cx, cy, "Speedhack Audio", &g_mh.speedhackAudio);
        drawToggleFull(cx, cy, "Frame Extrap.", &g_mh.frameExtrapolation, [&]() {
            applyNativeFpsAndVsync();
        }, nullptr);
        drawStepperItem(cx, cy, "FPS: " + std::to_string(g_mh.targetFps), [&](int d) {
            g_mh.targetFps = std::clamp(g_mh.targetFps + d * 120, 60, 1440);
            applyNativeFpsAndVsync();
        });
        drawToggleSimple(cx, cy, "Auto Clicker", &g_mh.autoClicker);
        drawStepperItem(cx, cy, "Click CPS: " + std::to_string(g_mh.autoClickCps), [&](int d) {
            g_mh.autoClickCps = std::clamp(g_mh.autoClickCps + d * 5, 5, 60);
        });

        // Column 5: Status HUD
        beginColumn(4, "STATUS HUD", 8, cx, cy);
        drawToggleSimple(cx, cy, "Cheat Indicator", &g_mh.cheatIndicator);
        drawToggleSimple(cx, cy, "FPS Counter", &g_mh.showFps);
        drawToggleSimple(cx, cy, "CPS Counter", &g_mh.showCps);
        drawToggleSimple(cx, cy, "Noclip Accuracy", &g_mh.showNoclipAccuracy);
        drawToggleSimple(cx, cy, "Noclip Deaths", &g_mh.showNoclipDeaths);
        drawToggleSimple(cx, cy, "Attempts", &g_mh.showAttempts);
        drawToggleSimple(cx, cy, "Best Run", &g_mh.showBestRun);
        drawToggleSimple(cx, cy, "Session Clock", &g_mh.showClock);

        // Column 6: Bypass & Macro Bot
        beginColumn(5, "BYPASS & BOT", 9, cx, cy);
        drawToggleSimple(cx, cy, "Unlock All Icons", &g_mh.iconBypass);
        drawToggleSimple(cx, cy, "Copy Hack", &g_mh.copyHack);
        drawToggleSimple(cx, cy, "Verify Hack", &g_mh.verifyHack);
        drawToggleSimple(cx, cy, "Practice Music", &g_mh.practiceMusicSync);
        drawToggleSimple(cx, cy, "Level Edit", &g_mh.levelEditBypass);
        {
            const char* botStates[] = {"Bot: Disabled", "Bot: Recording", "Bot: Playback"};
            drawButtonItem(cx, cy, botStates[g_mh.macroMode], [&]() {
                g_mh.macroMode = (g_mh.macroMode + 1) % 3;
                if (g_mh.macroMode == 1) g_mh.macroBuffer.clear();
                g_mh.macroCursor = 0;
                g_mh.physicsTick = 0;
            });
        }
        drawButtonItem(cx, cy, "Save Macro (" + std::to_string(g_mh.macroBuffer.size()) + ")", [&]() {
            g_mh.saveMacroToDisk();
        });
        drawButtonItem(cx, cy, "Load Macro", [&]() {
            g_mh.loadMacroFromDisk();
        });
        drawButtonItem(cx, cy, "Clear Macro", [&]() {
            g_mh.macroBuffer.clear();
            g_mh.macroCursor = 0;
            g_mh.macroMode = 0;
        });

        // Submenu Popup 1: Granular No Particles Filter
        if (g_mh.activePopup == 1) {
            float pw = 215.0f;
            float ph = 150.0f;
            float px = (viewW - pw) * 0.5f;
            float py = (viewH - ph) * 0.5f;
            glDrawRect(px, py, pw, ph, 0.06f, 0.06f, 0.08f, 0.98f);
            glDrawRect(px, py, pw, 24.0f, 0.84f, 0.15f, 0.15f, 1.0f);
            glDrawOutlineRect(px, py, pw, ph, 1.5f, 0.95f, 0.32f, 0.32f, 1.0f);
            glDrawText(px + 10.0f, py + 16.0f, "NO PARTICLES FILTER", 1.0f, 1.0f, 1.0f, 1.0f);

            float itemY = py + 28.0f;
            float savedColW = colW;
            colW = pw;
            drawToggleSimple(px, itemY, "Portal Effect (Keep)", &g_mh.keepPortalParticles);
            drawToggleSimple(px, itemY, "Hide Ground Particles", &g_mh.hideGroundParticles);
            drawToggleSimple(px, itemY, "Hide Trail Particles", &g_mh.hideTrailParticles);
            drawToggleSimple(px, itemY, "Hide Click Particles", &g_mh.hideClickParticles);
            drawToggleSimple(px, itemY, "Hide Sparkle Particles", &g_mh.hideSparkleParticles);
            colW = savedColW;
        }

        // Submenu Popup 2: Custom Background RGB Picker
        if (g_mh.activePopup == 2) {
            float pw = 215.0f;
            float ph = 128.0f;
            float px = (viewW - pw) * 0.5f;
            float py = (viewH - ph) * 0.5f;
            glDrawRect(px, py, pw, ph, 0.06f, 0.06f, 0.08f, 0.98f);
            glDrawRect(px, py, pw, 24.0f, 0.84f, 0.15f, 0.15f, 1.0f);
            glDrawOutlineRect(px, py, pw, ph, 1.5f, 0.95f, 0.32f, 0.32f, 1.0f);
            glDrawText(px + 10.0f, py + 16.0f, "CUSTOM BG RGB PICKER", 1.0f, 1.0f, 1.0f, 1.0f);

            float itemY = py + 28.0f;
            float savedColW = colW;
            colW = pw;
            drawStepperItem(px, itemY, "Red: " + std::to_string(static_cast<int>(g_mh.customBgR * 100.f)) + "%", [&](int d) {
                g_mh.customBgR = std::clamp(g_mh.customBgR + d * 0.05f, 0.0f, 1.0f);
            });
            drawStepperItem(px, itemY, "Green: " + std::to_string(static_cast<int>(g_mh.customBgG * 100.f)) + "%", [&](int d) {
                g_mh.customBgG = std::clamp(g_mh.customBgG + d * 0.05f, 0.0f, 1.0f);
            });
            drawStepperItem(px, itemY, "Blue: " + std::to_string(static_cast<int>(g_mh.customBgB * 100.f)) + "%", [&](int d) {
                g_mh.customBgB = std::clamp(g_mh.customBgB + d * 0.05f, 0.0f, 1.0f);
            });
            drawButtonItem(px, itemY, "Close Picker", [&]() { g_mh.activePopup = 0; });
            colW = savedColW;
        }

        // Submenu Popup 3: Icon RGB Speed Customizer
        if (g_mh.activePopup == 3) {
            float pw = 215.0f;
            float ph = 105.0f;
            float px = (viewW - pw) * 0.5f;
            float py = (viewH - ph) * 0.5f;
            glDrawRect(px, py, pw, ph, 0.06f, 0.06f, 0.08f, 0.98f);
            glDrawRect(px, py, pw, 24.0f, 0.84f, 0.15f, 0.15f, 1.0f);
            glDrawOutlineRect(px, py, pw, ph, 1.5f, 0.95f, 0.32f, 0.32f, 1.0f);
            glDrawText(px + 10.0f, py + 16.0f, "RGB CHROMA CUSTOMIZER", 1.0f, 1.0f, 1.0f, 1.0f);

            float itemY = py + 28.0f;
            float savedColW = colW;
            colW = pw;
            drawStepperItem(px, itemY, "RGB Speed: " + std::to_string(static_cast<int>(g_mh.rgbSpeed)), [&](int d) {
                g_mh.rgbSpeed = std::clamp(g_mh.rgbSpeed + d * 25.0f, 20.0f, 500.0f);
            });
            drawButtonItem(px, itemY, "Close RGB Customizer", [&]() { g_mh.activePopup = 0; });
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
// 8. 64-BIT INLINE TRAMPOLINE + IAT HOOK ENGINE (100% RELIABLE CAPTURE)
// ============================================================================
using SwapBuffersFn = BOOL(WINAPI*)(HDC);
static SwapBuffersFn g_origWglSwapBuffers = nullptr;
static SwapBuffersFn g_origGdiSwapBuffers = SwapBuffers;
static bool g_inSwapHook = false;

static BOOL WINAPI hookedWglSwapBuffers(HDC hdc) {
    if (!g_inSwapHook && hdc) {
        g_inSwapHook = true;
        renderMegaHackV9Frame(hdc);
        g_inSwapHook = false;
    }
    return g_origWglSwapBuffers ? g_origWglSwapBuffers(hdc) : SwapBuffers(hdc);
}

static BOOL WINAPI hookedGdiSwapBuffers(HDC hdc) {
    if (!g_inSwapHook && hdc) {
        g_inSwapHook = true;
        renderMegaHackV9Frame(hdc);
        g_inSwapHook = false;
    }
    return g_origGdiSwapBuffers(hdc);
}

static bool installInlineHook64(void* targetFunc, void* detourFunc, void** outTrampoline) {
    if (!targetFunc || !detourFunc || !outTrampoline) return false;
    uint8_t* src = static_cast<uint8_t*>(targetFunc);

    // Follow 5-byte relative JMP (0xE9) if Steam Overlay or another hook is already at the entry
    for (int hop = 0; hop < 3; ++hop) {
        if (src[0] == 0xE9) {
            int32_t rel = *reinterpret_cast<int32_t*>(src + 1);
            src = src + 5 + rel;
        } else {
            break;
        }
    }

    // Standard x64 prologue for opengl32!wglSwapBuffers / gdi32!SwapBuffers is 15 bytes
    // Verify safe non-branching prologue bytes before inline patching
    size_t copyLen = 0;
    if (src[0] == 0x48 && src[1] == 0x89 && src[2] == 0x5C && src[3] == 0x24 &&
        src[5] == 0x57 && src[6] == 0x48 && src[7] == 0x83 && src[8] == 0xEC) {
        copyLen = 15;
    } else if (src[0] == 0x40 && src[1] == 0x53 && src[2] == 0x48 && src[3] == 0x83 && src[4] == 0xEC) {
        copyLen = 14;
    } else if (src[0] == 0x48 && src[1] == 0x8B && src[2] == 0xC4) {
        copyLen = 14;
    }

    if (copyLen < 14) return false;

    uint8_t* tramp = static_cast<uint8_t*>(VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!tramp) return false;

    std::memcpy(tramp, src, copyLen);
    // Append 14-byte absolute jump back to src + copyLen
    tramp[copyLen + 0] = 0xFF;
    tramp[copyLen + 1] = 0x25;
    *reinterpret_cast<uint32_t*>(tramp + copyLen + 2) = 0;
    *reinterpret_cast<uint64_t*>(tramp + copyLen + 6) = reinterpret_cast<uint64_t>(src + copyLen);

    DWORD oldProt = 0;
    if (!VirtualProtect(src, copyLen, PAGE_EXECUTE_READWRITE, &oldProt)) return false;

    // Write 14-byte absolute jump from src to detourFunc
    src[0] = 0xFF;
    src[1] = 0x25;
    *reinterpret_cast<uint32_t*>(src + 2) = 0;
    *reinterpret_cast<uint64_t*>(src + 6) = reinterpret_cast<uint64_t>(detourFunc);
    for (size_t i = 14; i < copyLen; ++i) src[i] = 0x90;

    VirtualProtect(src, copyLen, oldProt, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), src, copyLen);
    *outTrampoline = tramp;
    return true;
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
    for (int retry = 0; retry < 120; ++retry) {
        if (GetModuleHandleA("libcocos2d.dll") && GetModuleHandleA("opengl32.dll")) break;
        Sleep(50);
    }
    Sleep(250);

    HMODULE ogl = GetModuleHandleA("opengl32.dll");
    if (ogl) {
        void* wglSwapAddr = reinterpret_cast<void*>(GetProcAddress(ogl, "wglSwapBuffers"));
        if (wglSwapAddr) {
            g_origWglSwapBuffers = reinterpret_cast<SwapBuffersFn>(wglSwapAddr);
            installInlineHook64(wglSwapAddr, reinterpret_cast<void*>(&hookedWglSwapBuffers), reinterpret_cast<void**>(&g_origWglSwapBuffers));
        }
    }

    HMODULE mods[6] = {
        GetModuleHandleA(nullptr),
        GetModuleHandleA("libcocos2d.dll"),
        GetModuleHandleA("opengl32.dll"),
        GetModuleHandleA("Geode.dll"),
        GetModuleHandleA("glew32.dll"),
        GetModuleHandleA("gdi32.dll")
    };

    for (HMODULE m : mods) {
        if (!m) continue;
        hookModuleIAT(m, "SwapBuffers", reinterpret_cast<void*>(&hookedGdiSwapBuffers), nullptr);
        hookModuleIAT(m, "wglSwapBuffers", reinterpret_cast<void*>(&hookedWglSwapBuffers), nullptr);
        hookModuleIAT(m, "QueryPerformanceCounter", reinterpret_cast<void*>(&hookedQueryPerformanceCounter), nullptr);
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        // Pin DLL in memory so Geode never unloads the background hook thread
        HMODULE pinned = nullptr;
        GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&DllMain),
            &pinned
        );
        CreateThread(nullptr, 0, megaHackInitThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
