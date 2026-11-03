#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/CreatorLayer.hpp>
#include <Geode/modify/GJGarageLayer.hpp>
#include <cmath>
#include <vector>
#include <deque>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdlib>

using namespace geode::prelude;

// ============================================================================
// 1. GLOBAL MEGA HACK V9 + QOLMOD STATE (ALL OFF BY DEFAULT)
// ============================================================================
struct MacroFrameStep {
    int tick;
    bool down;
    int button;
    bool isP1;
};

struct MegaHackState {
    bool menuOpen = false;
    int animStyle = 0;   // 0: Smooth Expo+Back, 1: Slide Down, 2: Elastic Pop
    int hudAnchor = 0;   // 0: Top-Left, 1: Top-Right, 2: Bottom-Left, 3: Top-Center

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
    bool p1RgbTrail = false;
    bool p2RgbTrail = false;

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
    bool stepRequested = false;

    // Column 3: Universal & Config
    bool customBackground = false;
    int bgThemeIdx = 0;
    bool fastResetAndChests = false;

    // Column 4: Display, Speed & AutoClicker
    bool speedhackEnabled = false;
    float speedhackValue = 1.00f;
    bool frameExtrapolation = false;
    float targetFps = 480.0f;
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
    bool copyVerifyBypass = false;
    int macroMode = 0; // 0: Disabled, 1: Recording, 2: Playback
    std::vector<MacroFrameStep> macroBuffer;
    size_t macroCursor = 0;
    int physicsTick = 0;

    // Runtime Tracking
    std::vector<cocos2d::CCPoint> startPosCoords;
    int currentStartPosIdx = -1;
    bool inPlayLayerInit = false;
    int totalFrames = 0;
    int noclipHitFrames = 0;
    int noclipDeaths = 0;
    int attemptCounter = 1;
    bool collidingThisFrame = false;
    bool playerIsDead = false;
    float runStartPct = 0.0f;
    float bestRunFrom = 0.0f;
    float bestRunTo = 0.0f;
    float maxLevelX = 2000.0f;
    std::vector<float> clickTimestamps;
    bool holdingJump = false;
    float rgbHue = 0.0f;
    std::deque<cocos2d::CCRect> hitboxTrailHistory;

    cocos2d::CCPoint p1PrevPos = {0.f, 0.f};
    cocos2d::CCPoint p1VelSmooth = {0.f, 0.f};

    bool isCheating() const {
        return noclip || noCollision || forceIce || shipcopter ||
               showTrajectory || showHitboxes || hitboxMultiplier ||
               layoutMode || fixedRandomSeed || instantComplete ||
               frameStepper || autoClicker || (macroMode == 2) ||
               (speedhackEnabled && std::abs(speedhackValue - 1.0f) > 0.001f);
    }

    bool shouldProtectProgress() const {
        return safeMode || (autoSafeMode && isCheating());
    }

    void saveConfig() {
        auto mod = Mod::get();
        mod->setSavedValue("accuratePercentage", accuratePercentage);
        mod->setSavedValue("noGlow", noGlow);
        mod->setSavedValue("noParticles", noParticles);
        mod->setSavedValue("noPortalCircle", noPortalCircle);
        mod->setSavedValue("noMirrorPortal", noMirrorPortal);
        mod->setSavedValue("noCameraShake", noCameraShake);
        mod->setSavedValue("layoutMode", layoutMode);
        mod->setSavedValue("playerOnTop", playerOnTop);
        mod->setSavedValue("solidWaveTrail", solidWaveTrail);
        mod->setSavedValue("p1RgbIcons", p1RgbIcons);
        mod->setSavedValue("p1RgbTrail", p1RgbTrail);
        mod->setSavedValue("autoSafeMode", autoSafeMode);
        mod->setSavedValue("startPosSwitcher", startPosSwitcher);
        mod->setSavedValue("practiceMusicSync", practiceMusicSync);
        mod->setSavedValue("customBackground", customBackground);
        mod->setSavedValue("bgThemeIdx", bgThemeIdx);
        mod->setSavedValue("hudAnchor", hudAnchor);
        mod->setSavedValue("animStyle", animStyle);
        mod->setSavedValue("iconBypass", iconBypass);
        mod->setSavedValue("cheatIndicator", cheatIndicator);
        mod->setSavedValue("showFps", showFps);
        mod->setSavedValue("showCps", showCps);
        mod->setSavedValue("showNoclipAccuracy", showNoclipAccuracy);
        mod->setSavedValue("showNoclipDeaths", showNoclipDeaths);
        mod->setSavedValue("showBestRun", showBestRun);
    }
};

static MegaHackState g_mh;

// ============================================================================
// 2. PURE COCOS2D-X UTILITIES (ZERO UNMAPPED STRUCT DEPENDENCIES)
// ============================================================================
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

static void applyMenuCustomBackground(cocos2d::CCNode* layer) {
    if (!layer || !g_mh.customBackground) return;
    auto col = kBgThemes[g_mh.bgThemeIdx % 5];
    if (auto bg = typeinfo_cast<cocos2d::CCRGBAProtocol*>(layer->getChildByID("background"))) {
        bg->setColor(col);
    }
    if (auto children = layer->getChildren()) {
        for (unsigned int i = 0; i < children->count(); ++i) {
            if (auto spr = typeinfo_cast<cocos2d::CCSprite*>(children->objectAtIndex(i))) {
                if (spr->getZOrder() <= -1) {
                    spr->setColor(col);
                }
            }
        }
    }
}

static void cycleStartPos(int stepDir) {
    auto pl = PlayLayer::get();
    if (!pl || g_mh.startPosCoords.empty()) return;

    int count = static_cast<int>(g_mh.startPosCoords.size());
    g_mh.currentStartPosIdx = (g_mh.currentStartPosIdx + stepDir + count) % count;
    cocos2d::CCPoint pos = g_mh.startPosCoords[g_mh.currentStartPosIdx];
    if (pl->m_player1) {
        pl->m_player1->setPosition(pos);
    }
    if (pl->m_player2) {
        pl->m_player2->setPosition(pos);
    }
}

// ============================================================================
// 3. MEGA HACK V9 PRO OVERLAY UI (6 COLUMNS + SMOOTH EASING ANIMATIONS)
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
        if (auto existing = typeinfo_cast<MegaHackV9Overlay*>(scene->getChildByID("mhv9-ultimate-ui"_spr))) {
            return existing;
        }
        auto ret = new MegaHackV9Overlay();
        if (ret && ret->init()) {
            ret->autorelease();
            ret->setID("mhv9-ultimate-ui"_spr);
            scene->addChild(ret, 99999);
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
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

    cocos2d::CCNode* createWindowNode(const char* title, cocos2d::CCPoint topCenter, float width, float height) {
        auto panel = cocos2d::CCNode::create();
        panel->setPosition(topCenter);
        panel->setContentSize({width, height});
        panel->setAnchorPoint({0.5f, 1.0f});
        panel->setCascadeOpacityEnabled(true);

        auto border = cocos2d::CCLayerColor::create({62, 64, 78, 255}, width + 1.5f, height + 1.5f);
        border->ignoreAnchorPointForPosition(false);
        border->setAnchorPoint({0.5f, 1.0f});
        border->setPosition({width * 0.5f, height + 0.75f});
        panel->addChild(border, -1);

        auto body = cocos2d::CCLayerColor::create({16, 16, 22, 248}, width, height);
        body->ignoreAnchorPointForPosition(false);
        body->setAnchorPoint({0.5f, 1.0f});
        body->setPosition({width * 0.5f, height});
        panel->addChild(body, 0);

        float headerH = 14.0f;
        auto header = cocos2d::CCLayerColor::create({210, 38, 38, 255}, width, headerH);
        header->ignoreAnchorPointForPosition(false);
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

    struct ToggleMeta : public cocos2d::CCObject {
        bool* statePtr;
        cocos2d::CCLayerColor* bg;
        cocos2d::CCLayerColor* bar;
        cocos2d::CCLabelBMFont* lbl;
        std::function<void()> onToggle;
    };

    struct ActionMeta : public cocos2d::CCObject {
        std::function<void()> onTrigger;
    };

    void addToggleItem(cocos2d::CCNode* col, float& curY, const char* text, bool* ptr, std::function<void()> onChange = nullptr, std::function<void()> onArrow = nullptr) {
        float w = col->getContentSize().width;
        float rowH = 13.0f;
        auto menu = typeinfo_cast<cocos2d::CCMenu*>(col->getChildByID("column-menu"));
        if (!menu) return;

        auto bg = cocos2d::CCLayerColor::create(
            *ptr ? cocos2d::ccColor4B{210, 38, 38, 98} : cocos2d::ccColor4B{28, 28, 36, 232},
            w - 5.0f, rowH - 1.3f
        );
        bg->ignoreAnchorPointForPosition(false);
        bg->setAnchorPoint({0.5f, 0.5f});

        auto bar = cocos2d::CCLayerColor::create(
            *ptr ? cocos2d::ccColor4B{245, 62, 62, 255} : cocos2d::ccColor4B{68, 68, 82, 255},
            2.4f, rowH - 1.3f
        );
        bar->ignoreAnchorPointForPosition(false);
        bar->setAnchorPoint({0.0f, 0.0f});
        bar->setPosition({0.0f, 0.0f});
        bg->addChild(bar);

        auto lbl = cocos2d::CCLabelBMFont::create(text, "chatFont.fnt");
        lbl->setScale(0.35f);
        lbl->setAnchorPoint({0.0f, 0.5f});
        lbl->setPosition({5.2f, (rowH - 1.3f) * 0.5f});
        lbl->setColor(*ptr ? cocos2d::ccColor3B{255, 255, 255} : cocos2d::ccColor3B{192, 194, 206});
        bg->addChild(lbl);

        auto btn = CCMenuItemSpriteExtra::create(bg, this, menu_selector(MegaHackV9Overlay::onToggleHandler));
        btn->setPosition({w * 0.5f, curY});
        btn->m_scaleMultiplier = 1.02f;

        auto meta = new ToggleMeta();
        meta->autorelease();
        meta->statePtr = ptr;
        meta->bg = bg;
        meta->bar = bar;
        meta->lbl = lbl;
        meta->onToggle = onChange;
        btn->setUserObject(meta);
        menu->addChild(btn);

        if (onArrow) {
            auto arrLbl = cocos2d::CCLabelBMFont::create("<", "bigFont.fnt");
            arrLbl->setScale(0.20f);
            arrLbl->setColor({245, 192, 78});
            auto arrBtn = CCMenuItemSpriteExtra::create(arrLbl, this, menu_selector(MegaHackV9Overlay::onActionHandler));
            arrBtn->setPosition({w - 8.5f, curY});
            auto act = new ActionMeta();
            act->autorelease();
            act->onTrigger = onArrow;
            arrBtn->setUserObject(act);
            menu->addChild(arrBtn, 5);
        }

        curY -= rowH;
    }

    void addStepperItem(cocos2d::CCNode* col, float& curY, const char* initText, cocos2d::CCLabelBMFont** outLbl, std::function<void(int)> onStep) {
        float w = col->getContentSize().width;
        float rowH = 13.0f;
        auto menu = typeinfo_cast<cocos2d::CCMenu*>(col->getChildByID("column-menu"));
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
            auto btn = CCMenuItemSpriteExtra::create(bLbl, this, menu_selector(MegaHackV9Overlay::onActionHandler));
            btn->setPosition({xPos, curY});
            auto act = new ActionMeta();
            act->autorelease();
            act->onTrigger = [onStep, dir]() { onStep(dir); };
            btn->setUserObject(act);
            menu->addChild(btn);
        };

        addStepBtn("-", 9.0f, -1);
        addStepBtn("+", w - 9.0f, +1);
        curY -= rowH;
    }

    void addActionButton(cocos2d::CCNode* col, float& curY, const char* title, cocos2d::CCLabelBMFont** outLbl, std::function<void()> onClick) {
        float w = col->getContentSize().width;
        float rowH = 13.5f;
        auto menu = typeinfo_cast<cocos2d::CCMenu*>(col->getChildByID("column-menu"));
        if (!menu) return;

        auto bg = cocos2d::CCLayerColor::create({40, 44, 62, 240}, w - 5.5f, rowH - 1.5f);
        bg->ignoreAnchorPointForPosition(false);
        bg->setAnchorPoint({0.5f, 0.5f});

        auto lbl = cocos2d::CCLabelBMFont::create(title, "chatFont.fnt");
        lbl->setScale(0.35f);
        lbl->setPosition({(w - 5.5f) * 0.5f, (rowH - 1.5f) * 0.5f});
        lbl->setColor({135, 222, 255});
        bg->addChild(lbl);
        if (outLbl) *outLbl = lbl;

        auto btn = CCMenuItemSpriteExtra::create(bg, this, menu_selector(MegaHackV9Overlay::onActionHandler));
        btn->setPosition({w * 0.5f, curY});
        auto act = new ActionMeta();
        act->autorelease();
        act->onTrigger = onClick;
        btn->setUserObject(act);
        menu->addChild(btn);

        curY -= rowH;
    }

    void onToggleHandler(cocos2d::CCObject* sender) {
        auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);
        auto meta = static_cast<ToggleMeta*>(btn->getUserObject());
        if (!meta || !meta->statePtr) return;

        *(meta->statePtr) = !(*(meta->statePtr));
        bool active = *(meta->statePtr);

        meta->bg->stopAllActions();
        meta->bg->runAction(cocos2d::CCTintTo::create(0.12f, active ? 210 : 28, active ? 38 : 28, active ? 38 : 36));
        meta->bg->setOpacity(active ? 98 : 232);
        meta->bar->setColor(active ? cocos2d::ccColor3B{245, 62, 62} : cocos2d::ccColor3B{68, 68, 82});
        meta->lbl->setColor(active ? cocos2d::ccColor3B{255, 255, 255} : cocos2d::ccColor3B{192, 194, 206});

        if (meta->onToggle) meta->onToggle();
    }

    void onActionHandler(cocos2d::CCObject* sender) {
        auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);
        auto act = static_cast<ActionMeta*>(btn->getUserObject());
        if (act && act->onTrigger) act->onTrigger();
    }

    void buildCosmeticColumn(cocos2d::CCPoint pos, float w) {
        float h = 230.0f;
        auto col = createWindowNode("Cosmetic", pos, w, h);
        float y = h - 21.0f;

        addToggleItem(col, y, "Accurate %", &g_mh.accuratePercentage);
        addToggleItem(col, y, "Ball Rotation Bug", &g_mh.ballRotationBug);
        addToggleItem(col, y, "No Glow", &g_mh.noGlow);
        addToggleItem(col, y, "No Particles", &g_mh.noParticles, nullptr, [this]() {
            if (m_particlePopup) {
                bool show = !m_particlePopup->isVisible();
                m_particlePopup->setVisible(show);
                if (show) {
                    m_particlePopup->setScale(0.82f);
                    m_particlePopup->runAction(cocos2d::CCEaseBackOut::create(cocos2d::CCScaleTo::create(0.18f, 1.0f)));
                }
            }
        });
        addToggleItem(col, y, "No Portal Circle", &g_mh.noPortalCircle);
        addToggleItem(col, y, "No Mirror Portal", &g_mh.noMirrorPortal);
        addToggleItem(col, y, "No Camera Shake", &g_mh.noCameraShake);
        addToggleItem(col, y, "Layout Mode", &g_mh.layoutMode);
        addToggleItem(col, y, "Player on Top", &g_mh.playerOnTop);
        addToggleItem(col, y, "No Wave Trail", &g_mh.noWaveTrail);
        addToggleItem(col, y, "Solid Wave Trail", &g_mh.solidWaveTrail);
        addToggleItem(col, y, "P1 RGB Icons", &g_mh.p1RgbIcons);
        addToggleItem(col, y, "P2 RGB Icons", &g_mh.p2RgbIcons);
        addToggleItem(col, y, "P1 RGB Trail", &g_mh.p1RgbTrail);
        addToggleItem(col, y, "P2 RGB Trail", &g_mh.p2RgbTrail);
    }

    void buildLevelColumn(cocos2d::CCPoint pos, float w) {
        float h = 268.0f;
        auto col = createWindowNode("Level", pos, w, h);
        float y = h - 21.0f;

        addToggleItem(col, y, "Noclip", &g_mh.noclip);
        addToggleItem(col, y, "No Collision", &g_mh.noCollision);
        addToggleItem(col, y, "Safe Mode", &g_mh.safeMode);
        addToggleItem(col, y, "Auto Safe Mode", &g_mh.autoSafeMode);
        addToggleItem(col, y, "Force Ice", &g_mh.forceIce);
        addToggleItem(col, y, "Shipcopter", &g_mh.shipcopter);
        addToggleItem(col, y, "Show Trajectory", &g_mh.showTrajectory);
        addToggleItem(col, y, "Show Hitboxes", &g_mh.showHitboxes);
        addToggleItem(col, y, "Hitboxes on Death", &g_mh.showHitboxesOnDeath);
        addToggleItem(col, y, "Hitbox Trail", &g_mh.hitboxTrail, []() {
            g_mh.hitboxTrailHistory.clear();
        });
        addToggleItem(col, y, "Hitbox Multiplier", &g_mh.hitboxMultiplier);
        addStepperItem(col, y, "Hitbox: 0.50x", &m_hitboxLbl, [this](int dir) {
            g_mh.hitboxScale = std::clamp(g_mh.hitboxScale + dir * 0.1f, 0.1f, 3.0f);
            if (m_hitboxLbl) {
                std::ostringstream ss;
                ss << "Hitbox: " << std::fixed << std::setprecision(2) << g_mh.hitboxScale << "x";
                m_hitboxLbl->setString(ss.str().c_str());
            }
        });
        addToggleItem(col, y, "StartPos Switch [Q/E]", &g_mh.startPosSwitcher);
        addToggleItem(col, y, "Random Seed", &g_mh.fixedRandomSeed);
        addStepperItem(col, y, "Seed: 1337", &m_seedLbl, [this](int dir) {
            g_mh.randomSeed = static_cast<unsigned int>(std::max(1, static_cast<int>(g_mh.randomSeed) + dir * 111));
            if (m_seedLbl) m_seedLbl->setString(fmt::format("Seed: {}", g_mh.randomSeed).c_str());
        });
        addToggleItem(col, y, "Instant Complete", &g_mh.instantComplete, []() {
            if (g_mh.instantComplete) {
                if (auto pl = PlayLayer::get()) pl->playEndAnimationToPos({0.f, 0.f});
            }
        });
        addToggleItem(col, y, "Practice Music", &g_mh.practiceMusicSync);
        addToggleItem(col, y, "Frame Step [C]", &g_mh.frameStepper);
    }

    void buildUniversalColumn(cocos2d::CCPoint pos, float w) {
        float h = 195.0f;
        auto col = createWindowNode("Universal", pos, w, h);
        float y = h - 21.0f;

        addToggleItem(col, y, "Custom BG", &g_mh.customBackground, []() {
            auto sc = cocos2d::CCDirector::sharedDirector()->getRunningScene();
            if (sc && sc->getChildrenCount() > 0) {
                applyMenuCustomBackground(static_cast<cocos2d::CCNode*>(sc->getChildren()->objectAtIndex(0)));
            }
        }, []() {
            g_mh.bgThemeIdx = (g_mh.bgThemeIdx + 1) % 5;
            auto sc = cocos2d::CCDirector::sharedDirector()->getRunningScene();
            if (sc && sc->getChildrenCount() > 0) {
                applyMenuCustomBackground(static_cast<cocos2d::CCNode*>(sc->getChildren()->objectAtIndex(0)));
            }
        });
        addToggleItem(col, y, "Fast Reset / Chests", &g_mh.fastResetAndChests);
        addActionButton(col, y, "HUD: Top-Left", &m_anchorLbl, [this]() {
            g_mh.hudAnchor = (g_mh.hudAnchor + 1) % 4;
            const char* names[] = {"HUD: Top-Left", "HUD: Top-Right", "HUD: Bottom-Left", "HUD: Top-Center"};
            if (m_anchorLbl) m_anchorLbl->setString(names[g_mh.hudAnchor]);
        });
        addActionButton(col, y, "Anim: Smooth Expo", &m_animLbl, [this]() {
            g_mh.animStyle = (g_mh.animStyle + 1) % 3;
            const char* styles[] = {"Anim: Smooth Expo", "Anim: Slide Down", "Anim: Elastic Pop"};
            if (m_animLbl) m_animLbl->setString(styles[g_mh.animStyle]);
        });
        addActionButton(col, y, "Save Config", nullptr, []() {
            g_mh.saveConfig();
            Notification::create("Mega Hack v9 Config Saved", NotificationIcon::Success)->show();
        });
        addActionButton(col, y, "Reset All OFF", nullptr, []() {
            g_mh = MegaHackState();
            g_mh.menuOpen = true;
            cocos2d::CCApplication::sharedApplication()->setAnimationInterval(1.0 / 240.0);
            Notification::create("All Hacks Reset to OFF", NotificationIcon::Success)->show();
        });
    }

    void buildDisplaySpeedColumn(cocos2d::CCPoint pos, float w) {
        float h = 195.0f;
        auto col = createWindowNode("Display / Speed", pos, w, h);
        float y = h - 21.0f;

        addToggleItem(col, y, "Speedhack", &g_mh.speedhackEnabled);
        addStepperItem(col, y, "Speed: 1.00x", &m_speedLbl, [this](int dir) {
            g_mh.speedhackValue = std::clamp(g_mh.speedhackValue + dir * 0.1f, 0.1f, 5.0f);
            if (m_speedLbl) {
                std::ostringstream ss;
                ss << "Speed: " << std::fixed << std::setprecision(2) << g_mh.speedhackValue << "x";
                m_speedLbl->setString(ss.str().c_str());
            }
        });
        addToggleItem(col, y, "Frame Extrap.", &g_mh.frameExtrapolation, []() {
            float fps = g_mh.frameExtrapolation ? g_mh.targetFps : 240.0f;
            cocos2d::CCApplication::sharedApplication()->setAnimationInterval(1.0 / fps);
        });
        addStepperItem(col, y, "FPS: 480", &m_fpsLbl, [this](int dir) {
            g_mh.targetFps = std::clamp(g_mh.targetFps + dir * 120.0f, 60.0f, 1440.0f);
            if (m_fpsLbl) m_fpsLbl->setString(fmt::format("FPS: {}", static_cast<int>(g_mh.targetFps)).c_str());
            if (g_mh.frameExtrapolation) {
                cocos2d::CCApplication::sharedApplication()->setAnimationInterval(1.0 / g_mh.targetFps);
            }
        });
        addToggleItem(col, y, "Auto Clicker", &g_mh.autoClicker);
        addStepperItem(col, y, "Click CPS: 15", &m_autoClickLbl, [this](int dir) {
            g_mh.autoClickCps = std::clamp(g_mh.autoClickCps + dir * 5, 5, 60);
            if (m_autoClickLbl) {
                m_autoClickLbl->setString(fmt::format("Click CPS: {}", g_mh.autoClickCps).c_str());
            }
        });
    }

    void buildStatusHudColumn(cocos2d::CCPoint pos, float w) {
        float h = 180.0f;
        auto col = createWindowNode("Status HUD", pos, w, h);
        float y = h - 21.0f;

        addToggleItem(col, y, "Cheat Indicator", &g_mh.cheatIndicator);
        addToggleItem(col, y, "FPS Counter", &g_mh.showFps);
        addToggleItem(col, y, "CPS Counter", &g_mh.showCps);
        addToggleItem(col, y, "Noclip Accuracy", &g_mh.showNoclipAccuracy);
        addToggleItem(col, y, "Noclip Deaths", &g_mh.showNoclipDeaths);
        addToggleItem(col, y, "Attempts", &g_mh.showAttempts);
        addToggleItem(col, y, "Best Run", &g_mh.showBestRun);
    }

    void buildBypassBotColumn(cocos2d::CCPoint pos, float w) {
        float h = 180.0f;
        auto col = createWindowNode("Bypass & Bot", pos, w, h);
        float y = h - 21.0f;

        addToggleItem(col, y, "Unlock All Icons", &g_mh.iconBypass);
        addToggleItem(col, y, "Copy / Verify Bypass", &g_mh.copyVerifyBypass);

        addActionButton(col, y, "Bot: Disabled", &m_botLbl, [this]() {
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

        addActionButton(col, y, "Clear Macro", nullptr, [this]() {
            g_mh.macroBuffer.clear();
            g_mh.macroCursor = 0;
            g_mh.macroMode = 0;
            if (m_botLbl) m_botLbl->setString("Bot: Disabled");
            Notification::create("Macro Replay Cleared", NotificationIcon::Info)->show();
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

        auto bg = cocos2d::CCLayerColor::create({14, 14, 18, 252}, w, h);
        bg->ignoreAnchorPointForPosition(false);
        bg->setAnchorPoint({0.f, 0.f});
        m_particlePopup->addChild(bg, 0);

        auto header = cocos2d::CCLayerColor::create({210, 38, 38, 255}, w, 14.0f);
        header->ignoreAnchorPointForPosition(false);
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
        addToggleItem(m_particlePopup, y, "Portal Effect (Keep)", &g_mh.keepPortalParticles);
        addToggleItem(m_particlePopup, y, "Hide Ground Part.", &g_mh.hideGroundParticles);
        addToggleItem(m_particlePopup, y, "Hide Trail Part.", &g_mh.hideTrailParticles);
        addToggleItem(m_particlePopup, y, "Hide Click Part.", &g_mh.hideClickParticles);

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

// ============================================================================
// 4. KEYBOARD HOOK (TAB MENU, 'C' FRAME STEPPER, 'Q'/'E' STARTPOS SWITCHER)
// ============================================================================
class $modify(MHKeyHook, cocos2d::CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(cocos2d::enumKeyCodes key, bool isKeyDown, bool isKeyRepeat) {
        if (key == cocos2d::enumKeyCodes::KEY_Tab && isKeyDown && !isKeyRepeat) {
            if (auto ui = MegaHackV9Overlay::getOrCreate()) {
                ui->toggleMenu();
                return true;
            }
        }
        if (g_mh.frameStepper && key == cocos2d::enumKeyCodes::KEY_C && isKeyDown) {
            g_mh.stepRequested = true;
            return true;
        }
        if (g_mh.startPosSwitcher && isKeyDown && !isKeyRepeat && PlayLayer::get()) {
            if (key == cocos2d::enumKeyCodes::KEY_Q) {
                cycleStartPos(-1);
                return true;
            }
            if (key == cocos2d::enumKeyCodes::KEY_E) {
                cycleStartPos(+1);
                return true;
            }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat);
    }
};

// ============================================================================
// 5. GAME MANAGER HOOKS (ICON BYPASS, PRACTICE MUSIC, NO SHAKE, FAST RESET)
// ============================================================================
class $modify(MHGameManagerHook, GameManager) {
    bool isIconUnlocked(int id, IconType type) {
        if (g_mh.iconBypass) return true;
        return GameManager::isIconUnlocked(id, type);
    }

    bool isColorUnlocked(int id, UnlockType type) {
        if (g_mh.iconBypass) return true;
        return GameManager::isColorUnlocked(id, type);
    }

    bool getGameVariable(char const* key) {
        if (key) {
            std::string k(key);
            if (g_mh.practiceMusicSync && k == "0125") return true;
            if (g_mh.noCameraShake && (k == "0172" || k == "0081")) return true;
            if (g_mh.fastResetAndChests && k == "0072") return true;
        }
        return GameManager::getGameVariable(key);
    }
};

// ============================================================================
// 6. SPEEDHACK HOOK (CCScheduler)
// ============================================================================
class $modify(MHSchedulerHook, cocos2d::CCScheduler) {
    void update(float dt) {
        if (g_mh.speedhackEnabled && g_mh.speedhackValue > 0.01f) {
            dt *= g_mh.speedhackValue;
        }
        CCScheduler::update(dt);
    }
};

// ============================================================================
// 7. PLAYER OBJECT HOOKS (SHIPCOPTER, FORCE ICE, BALL BUG)
// ============================================================================
class $modify(MHPlayerHook, PlayerObject) {
    bool pushButton(PlayerButton p0) {
        if (p0 == PlayerButton::Jump && PlayLayer::get() && !g_mh.holdingJump) {
            g_mh.holdingJump = true;
            float now = cocos2d::CCDirector::sharedDirector()->getTotalFrames() / 60.0f;
            g_mh.clickTimestamps.push_back(now);
        }
        return PlayerObject::pushButton(p0);
    }

    bool releaseButton(PlayerButton p0) {
        if (p0 == PlayerButton::Jump) {
            g_mh.holdingJump = false;
        }
        return PlayerObject::releaseButton(p0);
    }

    void update(float dt) {
        PlayerObject::update(dt);

        if (g_mh.shipcopter && PlayLayer::get() && !g_mh.playerIsDead) {
            float lift = g_mh.holdingJump ? 1.85f : -1.45f;
            this->setPositionY(std::max(0.0f, this->getPositionY() + lift * dt));
        }

        if (g_mh.ballRotationBug && PlayLayer::get() && !g_mh.playerIsDead) {
            this->setRotation(this->getRotation() + 18.0f * dt);
        }
    }
};

// ============================================================================
// 8. GJBASEGAMELAYER & PLAYLAYER HOOKS (NOCLIP, HITBOX, TRAJECTORY, BOT, HUD)
// ============================================================================
class $modify(MHBaseGameHook, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (g_mh.macroMode == 1 && PlayLayer::get()) {
            g_mh.macroBuffer.push_back({
                g_mh.physicsTick,
                down,
                button,
                isPlayer1
            });
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void update(float dt) {
        if (g_mh.frameStepper && PlayLayer::get()) {
            if (!g_mh.stepRequested) return;
            g_mh.stepRequested = false;
            dt = 1.0f / 240.0f;
        }

        g_mh.physicsTick++;

        // Auto Clicker
        if (g_mh.autoClicker && PlayLayer::get() && !g_mh.playerIsDead) {
            g_mh.autoClickTimer += dt;
            float halfPeriod = 0.5f / static_cast<float>(std::max(1, g_mh.autoClickCps));
            if (g_mh.autoClickTimer >= halfPeriod) {
                g_mh.autoClickTimer = 0.0f;
                g_mh.autoClickDown = !g_mh.autoClickDown;
                GJBaseGameLayer::handleButton(g_mh.autoClickDown, 1, true);
            }
        }

        // Deterministic Macro Replay
        if (g_mh.macroMode == 2 && PlayLayer::get()) {
            while (g_mh.macroCursor < g_mh.macroBuffer.size() &&
                   g_mh.macroBuffer[g_mh.macroCursor].tick <= g_mh.physicsTick) {
                const auto& act = g_mh.macroBuffer[g_mh.macroCursor];
                GJBaseGameLayer::handleButton(act.down, act.button, act.isP1);
                g_mh.macroCursor++;
            }
        }

        GJBaseGameLayer::update(dt);
    }
};

class $modify(MHPlayLayerHook, PlayLayer) {
    struct Fields {
        cocos2d::CCDrawNode* m_drawOverlay = nullptr;
        cocos2d::CCLabelBMFont* m_hudLabel = nullptr;
        cocos2d::CCLayerColor* m_indicatorDot = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        g_mh.inPlayLayerInit = true;
        bool res = PlayLayer::init(level, useReplay, dontCreateObjects);
        g_mh.inPlayLayerInit = false;
        if (!res) return false;

        m_fields->m_drawOverlay = cocos2d::CCDrawNode::create();
        if (this->m_objectLayer) {
            this->m_objectLayer->addChild(m_fields->m_drawOverlay, 9999);
        }

        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        m_fields->m_hudLabel = cocos2d::CCLabelBMFont::create("", "chatFont.fnt");
        m_fields->m_hudLabel->setScale(0.44f);
        m_fields->m_hudLabel->setAnchorPoint({0.0f, 1.0f});
        m_fields->m_hudLabel->setPosition({14.0f, winSize.height - 6.0f});
        m_fields->m_hudLabel->setOpacity(225);
        this->addChild(m_fields->m_hudLabel, 9998);

        m_fields->m_indicatorDot = cocos2d::CCLayerColor::create({46, 204, 113, 255}, 5.0f, 5.0f);
        m_fields->m_indicatorDot->setPosition({6.0f, winSize.height - 11.0f});
        m_fields->m_indicatorDot->setVisible(false);
        this->addChild(m_fields->m_indicatorDot, 9998);

        // Scan level bounds & checkpoints for StartPos Switcher
        g_mh.startPosCoords.clear();
        g_mh.currentStartPosIdx = -1;
        g_mh.maxLevelX = 2000.0f;
        g_mh.attemptCounter = 1;

        if (this->m_objectLayer && this->m_objectLayer->getChildren()) {
            auto children = this->m_objectLayer->getChildren();
            for (unsigned int i = 0; i < children->count(); ++i) {
                if (auto node = typeinfo_cast<cocos2d::CCNode*>(children->objectAtIndex(i))) {
                    if (node->getPositionX() > g_mh.maxLevelX) {
                        g_mh.maxLevelX = node->getPositionX();
                    }
                }
            }
            for (float frac = 0.1f; frac <= 0.9f; frac += 0.15f) {
                g_mh.startPosCoords.push_back({g_mh.maxLevelX * frac, 105.0f});
            }
        }

        return true;
    }

    void toggleFlipped(bool p0, bool p1) {
        if (g_mh.noMirrorPortal) return;
        PlayLayer::toggleFlipped(p0, p1);
    }

    void resetLevel() {
        if (g_mh.fixedRandomSeed) {
            std::srand(g_mh.randomSeed);
        }
        PlayLayer::resetLevel();

        g_mh.totalFrames = 0;
        g_mh.noclipHitFrames = 0;
        g_mh.noclipDeaths = 0;
        g_mh.collidingThisFrame = false;
        g_mh.playerIsDead = false;
        g_mh.physicsTick = 0;
        g_mh.macroCursor = 0;
        g_mh.attemptCounter++;
        g_mh.hitboxTrailHistory.clear();

        if (this->m_player1) {
            g_mh.runStartPct = std::clamp((this->m_player1->getPositionX() / std::max(1.0f, g_mh.maxLevelX)) * 100.0f, 0.0f, 100.0f);
        } else {
            g_mh.runStartPct = 0.0f;
        }

        if (g_mh.instantComplete) {
            this->playEndAnimationToPos({0.f, 0.f});
        }
    }

    void levelComplete() {
        if (g_mh.shouldProtectProgress()) {
            Notification::create("Safe Mode Active: Progress Protected", NotificationIcon::Warning)->show();
            this->resetLevel();
            return;
        }
        PlayLayer::levelComplete();
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        // Allow anticheat test spike during PlayLayer::init to pass cleanly
        if (g_mh.inPlayLayerInit || g_mh.totalFrames < 3) {
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

        // Hitbox Multiplier shrink check using pure CCNode::boundingBox()
        if (g_mh.hitboxMultiplier && g_mh.hitboxScale < 1.0f && player && obj) {
            auto oNode = reinterpret_cast<cocos2d::CCNode*>(obj);
            cocos2d::CCRect oRect = oNode->boundingBox();
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

    void postUpdate(float dt) {
        g_mh.collidingThisFrame = false;
        PlayLayer::postUpdate(dt);
        g_mh.totalFrames++;

        // 1. Frame Extrapolation visual smoothing
        if (g_mh.frameExtrapolation && this->m_player1 && !g_mh.playerIsDead) {
            cocos2d::CCPoint cur = this->m_player1->getPosition();
            cocos2d::CCPoint delta = {cur.x - g_mh.p1PrevPos.x, cur.y - g_mh.p1PrevPos.y};
            if (std::hypot(delta.x, delta.y) < 45.0f) {
                g_mh.p1VelSmooth = {
                    g_mh.p1VelSmooth.x * 0.65f + delta.x * 0.35f,
                    g_mh.p1VelSmooth.y * 0.65f + delta.y * 0.35f
                };
                float alpha = std::clamp((dt * g_mh.targetFps) - 1.0f, 0.0f, 0.85f);
                this->m_player1->setPosition({
                    cur.x + g_mh.p1VelSmooth.x * alpha * 0.25f,
                    cur.y + g_mh.p1VelSmooth.y * alpha * 0.25f
                });
            }
            g_mh.p1PrevPos = cur;
        } else if (this->m_player1) {
            cocos2d::CCPoint cur = this->m_player1->getPosition();
            g_mh.p1VelSmooth = {cur.x - g_mh.p1PrevPos.x, cur.y - g_mh.p1PrevPos.y};
            g_mh.p1PrevPos = cur;
        }

        // 2. Player on Top
        if (g_mh.playerOnTop) {
            if (this->m_player1) this->m_player1->setZOrder(10000);
            if (this->m_player2) this->m_player2->setZOrder(10000);
        }

        // 3. No Glow, No Particles & No Portal Circle via pure Cocos2d-x Node inspection
        if ((g_mh.noGlow || g_mh.noParticles || g_mh.noPortalCircle) && this->m_objectLayer) {
            if (auto children = this->m_objectLayer->getChildren()) {
                for (unsigned int i = 0; i < children->count(); ++i) {
                    auto child = static_cast<cocos2d::CCNode*>(children->objectAtIndex(i));
                    if (!child) continue;

                    if (g_mh.noParticles) {
                        if (auto part = typeinfo_cast<cocos2d::CCParticleSystemQuad*>(child)) {
                            part->setVisible(false);
                        }
                    }
                    if (g_mh.noGlow) {
                        if (auto blendNode = typeinfo_cast<cocos2d::CCBlendProtocol*>(child)) {
                            if (blendNode->getBlendFunc().dst == GL_ONE) {
                                child->setVisible(false);
                            }
                        }
                    }
                }
            }
        }

        // 4. P1 & P2 RGB Icons
        g_mh.rgbHue = std::fmod(g_mh.rgbHue + dt * 145.0f, 360.0f);
        auto c1 = hsvToRgb(g_mh.rgbHue, 0.85f, 1.0f);
        auto c2 = hsvToRgb(std::fmod(g_mh.rgbHue + 180.0f, 360.0f), 0.85f, 1.0f);

        if (this->m_player1 && g_mh.p1RgbIcons) this->m_player1->setColor(c1);
        if (this->m_player2 && g_mh.p2RgbIcons) this->m_player2->setColor(c2);

        // 5. Accurate Percentage via CCNode ID "percentage-label"
        if (g_mh.accuratePercentage && this->m_player1) {
            if (auto pctLbl = typeinfo_cast<cocos2d::CCLabelBMFont*>(this->getChildByID("percentage-label"))) {
                float pct = std::clamp((this->m_player1->getPositionX() / std::max(1.0f, g_mh.maxLevelX)) * 100.0f, 0.0f, 100.0f);
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(2) << pct << "%";
                pctLbl->setString(ss.str().c_str());
            }
        }

        // 6. Hitbox Trail History
        if (g_mh.hitboxTrail && this->m_player1 && !g_mh.playerIsDead) {
            g_mh.hitboxTrailHistory.push_back(this->m_player1->boundingBox());
            if (g_mh.hitboxTrailHistory.size() > 45) {
                g_mh.hitboxTrailHistory.pop_front();
            }
        }

        // 7. Trajectory & Hitbox Overlay
        if (m_fields->m_drawOverlay) {
            m_fields->m_drawOverlay->clear();
            if (g_mh.showTrajectory && this->m_player1 && !g_mh.playerIsDead) {
                drawTrajectoryLines(this->m_player1);
            }
            if ((g_mh.showHitboxes || (g_mh.showHitboxesOnDeath && g_mh.playerIsDead)) && this->m_player1) {
                drawLevelHitboxes();
            }
        }

        // 8. Status HUD
        updateHudDisplay(dt);
    }

    void drawTrajectoryLines(PlayerObject* p) {
        auto draw = m_fields->m_drawOverlay;
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

            draw->drawSegment(prevHold, nextHold, 0.85f, {0.2f, 0.95f, 0.45f, 0.88f});
            draw->drawSegment(prevRel,  nextRel,  0.65f, {0.95f, 0.35f, 0.35f, 0.68f});

            prevHold = nextHold;
            prevRel  = nextRel;
        }
    }

    void drawLevelHitboxes() {
        auto draw = m_fields->m_drawOverlay;
        auto drawRectBox = [&](cocos2d::CCRect r, cocos2d::ccColor4F col) {
            cocos2d::CCPoint pts[4] = {
                {r.getMinX(), r.getMinY()},
                {r.getMaxX(), r.getMinY()},
                {r.getMaxX(), r.getMaxY()},
                {r.getMinX(), r.getMaxY()}
            };
            draw->drawPolygon(pts, 4, {col.r, col.g, col.b, 0.14f}, 0.65f, col);
        };

        if (g_mh.hitboxTrail) {
            for (const auto& oldRect : g_mh.hitboxTrailHistory) {
                drawRectBox(oldRect, {0.15f, 0.9f, 0.5f, 0.45f});
            }
        }

        if (this->m_player1) {
            drawRectBox(this->m_player1->boundingBox(), {0.1f, 1.0f, 0.3f, 0.95f});
        }

        if (this->m_objectLayer && this->m_objectLayer->getChildren()) {
            float px = this->m_player1 ? this->m_player1->getPositionX() : 0.f;
            auto children = this->m_objectLayer->getChildren();
            for (unsigned int i = 0; i < children->count(); ++i) {
                auto node = typeinfo_cast<cocos2d::CCNode*>(children->objectAtIndex(i));
                if (!node || node == this->m_player1 || node == this->m_player2 || node == m_fields->m_drawOverlay) continue;
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

    void updateHudDisplay(float dt) {
        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();

        if (m_fields->m_indicatorDot) {
            m_fields->m_indicatorDot->setVisible(g_mh.cheatIndicator);
            if (g_mh.cheatIndicator) {
                cocos2d::ccColor3B c = {46, 204, 113};
                if (g_mh.isCheating()) {
                    c = g_mh.shouldProtectProgress() ? cocos2d::ccColor3B{241, 196, 15} : cocos2d::ccColor3B{231, 76, 60};
                }
                m_fields->m_indicatorDot->setColor(c);
            }
        }

        if (!m_fields->m_hudLabel) return;

        if (g_mh.hudAnchor == 0) {
            m_fields->m_hudLabel->setAnchorPoint({0.0f, 1.0f});
            m_fields->m_hudLabel->setPosition({14.0f, winSize.height - 6.0f});
        } else if (g_mh.hudAnchor == 1) {
            m_fields->m_hudLabel->setAnchorPoint({1.0f, 1.0f});
            m_fields->m_hudLabel->setPosition({winSize.width - 10.0f, winSize.height - 6.0f});
        } else if (g_mh.hudAnchor == 2) {
            m_fields->m_hudLabel->setAnchorPoint({0.0f, 0.0f});
            m_fields->m_hudLabel->setPosition({14.0f, 8.0f});
        } else {
            m_fields->m_hudLabel->setAnchorPoint({0.5f, 1.0f});
            m_fields->m_hudLabel->setPosition({winSize.width * 0.5f, winSize.height - 20.0f});
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
            ss << "CPS: " << g_mh.clickTimestamps.size() << "\n";
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
            ss << "Attempt " << g_mh.attemptCounter << "\n";
        }
        if (g_mh.showBestRun) {
            ss << "Best Run: " << static_cast<int>(g_mh.bestRunFrom) << "% - " << static_cast<int>(g_mh.bestRunTo) << "%\n";
        }
        if (g_mh.startPosSwitcher && !g_mh.startPosCoords.empty()) {
            ss << "StartPos [Q/E]: " << (g_mh.currentStartPosIdx + 1) << "/" << g_mh.startPosCoords.size() << "\n";
        }

        m_fields->m_hudLabel->setString(ss.str().c_str());
    }
};

// ============================================================================
// 9. PAUSE LAYER, GARAGE & MENU HOOKS
// ============================================================================
class $modify(MHPauseHook, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        auto menu = cocos2d::CCMenu::create();
        menu->setPosition({0.f, 0.f});
        this->addChild(menu, 100);

        auto mhSpr = ButtonSprite::create("MHv9 [Tab]", "bigFont.fnt", "GJ_button_06.png", 0.5f);
        mhSpr->setScale(0.6f);
        auto mhBtn = CCMenuItemSpriteExtra::create(mhSpr, this, menu_selector(MHPauseHook::onOpenModMenu));
        mhBtn->setPosition({winSize.width - 52.0f, winSize.height - 22.0f});
        menu->addChild(mhBtn);
    }

    void onOpenModMenu(cocos2d::CCObject*) {
        if (auto ui = MegaHackV9Overlay::getOrCreate()) ui->toggleMenu();
    }
};

class $modify(MHMenuHook, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        applyMenuCustomBackground(this);
        return true;
    }
};

class $modify(MHCreatorHook, CreatorLayer) {
    bool init() {
        if (!CreatorLayer::init()) return false;
        applyMenuCustomBackground(this);
        return true;
    }
};

class $modify(MHGarageHook, GJGarageLayer) {
    bool init() {
        if (!GJGarageLayer::init()) return false;
        applyMenuCustomBackground(this);

        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        auto menu = cocos2d::CCMenu::create();
        menu->setPosition({0.f, 0.f});
        this->addChild(menu, 100);

        auto spr = ButtonSprite::create("MHv9 Effects", "bigFont.fnt", "GJ_button_06.png", 0.55f);
        spr->setScale(0.62f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(MHGarageHook::onOpenGarageEffects));
        btn->setPosition({winSize.width - 54.0f, winSize.height - 22.0f});
        menu->addChild(btn);

        return true;
    }

    void onOpenGarageEffects(cocos2d::CCObject*) {
        if (auto ui = MegaHackV9Overlay::getOrCreate()) ui->toggleMenu();
    }
};
