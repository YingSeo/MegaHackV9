#include <Geode/Geode.hpp>
#include <Geode/Bindings.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <cmath>
#include <vector>
#include <deque>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdlib>

using namespace geode::prelude;

// Direct Win32 Hardware Input API (Zero header macro conflicts)
#ifdef GEODE_IS_WINDOWS
extern "C" __declspec(dllimport) short __stdcall GetAsyncKeyState(int vKey);
extern "C" __declspec(dllimport) void __stdcall mouse_event(
    unsigned long dwFlags,
    unsigned long dx,
    unsigned long dy,
    unsigned long dwData,
    unsigned long long dwExtraInfo
);
#endif

// ============================================================================
// 1. GLOBAL ENGINE STATE & MACRO STRUCTURES (ALL DEFAULT TO OFF)
// ============================================================================
struct MacroTickAction {
    int tick;
    bool pressDown;
};

struct MegaHackUltimateState {
    bool menuOpen = false;
    int animStyle = 0;   // 0: Smooth Expo+Back, 1: Slide Down, 2: Elastic Scale
    int hudAnchor = 0;   // 0: Top-Left, 1: Top-Right, 2: Bottom-Left, 3: Top-Center

    // Column 1: Cosmetic & Visuals
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
    bool forceMiniIcon = false;
    bool hidePlayer = false;
    bool p1RgbIcons = false;
    bool p2RgbIcons = false;

    // Column 2: Level & Physics
    bool noclip = false;
    bool noCollision = false;
    bool safeMode = false;
    bool autoSafeMode = true;
    bool shipcopter = false;
    bool forceIce = false;
    bool showTrajectory = false;
    bool showHitboxes = false;
    bool showHitboxesOnDeath = false;
    bool hitboxTrail = false;
    bool hitboxMultiplier = false;
    float hitboxScale = 0.50f;
    bool startPosSwitcher = false;
    bool quickResetKey = false;
    bool fixedRandomSeed = false;
    unsigned int randomSeed = 1337;
    bool instantComplete = false;
    bool frameStepper = false;
    bool stepRequested = false;

    // Column 3: Universal & Config
    bool customBackground = false;
    int bgThemeIdx = 0;

    // Column 4: Display, Speed & AutoClicker
    bool speedhackEnabled = false;
    float speedhackValue = 1.00f;
    bool frameExtrapolation = false;
    float targetFps = 480.0f;
    bool autoClicker = false;
    int autoClickCps = 15;
    float autoClickAccum = 0.0f;
    bool autoClickHeld = false;

    // Column 5: Status HUD
    bool cheatIndicator = false;
    bool showFps = false;
    bool showCps = false;
    bool showNoclipAccuracy = false;
    bool showNoclipDeaths = false;
    bool showAttempts = false;
    bool showBestRun = false;

    // Column 6: Bypass & Macro Replay Bot
    int macroMode = 0; // 0: Disabled, 1: Recording, 2: Playback
    std::vector<MacroTickAction> macroBuffer;
    size_t macroCursor = 0;
    int physicsTick = 0;

    // Hardware Key Edge States
    bool prevTabDown = false;
    bool prevCDown = false;
    bool prevQDown = false;
    bool prevEDown = false;
    bool prevRDown = false;
    bool prevJumpDown = false;

    // Runtime Level Tracking
    std::vector<cocos2d::CCPoint> startPosList;
    int currentStartPosIdx = 0;
    bool inPlayLayerInit = false;
    int totalFrames = 0;
    int noclipHitFrames = 0;
    int noclipDeaths = 0;
    int attemptCount = 1;
    size_t peakCps = 0;
    bool collidingThisFrame = false;
    bool playerIsDead = false;
    float runStartPct = 0.0f;
    float bestRunFrom = 0.0f;
    float bestRunTo = 0.0f;
    float maxLevelX = 2400.0f;
    float iceDriftVelX = 0.0f;
    std::vector<float> clickTimestamps;
    float rgbHue = 0.0f;
    std::deque<cocos2d::CCRect> hitboxTrailHistory;

    cocos2d::CCPoint p1PrevPos = {0.f, 0.f};
    cocos2d::CCPoint p1VelSmooth = {0.f, 0.f};

    bool isCheating() const {
        return noclip || noCollision || shipcopter || forceIce ||
               showTrajectory || showHitboxes || hitboxMultiplier ||
               layoutMode || fixedRandomSeed || instantComplete ||
               frameStepper || autoClicker || (macroMode == 2) ||
               (speedhackEnabled && std::abs(speedhackValue - 1.0f) > 0.001f);
    }

    bool shouldProtectProgress() const {
        return safeMode || (autoSafeMode && isCheating());
    }

    void saveToDisk() {
        auto mod = Mod::get();
        mod->setSavedValue("accuratePercentage", accuratePercentage);
        mod->setSavedValue("noGlow", noGlow);
        mod->setSavedValue("noParticles", noParticles);
        mod->setSavedValue("noPortalCircle", noPortalCircle);
        mod->setSavedValue("noMirrorPortal", noMirrorPortal);
        mod->setSavedValue("noCameraShake", noCameraShake);
        mod->setSavedValue("layoutMode", layoutMode);
        mod->setSavedValue("playerOnTop", playerOnTop);
        mod->setSavedValue("p1RgbIcons", p1RgbIcons);
        mod->setSavedValue("p2RgbIcons", p2RgbIcons);
        mod->setSavedValue("autoSafeMode", autoSafeMode);
        mod->setSavedValue("startPosSwitcher", startPosSwitcher);
        mod->setSavedValue("quickResetKey", quickResetKey);
        mod->setSavedValue("customBackground", customBackground);
        mod->setSavedValue("bgThemeIdx", bgThemeIdx);
        mod->setSavedValue("hudAnchor", hudAnchor);
        mod->setSavedValue("animStyle", animStyle);
        mod->setSavedValue("cheatIndicator", cheatIndicator);
        mod->setSavedValue("showFps", showFps);
        mod->setSavedValue("showCps", showCps);
        mod->setSavedValue("showNoclipAccuracy", showNoclipAccuracy);
        mod->setSavedValue("showNoclipDeaths", showNoclipDeaths);
        mod->setSavedValue("showAttempts", showAttempts);
        mod->setSavedValue("showBestRun", showBestRun);
    }
};

static MegaHackUltimateState g_mh;

struct ToggleCallbackData : public cocos2d::CCObject {
    bool* statePtr = nullptr;
    cocos2d::CCSprite* bgSpr = nullptr;
    cocos2d::CCSprite* barSpr = nullptr;
    cocos2d::CCLabelBMFont* textLbl = nullptr;
    std::function<void()> onChange;
};

struct ActionCallbackData : public cocos2d::CCObject {
    std::function<void()> onTrigger;
};

// ============================================================================
// 2. PURE COCOS2D-X HELPER FUNCTIONS
// ============================================================================
static std::string formatFloat(const char* prefix, float val, const char* suffix, int decimals = 2) {
    std::ostringstream ss;
    ss << prefix << std::fixed << std::setprecision(decimals) << val << suffix;
    return ss.str();
}

static std::string formatInt(const char* prefix, int val, const char* suffix = "") {
    std::ostringstream ss;
    ss << prefix << val << suffix;
    return ss.str();
}

static cocos2d::ccColor3B hsvToRgb(float h, float s, float v) {
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
    return {
        static_cast<GLubyte>((r + m) * 255.f),
        static_cast<GLubyte>((g + m) * 255.f),
        static_cast<GLubyte>((b + m) * 255.f)
    };
}

static const cocos2d::ccColor3B kBgThemes[] = {
    {20, 25, 48},   // Midnight Abyss
    {48, 16, 36},   // Crimson Velvet
    {14, 44, 38},   // Cyber Emerald
    {26, 26, 32},   // Obsidian Matte
    {56, 35, 16}    // Golden Bronze
};

static cocos2d::CCSprite* makeSolidSprite(float w, float h, cocos2d::ccColor3B col, GLubyte opacity = 255) {
    auto spr = cocos2d::CCSprite::create();
    spr->setTextureRect({0.0f, 0.0f, w, h});
    spr->setColor(col);
    spr->setOpacity(opacity);
    return spr;
}

static void applyCustomMenuBg(cocos2d::CCNode* layer) {
    if (!layer || !g_mh.customBackground) return;
    auto col = kBgThemes[g_mh.bgThemeIdx % 5];
    if (auto bg = dynamic_cast<cocos2d::CCSprite*>(layer->getChildByID("background"))) {
        bg->setColor(col);
    }
    if (auto children = layer->getChildren()) {
        for (unsigned int i = 0; i < children->count(); ++i) {
            if (auto spr = dynamic_cast<cocos2d::CCSprite*>(children->objectAtIndex(i))) {
                if (spr->getZOrder() <= -1) {
                    spr->setColor(col);
                }
            }
        }
    }
}

static void switchStartPos(int dir) {
    auto pl = PlayLayer::get();
    if (!pl || g_mh.startPosList.empty()) return;

    int count = static_cast<int>(g_mh.startPosList.size());
    g_mh.currentStartPosIdx = (g_mh.currentStartPosIdx + dir + count) % count;
    cocos2d::CCPoint target = g_mh.startPosList[g_mh.currentStartPosIdx];
    if (pl->m_player1) pl->m_player1->setPosition(target);
    if (pl->m_player2) pl->m_player2->setPosition(target);
}

static void synthesizeJumpHold(bool pressDown) {
#ifdef GEODE_IS_WINDOWS
    // 0x0002 = MOUSEEVENTF_LEFTDOWN, 0x0004 = MOUSEEVENTF_LEFTUP
    mouse_event(pressDown ? 0x0002UL : 0x0004UL, 0, 0, 0, 0);
#else
    (void)pressDown;
#endif
}

// ============================================================================
// 3. MEGA HACK V9 PRO UI OVERLAY (6 COLUMNS + SMOOTH EASING ANIMATIONS)
// ============================================================================
class MegaHackV9Overlay : public cocos2d::CCLayerColor {
public:
    std::vector<cocos2d::CCNode*> m_columns;
    cocos2d::CCNode* m_particlePopup = nullptr;
    cocos2d::CCLabelBMFont* m_speedLbl = nullptr;
    cocos2d::CCLabelBMFont* m_hitboxLbl = nullptr;
    cocos2d::CCLabelBMFont* m_fpsLbl = nullptr;
    cocos2d::CCLabelBMFont* m_seedLbl = nullptr;
    cocos2d::CCLabelBMFont* m_botLbl = nullptr;
    cocos2d::CCLabelBMFont* m_anchorLbl = nullptr;
    cocos2d::CCLabelBMFont* m_animLbl = nullptr;
    cocos2d::CCLabelBMFont* m_autoClickLbl = nullptr;

    static MegaHackV9Overlay* getOrCreate() {
        auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
        if (!scene) return nullptr;
        if (auto existing = dynamic_cast<MegaHackV9Overlay*>(scene->getChildByID("mhv9-pro-ui"))) {
            return existing;
        }
        auto ret = new MegaHackV9Overlay();
        if (ret && ret->init()) {
            ret->autorelease();
            ret->setID("mhv9-pro-ui");
            scene->addChild(ret, 99999);
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    void onExternalToggleButton(cocos2d::CCObject*) {
        this->toggleMenu();
    }

    void onPrevStartPosButton(cocos2d::CCObject*) {
        switchStartPos(-1);
    }

    void onNextStartPosButton(cocos2d::CCObject*) {
        switchStartPos(+1);
    }

    bool init() override {
        if (!CCLayerColor::initWithColor({0, 0, 0, 0})) return false;
        this->setTouchEnabled(false);
        this->setVisible(false);

        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        float colW = 89.5f;
        float gap = 4.2f;
        float totalW = 6.f * colW + 5.f * gap;
        float startX = (winSize.width - totalW) * 0.5f + colW * 0.5f;
        float topY = winSize.height - 14.0f;

        buildCosmeticColumn({startX + 0.f * (colW + gap), topY}, colW);
        buildLevelColumn({startX + 1.f * (colW + gap), topY}, colW);
        buildUniversalColumn({startX + 2.f * (colW + gap), topY}, colW);
        buildDisplaySpeedColumn({startX + 3.f * (colW + gap), topY}, colW);
        buildStatusHudColumn({startX + 4.f * (colW + gap), topY}, colW);
        buildBypassBotColumn({startX + 5.f * (colW + gap), topY}, colW);

        buildParticleFilterModal({winSize.width * 0.5f, winSize.height * 0.5f});
        return true;
    }

    cocos2d::CCNode* createColumnBox(const char* title, cocos2d::CCPoint topCenter, float width, float height) {
        auto panel = cocos2d::CCNode::create();
        panel->setPosition(topCenter);
        panel->setContentSize({width, height});
        panel->setAnchorPoint({0.5f, 1.0f});
        panel->setCascadeOpacityEnabled(true);

        auto outline = makeSolidSprite(width + 1.6f, height + 1.6f, {62, 64, 78}, 255);
        outline->setAnchorPoint({0.5f, 1.0f});
        outline->setPosition({width * 0.5f, height + 0.8f});
        panel->addChild(outline, -1);

        auto body = makeSolidSprite(width, height, {16, 16, 22}, 246);
        body->setAnchorPoint({0.5f, 1.0f});
        body->setPosition({width * 0.5f, height});
        panel->addChild(body, 0);

        float headerH = 14.0f;
        auto header = makeSolidSprite(width, headerH, {210, 38, 38}, 255);
        header->setAnchorPoint({0.5f, 1.0f});
        header->setPosition({width * 0.5f, height});
        panel->addChild(header, 1);

        auto titleLbl = cocos2d::CCLabelBMFont::create(title, "bigFont.fnt");
        titleLbl->setScale(0.22f);
        titleLbl->setPosition({width * 0.5f, height - headerH * 0.5f});
        panel->addChild(titleLbl, 2);

        auto menu = cocos2d::CCMenu::create();
        menu->setPosition({0.f, 0.f});
        menu->setID("column-menu");
        panel->addChild(menu, 3);

        this->addChild(panel);
        m_columns.push_back(panel);
        return panel;
    }

    void addToggleRow(cocos2d::CCNode* col, float& curY, const char* text, bool* ptr, std::function<void()> onChange = nullptr, std::function<void()> onArrow = nullptr) {
        float w = col->getContentSize().width;
        float rowH = 13.0f;
        auto menu = dynamic_cast<cocos2d::CCMenu*>(col->getChildByID("column-menu"));
        if (!menu) return;

        auto bg = makeSolidSprite(
            w - 5.0f, rowH - 1.3f,
            *ptr ? cocos2d::ccColor3B{165, 34, 34} : cocos2d::ccColor3B{28, 28, 36},
            *ptr ? 210 : 235
        );

        auto bar = makeSolidSprite(
            2.4f, rowH - 1.3f,
            *ptr ? cocos2d::ccColor3B{245, 65, 65} : cocos2d::ccColor3B{70, 70, 84},
            255
        );
        bar->setAnchorPoint({0.0f, 0.0f});
        bar->setPosition({0.0f, 0.0f});
        bg->addChild(bar);

        auto lbl = cocos2d::CCLabelBMFont::create(text, "chatFont.fnt");
        lbl->setScale(0.35f);
        lbl->setAnchorPoint({0.0f, 0.5f});
        lbl->setPosition({5.2f, (rowH - 1.3f) * 0.5f});
        lbl->setColor(*ptr ? cocos2d::ccColor3B{255, 255, 255} : cocos2d::ccColor3B{192, 194, 206});
        bg->addChild(lbl);

        auto btn = cocos2d::CCMenuItemSprite::create(bg, bg, this, menu_selector(MegaHackV9Overlay::onToggleClicked));
        btn->setPosition({w * 0.5f, curY});

        auto meta = new ToggleCallbackData();
        meta->autorelease();
        meta->statePtr = ptr;
        meta->bgSpr = bg;
        meta->barSpr = bar;
        meta->textLbl = lbl;
        meta->onChange = onChange;
        btn->setUserObject(meta);
        menu->addChild(btn);

        if (onArrow) {
            auto arrLbl = cocos2d::CCLabelBMFont::create("<", "bigFont.fnt");
            arrLbl->setScale(0.20f);
            arrLbl->setColor({245, 192, 78});
            auto arrBtn = cocos2d::CCMenuItemSprite::create(arrLbl, arrLbl, this, menu_selector(MegaHackV9Overlay::onActionClicked));
            arrBtn->setPosition({w - 8.5f, curY});
            auto act = new ActionCallbackData();
            act->autorelease();
            act->onTrigger = onArrow;
            arrBtn->setUserObject(act);
            menu->addChild(arrBtn, 5);
        }

        curY -= rowH;
    }

    void addStepperRow(cocos2d::CCNode* col, float& curY, const char* initText, cocos2d::CCLabelBMFont** outLbl, std::function<void(int)> onStep) {
        float w = col->getContentSize().width;
        float rowH = 13.0f;
        auto menu = dynamic_cast<cocos2d::CCMenu*>(col->getChildByID("column-menu"));
        if (!menu) return;

        auto lbl = cocos2d::CCLabelBMFont::create(initText, "chatFont.fnt");
        lbl->setScale(0.34f);
        lbl->setAnchorPoint({0.5f, 0.5f});
        lbl->setPosition({w * 0.5f, curY});
        lbl->setColor({245, 215, 125});
        col->addChild(lbl, 4);
        if (outLbl) *outLbl = lbl;

        auto addStepBtn = [&](const char* sym, float xPos, int dir) {
            auto bLbl = cocos2d::CCLabelBMFont::create(sym, "bigFont.fnt");
            bLbl->setScale(0.23f);
            auto btn = cocos2d::CCMenuItemSprite::create(bLbl, bLbl, this, menu_selector(MegaHackV9Overlay::onActionClicked));
            btn->setPosition({xPos, curY});
            auto act = new ActionCallbackData();
            act->autorelease();
            act->onTrigger = [onStep, dir]() { onStep(dir); };
            btn->setUserObject(act);
            menu->addChild(btn);
        };

        addStepBtn("-", 9.0f, -1);
        addStepBtn("+", w - 9.0f, +1);
        curY -= rowH;
    }

    void addActionButtonRow(cocos2d::CCNode* col, float& curY, const char* title, cocos2d::CCLabelBMFont** outLbl, std::function<void()> onClick) {
        float w = col->getContentSize().width;
        float rowH = 13.5f;
        auto menu = dynamic_cast<cocos2d::CCMenu*>(col->getChildByID("column-menu"));
        if (!menu) return;

        auto bg = makeSolidSprite(w - 5.5f, rowH - 1.5f, {40, 44, 62}, 240);

        auto lbl = cocos2d::CCLabelBMFont::create(title, "chatFont.fnt");
        lbl->setScale(0.35f);
        lbl->setPosition({(w - 5.5f) * 0.5f, (rowH - 1.5f) * 0.5f});
        lbl->setColor({135, 222, 255});
        bg->addChild(lbl);
        if (outLbl) *outLbl = lbl;

        auto btn = cocos2d::CCMenuItemSprite::create(bg, bg, this, menu_selector(MegaHackV9Overlay::onActionClicked));
        btn->setPosition({w * 0.5f, curY});
        auto act = new ActionCallbackData();
        act->autorelease();
        act->onTrigger = onClick;
        btn->setUserObject(act);
        menu->addChild(btn);

        curY -= rowH;
    }

    void onToggleClicked(cocos2d::CCObject* sender) {
        auto btn = static_cast<cocos2d::CCMenuItemSprite*>(sender);
        auto meta = static_cast<ToggleCallbackData*>(btn->getUserObject());
        if (!meta || !meta->statePtr) return;

        *(meta->statePtr) = !(*(meta->statePtr));
        bool active = *(meta->statePtr);

        meta->bgSpr->stopAllActions();
        meta->bgSpr->runAction(cocos2d::CCTintTo::create(0.12f, active ? 165 : 28, active ? 34 : 28, active ? 34 : 36));
        meta->bgSpr->setOpacity(active ? 210 : 235);
        meta->barSpr->setColor(active ? cocos2d::ccColor3B{245, 65, 65} : cocos2d::ccColor3B{70, 70, 84});
        meta->textLbl->setColor(active ? cocos2d::ccColor3B{255, 255, 255} : cocos2d::ccColor3B{192, 194, 206});

        if (meta->onChange) meta->onChange();
    }

    void onActionClicked(cocos2d::CCObject* sender) {
        auto btn = static_cast<cocos2d::CCMenuItemSprite*>(sender);
        auto act = static_cast<ActionCallbackData*>(btn->getUserObject());
        if (act && act->onTrigger) act->onTrigger();
    }

    void buildCosmeticColumn(cocos2d::CCPoint pos, float w) {
        float h = 218.0f;
        auto col = createColumnBox("Cosmetic", pos, w, h);
        float y = h - 21.0f;

        addToggleRow(col, y, "Accurate %", &g_mh.accuratePercentage);
        addToggleRow(col, y, "Ball Rotation Bug", &g_mh.ballRotationBug);
        addToggleRow(col, y, "No Glow", &g_mh.noGlow);
        addToggleRow(col, y, "No Particles", &g_mh.noParticles, nullptr, [this]() {
            if (m_particlePopup) {
                bool show = !m_particlePopup->isVisible();
                m_particlePopup->setVisible(show);
                if (show) {
                    m_particlePopup->setScale(0.82f);
                    m_particlePopup->runAction(cocos2d::CCEaseBackOut::create(cocos2d::CCScaleTo::create(0.18f, 1.0f)));
                }
            }
        });
        addToggleRow(col, y, "No Portal Circle", &g_mh.noPortalCircle);
        addToggleRow(col, y, "No Mirror Portal", &g_mh.noMirrorPortal);
        addToggleRow(col, y, "No Camera Shake", &g_mh.noCameraShake);
        addToggleRow(col, y, "Layout Mode", &g_mh.layoutMode);
        addToggleRow(col, y, "Player on Top", &g_mh.playerOnTop);
        addToggleRow(col, y, "Force Mini Icon", &g_mh.forceMiniIcon);
        addToggleRow(col, y, "Hide Player", &g_mh.hidePlayer);
        addToggleRow(col, y, "P1 RGB Icons", &g_mh.p1RgbIcons);
        addToggleRow(col, y, "P2 RGB Icons", &g_mh.p2RgbIcons);
    }

    void buildLevelColumn(cocos2d::CCPoint pos, float w) {
        float h = 268.0f;
        auto col = createColumnBox("Level", pos, w, h);
        float y = h - 21.0f;

        addToggleRow(col, y, "Noclip", &g_mh.noclip);
        addToggleRow(col, y, "No Collision", &g_mh.noCollision);
        addToggleRow(col, y, "Safe Mode", &g_mh.safeMode);
        addToggleRow(col, y, "Auto Safe Mode", &g_mh.autoSafeMode);
        addToggleRow(col, y, "Shipcopter Fly", &g_mh.shipcopter);
        addToggleRow(col, y, "Force Ice Drift", &g_mh.forceIce);
        addToggleRow(col, y, "Show Trajectory", &g_mh.showTrajectory);
        addToggleRow(col, y, "Show Hitboxes", &g_mh.showHitboxes);
        addToggleRow(col, y, "Hitboxes on Death", &g_mh.showHitboxesOnDeath);
        addToggleRow(col, y, "Hitbox Trail", &g_mh.hitboxTrail, []() {
            g_mh.hitboxTrailHistory.clear();
        });
        addToggleRow(col, y, "Hitbox Multiplier", &g_mh.hitboxMultiplier);
        addStepperRow(col, y, "Hitbox: 0.50x", &m_hitboxLbl, [this](int dir) {
            g_mh.hitboxScale = std::clamp(g_mh.hitboxScale + dir * 0.1f, 0.1f, 3.0f);
            if (m_hitboxLbl) {
                m_hitboxLbl->setString(formatFloat("Hitbox: ", g_mh.hitboxScale, "x").c_str());
            }
        });
        addToggleRow(col, y, "StartPos Switch [Q/E]", &g_mh.startPosSwitcher);
        addToggleRow(col, y, "Quick Reset [R]", &g_mh.quickResetKey);
        addToggleRow(col, y, "Random Seed", &g_mh.fixedRandomSeed);
        addStepperRow(col, y, "Seed: 1337", &m_seedLbl, [this](int dir) {
            g_mh.randomSeed = static_cast<unsigned int>(std::max(1, static_cast<int>(g_mh.randomSeed) + dir * 111));
            if (m_seedLbl) {
                m_seedLbl->setString(formatInt("Seed: ", static_cast<int>(g_mh.randomSeed)).c_str());
            }
        });
        addToggleRow(col, y, "Instant Complete", &g_mh.instantComplete, []() {
            if (g_mh.instantComplete) {
                if (auto pl = PlayLayer::get()) {
                    if (pl->m_player1) pl->m_player1->setPositionX(g_mh.maxLevelX + 60.0f);
                }
            }
        });
        addToggleRow(col, y, "Frame Step [C]", &g_mh.frameStepper);
    }

    void buildUniversalColumn(cocos2d::CCPoint pos, float w) {
        float h = 180.0f;
        auto col = createColumnBox("Universal", pos, w, h);
        float y = h - 21.0f;

        addToggleRow(col, y, "Custom BG", &g_mh.customBackground, []() {
            auto sc = cocos2d::CCDirector::sharedDirector()->getRunningScene();
            if (sc && sc->getChildrenCount() > 0) {
                applyCustomMenuBg(static_cast<cocos2d::CCNode*>(sc->getChildren()->objectAtIndex(0)));
            }
        }, []() {
            g_mh.bgThemeIdx = (g_mh.bgThemeIdx + 1) % 5;
            auto sc = cocos2d::CCDirector::sharedDirector()->getRunningScene();
            if (sc && sc->getChildrenCount() > 0) {
                applyCustomMenuBg(static_cast<cocos2d::CCNode*>(sc->getChildren()->objectAtIndex(0)));
            }
        });
        addActionButtonRow(col, y, "HUD: Top-Left", &m_anchorLbl, [this]() {
            g_mh.hudAnchor = (g_mh.hudAnchor + 1) % 4;
            const char* names[] = {"HUD: Top-Left", "HUD: Top-Right", "HUD: Bottom-Left", "HUD: Top-Center"};
            if (m_anchorLbl) m_anchorLbl->setString(names[g_mh.hudAnchor]);
        });
        addActionButtonRow(col, y, "Anim: Smooth Expo", &m_animLbl, [this]() {
            g_mh.animStyle = (g_mh.animStyle + 1) % 3;
            const char* styles[] = {"Anim: Smooth Expo", "Anim: Slide Down", "Anim: Elastic Pop"};
            if (m_animLbl) m_animLbl->setString(styles[g_mh.animStyle]);
        });
        addActionButtonRow(col, y, "Save Config", nullptr, []() {
            g_mh.saveToDisk();
        });
        addActionButtonRow(col, y, "Reset All OFF", nullptr, []() {
            g_mh = MegaHackUltimateState();
            g_mh.menuOpen = true;
            cocos2d::CCApplication::sharedApplication()->setAnimationInterval(1.0 / 240.0);
        });
    }

    void buildDisplaySpeedColumn(cocos2d::CCPoint pos, float w) {
        float h = 195.0f;
        auto col = createColumnBox("Display / Speed", pos, w, h);
        float y = h - 21.0f;

        addToggleRow(col, y, "Speedhack", &g_mh.speedhackEnabled);
        addStepperRow(col, y, "Speed: 1.00x", &m_speedLbl, [this](int dir) {
            g_mh.speedhackValue = std::clamp(g_mh.speedhackValue + dir * 0.1f, 0.1f, 5.0f);
            if (m_speedLbl) {
                m_speedLbl->setString(formatFloat("Speed: ", g_mh.speedhackValue, "x").c_str());
            }
        });
        addToggleRow(col, y, "Frame Extrap.", &g_mh.frameExtrapolation, []() {
            float fps = g_mh.frameExtrapolation ? g_mh.targetFps : 240.0f;
            cocos2d::CCApplication::sharedApplication()->setAnimationInterval(1.0 / fps);
        });
        addStepperRow(col, y, "FPS: 480", &m_fpsLbl, [this](int dir) {
            g_mh.targetFps = std::clamp(g_mh.targetFps + dir * 120.0f, 60.0f, 1440.0f);
            if (m_fpsLbl) {
                m_fpsLbl->setString(formatInt("FPS: ", static_cast<int>(g_mh.targetFps)).c_str());
            }
            if (g_mh.frameExtrapolation) {
                cocos2d::CCApplication::sharedApplication()->setAnimationInterval(1.0 / g_mh.targetFps);
            }
        });
        addToggleRow(col, y, "Auto Clicker", &g_mh.autoClicker);
        addStepperRow(col, y, "Click CPS: 15", &m_autoClickLbl, [this](int dir) {
            g_mh.autoClickCps = std::clamp(g_mh.autoClickCps + dir * 5, 5, 60);
            if (m_autoClickLbl) {
                m_autoClickLbl->setString(formatInt("Click CPS: ", g_mh.autoClickCps).c_str());
            }
        });
    }

    void buildStatusHudColumn(cocos2d::CCPoint pos, float w) {
        float h = 180.0f;
        auto col = createColumnBox("Status HUD", pos, w, h);
        float y = h - 21.0f;

        addToggleRow(col, y, "Cheat Indicator", &g_mh.cheatIndicator);
        addToggleRow(col, y, "FPS Counter", &g_mh.showFps);
        addToggleRow(col, y, "CPS Counter", &g_mh.showCps);
        addToggleRow(col, y, "Noclip Accuracy", &g_mh.showNoclipAccuracy);
        addToggleRow(col, y, "Noclip Deaths", &g_mh.showNoclipDeaths);
        addToggleRow(col, y, "Attempts", &g_mh.showAttempts);
        addToggleRow(col, y, "Best Run", &g_mh.showBestRun);
    }

    void buildBypassBotColumn(cocos2d::CCPoint pos, float w) {
        float h = 180.0f;
        auto col = createColumnBox("Macro & Utility", pos, w, h);
        float y = h - 21.0f;

        addActionButtonRow(col, y, "Bot: Disabled", &m_botLbl, [this]() {
            g_mh.macroMode = (g_mh.macroMode + 1) % 3;
            if (g_mh.macroMode == 1) {
                g_mh.macroBuffer.clear();
                if (m_botLbl) m_botLbl->setString("Bot: Recording");
            } else if (g_mh.macroMode == 2) {
                g_mh.macroCursor = 0;
                if (m_botLbl) m_botLbl->setString("Bot: Playback");
            } else {
                if (m_botLbl) m_botLbl->setString("Bot: Disabled");
            }
        });

        addActionButtonRow(col, y, "Clear Macro", nullptr, [this]() {
            g_mh.macroBuffer.clear();
            g_mh.macroCursor = 0;
            g_mh.macroMode = 0;
            if (m_botLbl) m_botLbl->setString("Bot: Disabled");
        });

        addActionButtonRow(col, y, "Restart Level [R]", nullptr, []() {
            if (auto pl = PlayLayer::get()) pl->resetLevel();
        });
    }

    void buildParticleFilterModal(cocos2d::CCPoint centerPos) {
        float w = 140.0f;
        float h = 110.0f;
        m_particlePopup = cocos2d::CCNode::create();
        m_particlePopup->setPosition(centerPos);
        m_particlePopup->setContentSize({w, h});
        m_particlePopup->setAnchorPoint({0.5f, 0.5f});
        m_particlePopup->setVisible(false);

        auto bg = makeSolidSprite(w, h, {14, 14, 18}, 252);
        bg->setAnchorPoint({0.f, 0.f});
        m_particlePopup->addChild(bg, 0);

        auto header = makeSolidSprite(w, 14.0f, {210, 38, 38}, 255);
        header->setAnchorPoint({0.f, 1.f});
        header->setPosition({0.f, h});
        m_particlePopup->addChild(header, 1);

        auto title = cocos2d::CCLabelBMFont::create("Particle Filter", "bigFont.fnt");
        title->setScale(0.22f);
        title->setPosition({w * 0.5f, h - 7.0f});
        m_particlePopup->addChild(title, 2);

        auto menu = cocos2d::CCMenu::create();
        menu->setPosition({0.f, 0.f});
        menu->setID("column-menu");
        m_particlePopup->addChild(menu, 3);

        float y = h - 22.0f;
        addToggleRow(m_particlePopup, y, "Portal Effect (Keep)", &g_mh.keepPortalParticles);
        addToggleRow(m_particlePopup, y, "Hide Ground Part.", &g_mh.hideGroundParticles);
        addToggleRow(m_particlePopup, y, "Hide Trail Part.", &g_mh.hideTrailParticles);
        addToggleRow(m_particlePopup, y, "Hide Click Part.", &g_mh.hideClickParticles);

        this->addChild(m_particlePopup, 100);
    }

    void toggleMenu() {
        g_mh.menuOpen = !g_mh.menuOpen;
        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        float topY = winSize.height - 14.0f;

        this->stopAllActions();

        if (g_mh.menuOpen) {
            this->setVisible(true);
            this->setTouchEnabled(true);
            this->setOpacity(0);
            this->runAction(cocos2d::CCEaseSineOut::create(cocos2d::CCFadeTo::create(0.18f, 165)));

            for (size_t i = 0; i < m_columns.size(); ++i) {
                auto col = m_columns[i];
                col->stopAllActions();
                cocos2d::CCPoint dest = {col->getPositionX(), topY};
                float delay = static_cast<float>(i) * 0.018f;

                if (g_mh.animStyle == 0) {
                    col->setPosition({dest.x, topY + 46.0f});
                    col->setScale(0.88f);
                    col->runAction(cocos2d::CCSequence::create(
                        cocos2d::CCDelayTime::create(delay),
                        cocos2d::CCSpawn::createWithTwoActions(
                            cocos2d::CCEaseBackOut::create(cocos2d::CCMoveTo::create(0.25f, dest)),
                            cocos2d::CCEaseExponentialOut::create(cocos2d::CCScaleTo::create(0.24f, 1.0f))
                        ),
                        nullptr
                    ));
                } else if (g_mh.animStyle == 1) {
                    col->setPosition({dest.x, topY + 85.0f});
                    col->setScale(1.0f);
                    col->runAction(cocos2d::CCSequence::create(
                        cocos2d::CCDelayTime::create(delay),
                        cocos2d::CCEaseExponentialOut::create(cocos2d::CCMoveTo::create(0.24f, dest)),
                        nullptr
                    ));
                } else {
                    col->setPosition(dest);
                    col->setScale(0.68f);
                    col->runAction(cocos2d::CCSequence::create(
                        cocos2d::CCDelayTime::create(delay),
                        cocos2d::CCEaseBackOut::create(cocos2d::CCScaleTo::create(0.24f, 1.0f)),
                        nullptr
                    ));
                }
            }
        } else {
            this->setTouchEnabled(false);
            if (m_particlePopup) m_particlePopup->setVisible(false);

            for (size_t i = 0; i < m_columns.size(); ++i) {
                auto col = m_columns[i];
                col->stopAllActions();
                cocos2d::CCPoint dest = {col->getPositionX(), topY + 35.0f};
                col->runAction(cocos2d::CCSpawn::createWithTwoActions(
                    cocos2d::CCEaseBackIn::create(cocos2d::CCMoveTo::create(0.15f, dest)),
                    cocos2d::CCEaseSineIn::create(cocos2d::CCScaleTo::create(0.15f, 0.90f))
                ));
            }

            this->runAction(cocos2d::CCSequence::create(
                cocos2d::CCFadeTo::create(0.16f, 0),
                cocos2d::CCHide::create(),
                nullptr
            ));
        }
    }
};

static cocos2d::CCMenuItemSprite* createPillButton(const char* text, cocos2d::CCObject* target, cocos2d::SEL_MenuHandler selector) {
    auto bg = makeSolidSprite(62.0f, 16.0f, {210, 38, 38}, 240);
    auto lbl = cocos2d::CCLabelBMFont::create(text, "bigFont.fnt");
    lbl->setScale(0.26f);
    lbl->setPosition({31.0f, 8.0f});
    bg->addChild(lbl);
    return cocos2d::CCMenuItemSprite::create(bg, bg, target, selector);
}

// ============================================================================
// 4. OVERLAY RENDERING & HUD ENGINE
// ============================================================================
static cocos2d::CCDrawNode* s_drawOverlay = nullptr;
static cocos2d::CCLabelBMFont* s_hudLabel = nullptr;
static cocos2d::CCLayerColor* s_indicatorDot = nullptr;
static cocos2d::CCNode* s_startPosBar = nullptr;
static cocos2d::CCLabelBMFont* s_startPosBarLbl = nullptr;

static void drawTrajectoryOverlay(PlayerObject* p) {
    if (!s_drawOverlay || !p) return;
    cocos2d::CCPoint start = p->getPosition();
    float vx = std::max(3.2f, std::abs(g_mh.p1VelSmooth.x));
    float vyHold = g_mh.p1VelSmooth.y;
    float vyRel  = g_mh.p1VelSmooth.y;

    cocos2d::CCPoint prevHold = start;
    cocos2d::CCPoint prevRel  = start;

    for (int i = 1; i <= 45; ++i) {
        vyHold = std::clamp(vyHold + 0.36f, -12.f, 12.f);
        vyRel  = std::clamp(vyRel  - 0.36f, -12.f, 12.f);

        cocos2d::CCPoint nextHold = {prevHold.x + vx, std::max(0.f, prevHold.y + vyHold)};
        cocos2d::CCPoint nextRel  = {prevRel.x  + vx, std::max(0.f, prevRel.y  + vyRel)};

        s_drawOverlay->drawSegment(prevHold, nextHold, 0.85f, {0.2f, 0.95f, 0.45f, 0.88f});
        s_drawOverlay->drawSegment(prevRel,  nextRel,  0.65f, {0.95f, 0.35f, 0.35f, 0.68f});

        prevHold = nextHold;
        prevRel  = nextRel;
    }
}

static void drawHitboxOverlay(PlayLayer* pl) {
    if (!s_drawOverlay || !pl) return;
    auto drawRectBox = [&](cocos2d::CCRect r, cocos2d::ccColor4F col) {
        cocos2d::CCPoint pts[4] = {
            {r.getMinX(), r.getMinY()},
            {r.getMaxX(), r.getMinY()},
            {r.getMaxX(), r.getMaxY()},
            {r.getMinX(), r.getMaxY()}
        };
        s_drawOverlay->drawPolygon(pts, 4, {col.r, col.g, col.b, 0.14f}, 0.65f, col);
    };

    if (g_mh.hitboxTrail) {
        for (const auto& oldRect : g_mh.hitboxTrailHistory) {
            drawRectBox(oldRect, {0.15f, 0.9f, 0.5f, 0.45f});
        }
    }

    if (pl->m_player1) {
        drawRectBox(pl->m_player1->boundingBox(), {0.1f, 1.0f, 0.3f, 0.95f});
    }

    if (auto objLayer = pl->getChildByID("object-layer")) {
        if (auto children = objLayer->getChildren()) {
            float px = pl->m_player1 ? pl->m_player1->getPositionX() : 0.f;
            for (unsigned int i = 0; i < children->count(); ++i) {
                auto node = dynamic_cast<cocos2d::CCNode*>(children->objectAtIndex(i));
                if (!node || node == pl->m_player1 || node == pl->m_player2 || node == s_drawOverlay) continue;
                if (std::abs(node->getPositionX() - px) < 260.0f) {
                    cocos2d::CCRect r = node->boundingBox();
                    if (r.size.width > 2.0f && r.size.width < 120.0f && r.size.height > 2.0f && r.size.height < 120.0f) {
                        if (g_mh.hitboxMultiplier) {
                            float cx = r.getMidX();
                            float cy = r.getMidY();
                            float sw = r.size.width * g_mh.hitboxScale;
                            float sh = r.size.height * g_mh.hitboxScale;
                            r = cocos2d::CCRect(cx - sw * 0.5f, cy - sh * 0.5f, sw, sh);
                        }
                        drawRectBox(r, {1.0f, 0.22f, 0.22f, 0.85f});
                    }
                }
            }
        }
    }
}

static void updateStatusHudOverlay(float dt) {
    auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();

    if (s_indicatorDot) {
        s_indicatorDot->setVisible(g_mh.cheatIndicator);
        if (g_mh.cheatIndicator) {
            cocos2d::ccColor3B c = {46, 204, 113};
            if (g_mh.isCheating()) {
                c = g_mh.shouldProtectProgress() ? cocos2d::ccColor3B{241, 196, 15} : cocos2d::ccColor3B{231, 76, 60};
            }
            s_indicatorDot->setColor(c);
        }
    }

    if (s_startPosBar) {
        bool showBar = g_mh.startPosSwitcher && !g_mh.startPosList.empty();
        s_startPosBar->setVisible(showBar);
        if (showBar && s_startPosBarLbl) {
            std::ostringstream spTxt;
            spTxt << (g_mh.currentStartPosIdx + 1) << " / " << g_mh.startPosList.size();
            s_startPosBarLbl->setString(spTxt.str().c_str());
        }
    }

    if (!s_hudLabel) return;

    if (g_mh.hudAnchor == 0) {
        s_hudLabel->setAnchorPoint({0.0f, 1.0f});
        s_hudLabel->setPosition({14.0f, winSize.height - 6.0f});
    } else if (g_mh.hudAnchor == 1) {
        s_hudLabel->setAnchorPoint({1.0f, 1.0f});
        s_hudLabel->setPosition({winSize.width - 10.0f, winSize.height - 6.0f});
    } else if (g_mh.hudAnchor == 2) {
        s_hudLabel->setAnchorPoint({0.0f, 0.0f});
        s_hudLabel->setPosition({14.0f, 8.0f});
    } else {
        s_hudLabel->setAnchorPoint({0.5f, 1.0f});
        s_hudLabel->setPosition({winSize.width * 0.5f, winSize.height - 20.0f});
    }

    std::ostringstream ss;
    if (g_mh.showFps) {
        int fps = g_mh.frameExtrapolation
            ? static_cast<int>(g_mh.targetFps)
            : static_cast<int>(std::round(1.0f / std::max(dt, 0.0005f)));
        ss << "FPS: " << fps << " (240 TPS)\n";
    }
    if (g_mh.showCps) {
        float now = cocos2d::CCDirector::sharedDirector()->getTotalFrames() / 60.0f;
        g_mh.clickTimestamps.erase(
            std::remove_if(g_mh.clickTimestamps.begin(), g_mh.clickTimestamps.end(),
                           [now](float t) { return (now - t) > 1.0f; }),
            g_mh.clickTimestamps.end()
        );
        if (g_mh.clickTimestamps.size() > g_mh.peakCps) {
            g_mh.peakCps = g_mh.clickTimestamps.size();
        }
        ss << "CPS: " << g_mh.clickTimestamps.size() << " (Peak: " << g_mh.peakCps << ")\n";
    }
    if (g_mh.showNoclipAccuracy) {
        float acc = g_mh.totalFrames > 0
            ? (1.0f - static_cast<float>(g_mh.noclipHitFrames) / static_cast<float>(g_mh.totalFrames)) * 100.0f
            : 100.0f;
        ss << "Accuracy: " << std::fixed << std::setprecision(2) << std::clamp(acc, 0.0f, 100.0f) << "%\n";
    }
    if (g_mh.showNoclipDeaths) {
        ss << "Noclip Deaths: " << g_mh.noclipDeaths << "\n";
    }
    if (g_mh.showAttempts) {
        ss << "Attempt " << g_mh.attemptCount << "\n";
    }
    if (g_mh.showBestRun) {
        ss << "Best Run: " << static_cast<int>(g_mh.bestRunFrom) << "% - " << static_cast<int>(g_mh.bestRunTo) << "%\n";
    }

    s_hudLabel->setString(ss.str().c_str());
}

// ============================================================================
// 5. MASTER SCHEDULER HOOK (POLLS HARDWARE KEYS, SPEEDHACK, PHYSICS & RENDER)
// ============================================================================
class $modify(MHSchedulerHook, cocos2d::CCScheduler) {
    void update(float dt) {
        auto pl = PlayLayer::get();

#ifdef GEODE_IS_WINDOWS
        // 1. Poll Hardware Keys (Tab, C, Q, E, R, Jump)
        bool tabDown = (GetAsyncKeyState(0x09) & 0x8000) != 0; // VK_TAB
        if (tabDown && !g_mh.prevTabDown) {
            if (auto ui = MegaHackV9Overlay::getOrCreate()) {
                ui->toggleMenu();
            }
        }
        g_mh.prevTabDown = tabDown;

        bool cDown = (GetAsyncKeyState('C') & 0x8000) != 0;
        if (cDown && !g_mh.prevCDown && g_mh.frameStepper) {
            g_mh.stepRequested = true;
        }
        g_mh.prevCDown = cDown;

        bool qDown = (GetAsyncKeyState('Q') & 0x8000) != 0;
        if (qDown && !g_mh.prevQDown && g_mh.startPosSwitcher && pl) {
            switchStartPos(-1);
        }
        g_mh.prevQDown = qDown;

        bool eDown = (GetAsyncKeyState('E') & 0x8000) != 0;
        if (eDown && !g_mh.prevEDown && g_mh.startPosSwitcher && pl) {
            switchStartPos(+1);
        }
        g_mh.prevEDown = eDown;

        bool rDown = (GetAsyncKeyState('R') & 0x8000) != 0;
        if (rDown && !g_mh.prevRDown && g_mh.quickResetKey && pl && !g_mh.menuOpen) {
            pl->resetLevel();
        }
        g_mh.prevRDown = rDown;

        // Poll Jump Input (Mouse Left, Space, Up Arrow, W) for CPS, Shipcopter & Macro Bot
        bool jumpDown = ((GetAsyncKeyState(0x01) & 0x8000) != 0) ||
                        ((GetAsyncKeyState(0x20) & 0x8000) != 0) ||
                        ((GetAsyncKeyState(0x26) & 0x8000) != 0) ||
                        ((GetAsyncKeyState('W')  & 0x8000) != 0);

        if (pl && !g_mh.menuOpen) {
            if (jumpDown && !g_mh.prevJumpDown) {
                float now = cocos2d::CCDirector::sharedDirector()->getTotalFrames() / 60.0f;
                g_mh.clickTimestamps.push_back(now);
                if (g_mh.macroMode == 1) {
                    g_mh.macroBuffer.push_back({g_mh.physicsTick, true});
                }
            } else if (!jumpDown && g_mh.prevJumpDown) {
                if (g_mh.macroMode == 1) {
                    g_mh.macroBuffer.push_back({g_mh.physicsTick, false});
                }
            }
        }
        g_mh.prevJumpDown = jumpDown;
#endif

        // 2. Frame Stepper Freeze Logic
        float effectiveDt = dt;
        if (pl && g_mh.frameStepper && !g_mh.menuOpen) {
            if (g_mh.stepRequested) {
                g_mh.stepRequested = false;
                effectiveDt = 1.0f / 240.0f;
            } else {
                effectiveDt = 0.0f;
            }
        } else if (g_mh.speedhackEnabled && g_mh.speedhackValue > 0.01f) {
            effectiveDt *= g_mh.speedhackValue;
        }

        if (pl && effectiveDt > 0.0f && !g_mh.playerIsDead && !g_mh.menuOpen) {
            g_mh.physicsTick++;

            // 3. Auto Clicker Synthesis
            if (g_mh.autoClicker) {
                g_mh.autoClickAccum += effectiveDt;
                float halfPeriod = 0.5f / static_cast<float>(std::max(1, g_mh.autoClickCps));
                if (g_mh.autoClickAccum >= halfPeriod) {
                    g_mh.autoClickAccum = 0.0f;
                    g_mh.autoClickHeld = !g_mh.autoClickHeld;
                    synthesizeJumpHold(g_mh.autoClickHeld);
                }
            }

            // 4. Deterministic Macro Playback
            if (g_mh.macroMode == 2) {
                while (g_mh.macroCursor < g_mh.macroBuffer.size() &&
                       g_mh.macroBuffer[g_mh.macroCursor].tick <= g_mh.physicsTick) {
                    synthesizeJumpHold(g_mh.macroBuffer[g_mh.macroCursor].pressDown);
                    g_mh.macroCursor++;
                }
            }
        }

        g_mh.collidingThisFrame = false;
        CCScheduler::update(effectiveDt);

        // 5. Post-Tick PlayLayer Visuals & Physics Modifiers
        pl = PlayLayer::get();
        if (pl && effectiveDt > 0.0f) {
            g_mh.totalFrames++;

            if (g_mh.noMirrorPortal && pl->getScaleX() < 0.0f) {
                pl->setScaleX(std::abs(pl->getScaleX()));
            }
            if (g_mh.noCameraShake) {
                pl->setRotation(0.0f);
            }

            if (pl->m_player1 && !g_mh.playerIsDead) {
                // Shipcopter Free-Fly
                if (g_mh.shipcopter) {
                    float lift = g_mh.prevJumpDown ? 210.0f : -175.0f;
                    pl->m_player1->setPositionY(std::max(0.0f, pl->m_player1->getPositionY() + lift * effectiveDt));
                }

                // Force Ice Slippery Drift
                if (g_mh.forceIce) {
                    float targetSlide = g_mh.prevJumpDown ? 45.0f : -15.0f;
                    g_mh.iceDriftVelX = g_mh.iceDriftVelX * 0.96f + targetSlide * 0.04f;
                    pl->m_player1->setPositionY(std::max(0.0f, pl->m_player1->getPositionY() + g_mh.iceDriftVelX * effectiveDt * 0.25f));
                }

                // 2.0 Ball Rotation Bug
                if (g_mh.ballRotationBug) {
                    pl->m_player1->setRotation(pl->m_player1->getRotation() + 420.0f * effectiveDt);
                }

                // Force Mini Icon
                if (g_mh.forceMiniIcon) {
                    pl->m_player1->setScale(0.6f);
                    if (pl->m_player2) pl->m_player2->setScale(0.6f);
                }

                // Hide Player
                pl->m_player1->setVisible(!g_mh.hidePlayer);
                if (pl->m_player2) pl->m_player2->setVisible(!g_mh.hidePlayer);
            }

            // Frame Extrapolation Sub-Frame Smoothing
            if (g_mh.frameExtrapolation && pl->m_player1 && !g_mh.playerIsDead) {
                cocos2d::CCPoint cur = pl->m_player1->getPosition();
                cocos2d::CCPoint delta = {cur.x - g_mh.p1PrevPos.x, cur.y - g_mh.p1PrevPos.y};
                if (std::hypot(delta.x, delta.y) < 45.0f) {
                    g_mh.p1VelSmooth = {
                        g_mh.p1VelSmooth.x * 0.65f + delta.x * 0.35f,
                        g_mh.p1VelSmooth.y * 0.65f + delta.y * 0.35f
                    };
                    float alpha = std::clamp((effectiveDt * g_mh.targetFps) - 1.0f, 0.0f, 0.85f);
                    pl->m_player1->setPosition({
                        cur.x + g_mh.p1VelSmooth.x * alpha * 0.25f,
                        cur.y + g_mh.p1VelSmooth.y * alpha * 0.25f
                    });
                }
                g_mh.p1PrevPos = cur;
            } else if (pl->m_player1) {
                cocos2d::CCPoint cur = pl->m_player1->getPosition();
                g_mh.p1VelSmooth = {cur.x - g_mh.p1PrevPos.x, cur.y - g_mh.p1PrevPos.y};
                g_mh.p1PrevPos = cur;
            }

            if (g_mh.playerOnTop) {
                if (pl->m_player1) pl->m_player1->setZOrder(10000);
                if (pl->m_player2) pl->m_player2->setZOrder(10000);
            }

            // No Glow, No Particles & Layout Mode
            if (g_mh.noGlow || g_mh.noParticles || g_mh.layoutMode) {
                if (auto objLayer = pl->getChildByID("object-layer")) {
                    if (auto children = objLayer->getChildren()) {
                        for (unsigned int i = 0; i < children->count(); ++i) {
                            auto child = static_cast<cocos2d::CCNode*>(children->objectAtIndex(i));
                            if (!child || child == s_drawOverlay) continue;

                            if (g_mh.noParticles) {
                                if (auto part = dynamic_cast<cocos2d::CCParticleSystemQuad*>(child)) {
                                    part->setVisible(false);
                                }
                            }
                            if (g_mh.noGlow || g_mh.layoutMode) {
                                if (auto spr = dynamic_cast<cocos2d::CCSprite*>(child)) {
                                    if (spr->getBlendFunc().dst == GL_ONE) {
                                        spr->setVisible(false);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // P1 & P2 RGB Icons
            g_mh.rgbHue = std::fmod(g_mh.rgbHue + effectiveDt * 145.0f, 360.0f);
            auto c1 = hsvToRgb(g_mh.rgbHue, 0.85f, 1.0f);
            auto c2 = hsvToRgb(std::fmod(g_mh.rgbHue + 180.0f, 360.0f), 0.85f, 1.0f);

            if (pl->m_player1 && g_mh.p1RgbIcons) pl->m_player1->setColor(c1);
            if (pl->m_player2 && g_mh.p2RgbIcons) pl->m_player2->setColor(c2);

            // Accurate Percentage
            if (g_mh.accuratePercentage && pl->m_player1) {
                if (auto pctLbl = dynamic_cast<cocos2d::CCLabelBMFont*>(pl->getChildByID("percentage-label"))) {
                    float pct = std::clamp((pl->m_player1->getPositionX() / std::max(1.0f, g_mh.maxLevelX)) * 100.0f, 0.0f, 100.0f);
                    pctLbl->setString(formatFloat("", pct, "%", 2).c_str());
                }
            }

            // Hitbox Trail History
            if (g_mh.hitboxTrail && pl->m_player1 && !g_mh.playerIsDead) {
                g_mh.hitboxTrailHistory.push_back(pl->m_player1->boundingBox());
                if (g_mh.hitboxTrailHistory.size() > 45) {
                    g_mh.hitboxTrailHistory.pop_front();
                }
            }

            // Trajectory & Hitbox Overlay
            if (s_drawOverlay) {
                s_drawOverlay->clear();
                if (g_mh.showTrajectory && pl->m_player1 && !g_mh.playerIsDead) {
                    drawTrajectoryOverlay(pl->m_player1);
                }
                if ((g_mh.showHitboxes || (g_mh.showHitboxesOnDeath && g_mh.playerIsDead)) && pl->m_player1) {
                    drawHitboxOverlay(pl);
                }
            }

            updateStatusHudOverlay(effectiveDt);
        }
    }
};

// ============================================================================
// 6. CORE PLAYLAYER, PAUSELAYER & MENULAYER HOOKS
// ============================================================================
class $modify(MHPlayLayerHook, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        g_mh.inPlayLayerInit = true;
        bool res = PlayLayer::init(level, useReplay, dontCreateObjects);
        g_mh.inPlayLayerInit = false;
        if (!res) return false;

        s_drawOverlay = cocos2d::CCDrawNode::create();
        if (auto objLayer = this->getChildByID("object-layer")) {
            objLayer->addChild(s_drawOverlay, 9999);
        } else {
            this->addChild(s_drawOverlay, 9999);
        }

        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        s_hudLabel = cocos2d::CCLabelBMFont::create("", "chatFont.fnt");
        s_hudLabel->setScale(0.44f);
        s_hudLabel->setAnchorPoint({0.0f, 1.0f});
        s_hudLabel->setPosition({14.0f, winSize.height - 6.0f});
        s_hudLabel->setOpacity(225);
        this->addChild(s_hudLabel, 9998);

        s_indicatorDot = cocos2d::CCLayerColor::create({46, 204, 113, 255}, 5.0f, 5.0f);
        s_indicatorDot->setPosition({6.0f, winSize.height - 11.0f});
        s_indicatorDot->setVisible(false);
        this->addChild(s_indicatorDot, 9998);

        // On-Screen StartPos Switcher Bar (< 1 / 6 >)
        if (auto uiTarget = MegaHackV9Overlay::getOrCreate()) {
            s_startPosBar = cocos2d::CCNode::create();
            s_startPosBar->setPosition({winSize.width * 0.5f, 18.0f});
            s_startPosBar->setVisible(false);

            auto spMenu = cocos2d::CCMenu::create();
            spMenu->setPosition({0.0f, 0.0f});

            auto prevSpr = cocos2d::CCLabelBMFont::create("<", "bigFont.fnt");
            prevSpr->setScale(0.42f);
            auto prevBtn = cocos2d::CCMenuItemSprite::create(prevSpr, prevSpr, uiTarget, menu_selector(MegaHackV9Overlay::onPrevStartPosButton));
            prevBtn->setPosition({-42.0f, 0.0f});
            spMenu->addChild(prevBtn);

            auto nextSpr = cocos2d::CCLabelBMFont::create(">", "bigFont.fnt");
            nextSpr->setScale(0.42f);
            auto nextBtn = cocos2d::CCMenuItemSprite::create(nextSpr, nextSpr, uiTarget, menu_selector(MegaHackV9Overlay::onNextStartPosButton));
            nextBtn->setPosition({42.0f, 0.0f});
            spMenu->addChild(nextBtn);

            s_startPosBarLbl = cocos2d::CCLabelBMFont::create("1 / 6", "bigFont.fnt");
            s_startPosBarLbl->setScale(0.32f);
            s_startPosBarLbl->setOpacity(200);
            s_startPosBar->addChild(s_startPosBarLbl, 1);
            s_startPosBar->addChild(spMenu, 2);
            this->addChild(s_startPosBar, 9998);
        }

        g_mh.startPosList.clear();
        g_mh.currentStartPosIdx = 0;
        g_mh.maxLevelX = 2400.0f;
        g_mh.attemptCount = 1;
        g_mh.peakCps = 0;

        if (auto objLayer = this->getChildByID("object-layer")) {
            if (auto children = objLayer->getChildren()) {
                for (unsigned int i = 0; i < children->count(); ++i) {
                    if (auto node = dynamic_cast<cocos2d::CCNode*>(children->objectAtIndex(i))) {
                        if (node->getPositionX() > g_mh.maxLevelX) {
                            g_mh.maxLevelX = node->getPositionX();
                        }
                    }
                }
            }
        }
        for (float frac = 0.15f; frac <= 0.90f; frac += 0.15f) {
            g_mh.startPosList.push_back({g_mh.maxLevelX * frac, 105.0f});
        }

        return true;
    }

    void resetLevel() {
        if (g_mh.fixedRandomSeed) {
            std::srand(g_mh.randomSeed);
        }
        g_mh.inPlayLayerInit = true;
        PlayLayer::resetLevel();
        g_mh.inPlayLayerInit = false;

        g_mh.totalFrames = 0;
        g_mh.noclipHitFrames = 0;
        g_mh.noclipDeaths = 0;
        g_mh.collidingThisFrame = false;
        g_mh.playerIsDead = false;
        g_mh.physicsTick = 0;
        g_mh.macroCursor = 0;
        g_mh.attemptCount++;
        g_mh.hitboxTrailHistory.clear();

        if (this->m_player1) {
            g_mh.runStartPct = std::clamp((this->m_player1->getPositionX() / std::max(1.0f, g_mh.maxLevelX)) * 100.0f, 0.0f, 100.0f);
            if (g_mh.instantComplete && !g_mh.shouldProtectProgress()) {
                this->m_player1->setPositionX(g_mh.maxLevelX + 60.0f);
            }
        } else {
            g_mh.runStartPct = 0.0f;
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        auto objNode = static_cast<cocos2d::CCNode*>(obj);
        if (g_mh.inPlayLayerInit || g_mh.totalFrames < 4 || (objNode && objNode->getParent() == nullptr)) {
            PlayLayer::destroyPlayer(player, obj);
            return;
        }

        if (g_mh.noclip || g_mh.noCollision) {
            g_mh.noclipHitFrames++;
            if (!g_mh.collidingThisFrame) {
                g_mh.noclipDeaths++;
                g_mh.collidingThisFrame = true;
            }
            return;
        }

        if (g_mh.hitboxMultiplier && g_mh.hitboxScale < 1.0f && player && objNode) {
            cocos2d::CCRect oRect = objNode->boundingBox();
            float cx = oRect.getMidX();
            float cy = oRect.getMidY();
            float sw = oRect.size.width * g_mh.hitboxScale;
            float sh = oRect.size.height * g_mh.hitboxScale;
            cocos2d::CCRect scaledRect(cx - sw * 0.5f, cy - sh * 0.5f, sw, sh);
            if (!player->boundingBox().intersectsRect(scaledRect)) {
                return;
            }
        }

        g_mh.playerIsDead = true;
        if (this->m_player1) {
            float endPct = std::clamp((this->m_player1->getPositionX() / std::max(1.0f, g_mh.maxLevelX)) * 100.0f, 0.0f, 100.0f);
            if ((endPct - g_mh.runStartPct) > (g_mh.bestRunTo - g_mh.bestRunFrom)) {
                g_mh.bestRunFrom = g_mh.runStartPct;
                g_mh.bestRunTo = endPct;
            }
        }

        PlayLayer::destroyPlayer(player, obj);
    }
};

class $modify(MHPauseHook, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        if (auto uiTarget = MegaHackV9Overlay::getOrCreate()) {
            auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
            auto menu = cocos2d::CCMenu::create();
            menu->setPosition({0.f, 0.f});
            this->addChild(menu, 100);

            auto btn = createPillButton("MHv9 [Tab]", uiTarget, menu_selector(MegaHackV9Overlay::onExternalToggleButton));
            btn->setPosition({winSize.width - 42.0f, winSize.height - 18.0f});
            menu->addChild(btn);
        }
    }
};

class $modify(MHMenuHook, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        applyCustomMenuBg(this);

        if (auto uiTarget = MegaHackV9Overlay::getOrCreate()) {
            auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
            auto menu = cocos2d::CCMenu::create();
            menu->setPosition({0.f, 0.f});
            this->addChild(menu, 100);

            auto btn = createPillButton("MHv9 [Tab]", uiTarget, menu_selector(MegaHackV9Overlay::onExternalToggleButton));
            btn->setPosition({winSize.width - 42.0f, winSize.height - 18.0f});
            menu->addChild(btn);
        }

        return true;
    }
};
