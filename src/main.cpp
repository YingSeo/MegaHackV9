#include <Geode/Geode.hpp>
#include <Geode/Bindings.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/GameStatsManager.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
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
// 1. ENGINE STATE & DETERMINISTIC MACRO DATA (ALL OFF BY DEFAULT)
// ============================================================================
struct MacroFrameAction {
    int tick;
    bool down;
    int button;
    bool isP1;
};

struct MegaHackEngine {
    bool menuOpen = false;
    int animStyle = 0;   // 0: Smooth Expo+Back, 1: Slide Down, 2: Elastic Scale
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
    bool noWavePulse = false;
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

    // Column 3: Universal & UI
    bool customBackground = false;
    int bgThemeIdx = 0;
    bool fastChests = false;

    // Column 4: Display, Speed & AutoClicker
    bool speedhackEnabled = false;
    float speedhackValue = 1.00f;
    bool frameExtrapolation = false;
    float targetFps = 480.0f;
    bool autoClicker = false;
    int autoClickCps = 15;
    float autoClickTimer = 0.0f;
    bool autoClickState = false;

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
    int macroMode = 0; // 0: Off, 1: Record, 2: Playback
    std::vector<MacroFrameAction> macroBuffer;
    size_t macroCursor = 0;
    int physicsTick = 0;

    // Runtime Level State
    std::vector<GameObject*> startPosObjects;
    int currentStartPosIdx = -1;
    int totalFrames = 0;
    int noclipHitFrames = 0;
    int noclipDeaths = 0;
    bool collidingThisFrame = false;
    bool playerIsDead = false;
    float runStartPct = 0.0f;
    float bestRunFrom = 0.0f;
    float bestRunTo = 0.0f;
    std::vector<float> clickTimestamps;
    bool holdingJump = false;
    float rgbHue = 0.0f;
    std::deque<cocos2d::CCRect> hitboxTrailBuffer;

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

    void saveToGeodeConfig() {
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
        mod->setSavedValue("copyHack", copyHack);
        mod->setSavedValue("verifyHack", verifyHack);
        mod->setSavedValue("levelEditBypass", levelEditBypass);
        mod->setSavedValue("cheatIndicator", cheatIndicator);
        mod->setSavedValue("showFps", showFps);
        mod->setSavedValue("showCps", showCps);
        mod->setSavedValue("showNoclipAccuracy", showNoclipAccuracy);
        mod->setSavedValue("showNoclipDeaths", showNoclipDeaths);
        mod->setSavedValue("showBestRun", showBestRun);
    }
};

static MegaHackEngine g_mh;

// ============================================================================
// 2. ENGINE UTILITIES (RGB HSV, MENU BACKGROUND, LAYOUT & GLOW)
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

static void syncLevelObjectsCosmetics(PlayLayer* pl) {
    if (!pl || !pl->m_objects) return;
    auto objs = pl->m_objects;
    for (unsigned int i = 0; i < objs->count(); ++i) {
        auto obj = typeinfo_cast<GameObject*>(objs->objectAtIndex(i));
        if (!obj) continue;

        // No Glow direct engine property
        obj->m_hasNoGlow = g_mh.noGlow;

        // Layout Mode (Object Type 7 = Decoration)
        int objType = static_cast<int>(obj->m_objectType);
        bool isPortalOrSpeed = (obj->m_objectID >= 10 && obj->m_objectID <= 111);
        if (g_mh.layoutMode && objType == 7 && !isPortalOrSpeed) {
            obj->setVisible(false);
        } else if (!g_mh.layoutMode && objType == 7) {
            obj->setVisible(true);
        }
    }
}

static void cycleStartPos(int stepDir) {
    auto pl = PlayLayer::get();
    if (!pl || g_mh.startPosObjects.empty()) return;

    int count = static_cast<int>(g_mh.startPosObjects.size());
    g_mh.currentStartPosIdx = (g_mh.currentStartPosIdx + stepDir + count) % count;
    auto targetObj = g_mh.startPosObjects[g_mh.currentStartPosIdx];
    if (targetObj && pl->m_player1) {
        cocos2d::CCPoint pos = targetObj->getPosition();
        pl->m_player1->setPosition(pos);
        if (pl->m_player2) {
            pl->m_player2->setPosition(pos);
        }
    }
}

// ============================================================================
// 3. MEGA HACK V9 PRO OVERLAY UI (6 COLUMNS + STAGGERED EASING ANIMATIONS)
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

    // ------------------------------------------------------------------------
    // Column Builders
    // ------------------------------------------------------------------------
    void buildCosmeticColumn(cocos2d::CCPoint pos, float w) {
        float h = 242.0f;
        auto col = createWindowNode("Cosmetic", pos, w, h);
        float y = h - 21.0f;

        addToggleItem(col, y, "Accurate %", &g_mh.accuratePercentage);
        addToggleItem(col, y, "Ball Rotation Bug", &g_mh.ballRotationBug);
        addToggleItem(col, y, "No Glow", &g_mh.noGlow, []() {
            if (auto pl = PlayLayer::get()) syncLevelObjectsCosmetics(pl);
        });
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
        addToggleItem(col, y, "No Camera Shake", &g_mh.noCameraShake, []() {
            GameManager::sharedState()->setGameVariable("0172", g_mh.noCameraShake);
        });
        addToggleItem(col, y, "Layout Mode", &g_mh.layoutMode, []() {
            if (auto pl = PlayLayer::get()) syncLevelObjectsCosmetics(pl);
        });
        addToggleItem(col, y, "Player on Top", &g_mh.playerOnTop);
        addToggleItem(col, y, "No Wave Trail", &g_mh.noWaveTrail);
        addToggleItem(col, y, "No Wave Pulse", &g_mh.noWavePulse);
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
            g_mh.hitboxTrailBuffer.clear();
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
        addToggleItem(col, y, "Practice Music", &g_mh.practiceMusicSync, []() {
            GameManager::sharedState()->setGameVariable("0125", g_mh.practiceMusicSync);
        });
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
        addToggleItem(col, y, "Fast Chests", &g_mh.fastChests);
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
            g_mh.saveToGeodeConfig();
            Notification::create("Mega Hack v9 Config Saved", NotificationIcon::Success)->show();
        });
        addActionButton(col, y, "Reset All OFF", nullptr, []() {
            g_mh = MegaHackEngine();
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
        float h = 205.0f;
        auto col = createWindowNode("Bypass & Bot", pos, w, h);
        float y = h - 21.0f;

        addToggleItem(col, y, "Unlock Icons", &g_mh.iconBypass);
        addToggleItem(col, y, "Copy Hack", &g_mh.copyHack);
        addToggleItem(col, y, "Verify Hack", &g_mh.verifyHack);
        addToggleItem(col, y, "Level Edit", &g_mh.levelEditBypass);

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
// 5. BYPASS & CREATOR HOOKS (ICONS, COLORS, PRACTICE MUSIC, COPY, VERIFY)
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
};

class $modify(MHStatsHook, GameStatsManager) {
    bool isItemEnabled(UnlockType type, int id) {
        if (g_mh.practiceMusicSync && static_cast<int>(type) == 12 && id == 17) {
            return true;
        }
        return GameStatsManager::isItemEnabled(type, id);
    }
};

class $modify(MHLevelInfoHook, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (g_mh.copyHack && level) {
            level->m_isEditable = true;
            level->m_password = 1;
            level->m_originalLevel = 0;
        }
        return LevelInfoLayer::init(level, challenge);
    }
};

class $modify(MHEditLevelHook, EditLevelLayer) {
    bool init(GJGameLevel* level) {
        if (level) {
            if (g_mh.verifyHack) {
                level->m_isVerified = true;
                level->m_isVerifiedRaw = true;
            }
            if (g_mh.copyHack) {
                level->m_originalLevel = 0;
            }
        }
        return EditLevelLayer::init(level);
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
// 7. PLAYER OBJECT HOOKS (SHIPCOPTER, FORCE ICE, BALL BUG, PARTICLES, TRAILS)
// ============================================================================
class $modify(MHPlayerHook, PlayerObject) {
    bool pushButton(PlayerButton p0) {
        if (p0 == PlayerButton::Jump && PlayLayer::get() && !g_mh.holdingJump) {
            g_mh.holdingJump = true;
            float now = cocos2d::CCDirector::sharedDirector()->getTotalFrames() / 60.0f;
            g_mh.clickTimestamps.push_back(now);
        }
        if (g_mh.shipcopter && this->m_isSwing && p0 == PlayerButton::Jump) {
            this->m_isHolding = true;
            return true;
        }
        return PlayerObject::pushButton(p0);
    }

    bool releaseButton(PlayerButton p0) {
        if (p0 == PlayerButton::Jump) {
            g_mh.holdingJump = false;
        }
        if (g_mh.shipcopter && this->m_isSwing && p0 == PlayerButton::Jump) {
            this->m_isHolding = false;
            return true;
        }
        return PlayerObject::releaseButton(p0);
    }

    void update(float dt) {
        if (g_mh.forceIce) {
            this->m_isOnIce = true;
        }
        if (g_mh.shipcopter && this->m_isSwing) {
            float accel = this->m_isHolding ? 0.88f : -0.75f;
            if (this->m_isUpsideDown) accel = -accel;
            this->m_yVelocity = std::clamp(this->m_yVelocity + accel * dt * 2.25f, -14.0, 14.0);
        }

        PlayerObject::update(dt);

        if (g_mh.ballRotationBug && this->m_isBall) {
            this->setRotation(this->getRotation() + (this->m_isUpsideDown ? -19.0f : 19.0f) * dt);
        }

        if (g_mh.noParticles) {
            bool hideGround = g_mh.hideGroundParticles || !g_mh.keepPortalParticles;
            bool hideTrail  = g_mh.hideTrailParticles  || !g_mh.keepPortalParticles;
            bool hideClick  = g_mh.hideClickParticles  || !g_mh.keepPortalParticles;

            if (this->m_playerGroundParticles) this->m_playerGroundParticles->setVisible(!hideGround);
            if (this->m_trailingParticles)     this->m_trailingParticles->setVisible(!hideTrail);
            if (this->m_shipClickParticles)    this->m_shipClickParticles->setVisible(!hideClick);
            if (this->m_ufoClickParticles)     this->m_ufoClickParticles->setVisible(!hideClick);
        }

        if (this->m_waveTrail) {
            if (g_mh.noWaveTrail) {
                this->m_waveTrail->setVisible(false);
            } else {
                this->m_waveTrail->setVisible(true);
                if (g_mh.noWavePulse) {
                    this->m_waveTrail->m_pulseSize = 0.5f;
                }
                if (g_mh.solidWaveTrail) {
                    this->m_waveTrail->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});
                    this->m_waveTrail->setOpacity(255);
                }
            }
        }
    }
};

// ============================================================================
// 8. GJBASEGAMELAYER & PLAYLAYER HOOKS (BOT, STEPPER, AUTOCLICK, HITBOX, HUD)
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

        // Auto Clicker Engine
        if (g_mh.autoClicker && PlayLayer::get() && !g_mh.playerIsDead) {
            g_mh.autoClickTimer += dt;
            float halfPeriod = 0.5f / static_cast<float>(std::max(1, g_mh.autoClickCps));
            if (g_mh.autoClickTimer >= halfPeriod) {
                g_mh.autoClickTimer = 0.0f;
                g_mh.autoClickState = !g_mh.autoClickState;
                GJBaseGameLayer::handleButton(g_mh.autoClickState, 1, true);
            }
        }

        // Deterministic Macro Bot Playback
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
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

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

        // Scan StartPos Objects (Object ID 31)
        g_mh.startPosObjects.clear();
        g_mh.currentStartPosIdx = -1;
        if (this->m_objects) {
            for (unsigned int i = 0; i < this->m_objects->count(); ++i) {
                if (auto obj = typeinfo_cast<GameObject*>(this->m_objects->objectAtIndex(i))) {
                    if (obj->m_objectID == 31) {
                        g_mh.startPosObjects.push_back(obj);
                    }
                }
            }
            std::sort(g_mh.startPosObjects.begin(), g_mh.startPosObjects.end(), [](GameObject* a, GameObject* b) {
                return a->getPositionX() < b->getPositionX();
            });
        }

        syncLevelObjectsCosmetics(this);
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
        g_mh.hitboxTrailBuffer.clear();
        g_mh.runStartPct = this->getCurrentPercent();

        syncLevelObjectsCosmetics(this);

        if (g_mh.instantComplete && !this->m_isPracticeMode) {
            this->playEndAnimationToPos({0.f, 0.f});
        }
    }

    void levelComplete() {
        if (g_mh.shouldProtectProgress() && this->m_level) {
            int savedPct = static_cast<int>(this->m_level->m_normalPercent);
            PlayLayer::levelComplete();
            this->m_level->m_normalPercent = savedPct;
            return;
        }
        PlayLayer::levelComplete();
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        if (obj == this->m_anticheatSpike) {
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

        // Hitbox Multiplier shrink check: allow survival if outside scaled hazard rect
        if (g_mh.hitboxMultiplier && g_mh.hitboxScale < 1.0f && player && obj) {
            int t = static_cast<int>(obj->m_objectType);
            if (t == 2 || t == 36) {
                cocos2d::CCRect oRect = obj->getObjectRect();
                float cx = oRect.getMidX();
                float cy = oRect.getMidY();
                float sw = oRect.size.width * g_mh.hitboxScale;
                float sh = oRect.size.height * g_mh.hitboxScale;
                cocos2d::CCRect scaledRect(cx - sw * 0.5f, cy - sh * 0.5f, sw, sh);
                if (!player->getObjectRect().intersectsRect(scaledRect)) {
                    return;
                }
            }
        }

        g_mh.playerIsDead = true;
        float endPct = this->getCurrentPercent();
        if ((endPct - g_mh.runStartPct) > (g_mh.bestRunTo - g_mh.bestRunFrom)) {
            g_mh.bestRunFrom = g_mh.runStartPct;
            g_mh.bestRunTo = endPct;
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
        }

        // 2. Player on Top
        if (g_mh.playerOnTop) {
            if (this->m_player1) {
                this->m_player1->setZOrder(10000);
                if (this->m_player1->m_waveTrail) this->m_player1->m_waveTrail->setZOrder(9999);
            }
            if (this->m_player2) {
                this->m_player2->setZOrder(10000);
                if (this->m_player2->m_waveTrail) this->m_player2->m_waveTrail->setZOrder(9999);
            }
        }

        // 3. No Portal Circle (Hide CCCircleWave nodes in objectLayer/PlayLayer)
        if (g_mh.noPortalCircle && this->m_objectLayer) {
            if (auto children = this->m_objectLayer->getChildren()) {
                for (unsigned int i = 0; i < children->count(); ++i) {
                    if (auto cw = typeinfo_cast<CCCircleWave*>(children->objectAtIndex(i))) {
                        cw->setVisible(false);
                    }
                }
            }
        }

        // 4. P1 & P2 RGB Icons / Trails
        g_mh.rgbHue = std::fmod(g_mh.rgbHue + dt * 145.0f, 360.0f);
        auto c1 = hsvToRgb(g_mh.rgbHue, 0.85f, 1.0f);
        auto c2 = hsvToRgb(std::fmod(g_mh.rgbHue + 180.0f, 360.0f), 0.85f, 1.0f);

        if (this->m_player1) {
            if (g_mh.p1RgbIcons) this->m_player1->setColor(c1);
            if (g_mh.p1RgbTrail && this->m_player1->m_waveTrail) this->m_player1->m_waveTrail->setColor(c1);
            if (g_mh.p1RgbTrail && this->m_player1->m_regularTrail) this->m_player1->m_regularTrail->setColor(c1);
        }
        if (this->m_player2) {
            if (g_mh.p2RgbIcons) this->m_player2->setColor(c2);
            if (g_mh.p2RgbTrail && this->m_player2->m_waveTrail) this->m_player2->m_waveTrail->setColor(c2);
            if (g_mh.p2RgbTrail && this->m_player2->m_regularTrail) this->m_player2->m_regularTrail->setColor(c2);
        }

        // 5. Accurate Percentage
        if (g_mh.accuratePercentage && this->m_percentageLabel) {
            float pct = std::clamp(this->getCurrentPercent(), 0.0f, 100.0f);
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2) << pct << "%";
            this->m_percentageLabel->setString(ss.str().c_str());
        }

        // 6. Record Hitbox Trail
        if (g_mh.hitboxTrail && this->m_player1 && !g_mh.playerIsDead) {
            g_mh.hitboxTrailBuffer.push_back(this->m_player1->getObjectRect());
            if (g_mh.hitboxTrailBuffer.size() > 45) {
                g_mh.hitboxTrailBuffer.pop_front();
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
        float vx = p->m_playerSpeed * 5.77f;
        float vyHold = static_cast<float>(p->m_yVelocity);
        float vyRel  = static_cast<float>(p->m_yVelocity);
        float grav   = p->m_isUpsideDown ? 0.88f : -0.88f;

        cocos2d::CCPoint prevHold = start;
        cocos2d::CCPoint prevRel  = start;

        for (int i = 1; i <= 48; ++i) {
            if (p->m_isShip || (p->m_isSwing && g_mh.shipcopter)) {
                vyHold = std::clamp(vyHold - grav * 0.45f, -14.f, 14.f);
                vyRel  = std::clamp(vyRel  + grav * 0.45f, -14.f, 14.f);
            } else if (p->m_isDart) {
                float slope = p->m_isMini ? 1.6f : 1.0f;
                vyHold = (p->m_isUpsideDown ? -vx : vx) * slope;
                vyRel  = -vyHold;
            } else {
                vyHold = std::clamp(vyHold + grav * 0.55f, -15.f, 15.f);
                vyRel  = std::clamp(vyRel  + grav * 0.55f, -15.f, 15.f);
            }

            cocos2d::CCPoint nextHold = {prevHold.x + vx * 0.5f, std::max(0.f, prevHold.y + vyHold * 0.65f)};
            cocos2d::CCPoint nextRel  = {prevRel.x  + vx * 0.5f, std::max(0.f, prevRel.y  + vyRel  * 0.65f)};

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
            for (const auto& oldRect : g_mh.hitboxTrailBuffer) {
                drawRectBox(oldRect, {0.15f, 0.9f, 0.5f, 0.45f});
            }
        }

        if (this->m_player1) {
            drawRectBox(this->m_player1->getObjectRect(), {0.1f, 1.0f, 0.3f, 0.95f});
        }

        if (this->m_objects) {
            float px = this->m_player1 ? this->m_player1->getPositionX() : 0.f;
            for (unsigned int i = 0; i < this->m_objects->count(); ++i) {
                auto obj = typeinfo_cast<GameObject*>(this->m_objects->objectAtIndex(i));
                if (!obj) continue;
                if (std::abs(obj->getPositionX() - px) < 275.0f) {
                    int t = static_cast<int>(obj->m_objectType);
                    cocos2d::CCRect r = obj->getObjectRect();
                    if (t == 2 || t == 36) {
                        if (g_mh.hitboxMultiplier) {
                            float cx = r.getMidX();
                            float cy = r.getMidY();
                            float sw = r.size.width * g_mh.hitboxScale;
                            float sh = r.size.height * g_mh.hitboxScale;
                            r = cocos2d::CCRect(cx - sw * 0.5f, cy - sh * 0.5f, sw, sh);
                        }
                        drawRectBox(r, {1.0f, 0.18f, 0.18f, 0.92f});
                    } else if (t == 0 || t == 23) {
                        drawRectBox(r, {0.2f, 0.55f, 1.0f, 0.85f});
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
        if (g_mh.showAttempts && this->m_level) {
            ss << "Attempt " << static_cast<int>(this->m_level->m_attempts) << "\n";
        }
        if (g_mh.showBestRun) {
            ss << "Best Run: " << static_cast<int>(g_mh.bestRunFrom) << "% - " << static_cast<int>(g_mh.bestRunTo) << "%\n";
        }
        if (g_mh.startPosSwitcher && !g_mh.startPosObjects.empty()) {
            ss << "StartPos [Q/E]: " << (g_mh.currentStartPosIdx + 1) << "/" << g_mh.startPosObjects.size() << "\n";
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

        if (g_mh.levelEditBypass) {
            auto editSpr = ButtonSprite::create("Edit Level", "bigFont.fnt", "GJ_button_01.png", 0.5f);
            editSpr->setScale(0.6f);
            auto editBtn = CCMenuItemSpriteExtra::create(editSpr, this, menu_selector(MHPauseHook::onOpenLevelEditor));
            editBtn->setPosition({winSize.width - 52.0f, winSize.height - 46.0f});
            menu->addChild(editBtn);
        }
    }

    void onOpenModMenu(cocos2d::CCObject*) {
        if (auto ui = MegaHackV9Overlay::getOrCreate()) ui->toggleMenu();
    }

    void onOpenLevelEditor(cocos2d::CCObject*) {
        if (auto pl = PlayLayer::get()) {
            if (pl->m_level) {
                auto sc = EditLevelLayer::scene(pl->m_level);
                cocos2d::CCDirector::sharedDirector()->replaceScene(cocos2d::CCTransitionFade::create(0.5f, sc));
            }
        }
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
