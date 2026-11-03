#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/GameStatsManager.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/RewardUnlockLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/CreatorLayer.hpp>
#include <Geode/modify/GJGarageLayer.hpp>
#include <Geode/modify/CCCircleWave.hpp>
#include <cmath>
#include <vector>
#include <deque>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>

using namespace geode::prelude;

struct MacroStep {
    int frame;
    bool down;
    int button;
    bool isPlayer1;
};

struct EngineState {
    bool menuVisible = false;
    int animStyle = 0;
    int hudAnchor = 0;

    bool accuratePercentage = false;
    bool ballRotationBug = false;
    bool noGlow = false;
    bool noParticles = false;
    bool keepPortalParticles = true;
    bool hideGroundParticles = false;
    bool hideTrailParticles = false;
    bool hideClickParticles = false;
    bool noPortalCircle = false;
    bool noMirrorTransition = false;
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

    bool customBackground = false;
    int bgPresetIndex = 0;
    bool fastChests = false;
    bool iconBypass = false;
    bool copyHack = false;
    bool verifyHack = false;
    bool levelEditBypass = false;

    bool speedhackEnabled = false;
    float speedhackValue = 1.00f;
    bool speedhackAudio = false;
    bool frameExtrapolation = false;
    float targetFps = 480.0f;

    bool cheatIndicator = false;
    bool showFps = false;
    bool showCps = false;
    bool showNoclipAccuracy = false;
    bool showNoclipDeaths = false;
    bool showAttempts = false;
    bool showBestRun = false;

    int macroMode = 0;
    std::vector<MacroStep> macroActions;
    size_t macroCursor = 0;
    int physicsTick = 0;

    std::vector<StartPosObject*> startPositions;
    int currentStartPosIdx = -1;

    int totalFrames = 0;
    int noclipHitFrames = 0;
    int noclipDeaths = 0;
    bool collidingThisTick = false;
    bool playerIsDead = false;
    float runStartPct = 0.0f;
    float bestRunStart = 0.0f;
    float bestRunEnd = 0.0f;
    std::vector<float> clickTimes;
    bool holdingJumpBtn = false;
    float rgbHue = 0.0f;
    std::deque<cocos2d::CCRect> playerHitboxHistory;

    cocos2d::CCPoint p1LastPos = {0.f, 0.f};
    cocos2d::CCPoint p1SmoothVel = {0.f, 0.f};

    bool isCheating() const {
        return noclip || noCollision || forceIce || shipcopter ||
               showTrajectory || showHitboxes || hitboxMultiplier ||
               layoutMode || fixedRandomSeed || instantComplete ||
               frameStepper || (macroMode == 2) ||
               (speedhackEnabled && std::abs(speedhackValue - 1.0f) > 0.001f);
    }

    bool isSafeModeActive() const {
        return safeMode || (autoSafeMode && isCheating());
    }
};

static EngineState g_hack;

static cocos2d::ccColor3B hsvToRgb(float h, float s, float v) {
    float c = v * s;
    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r = 0, g = 0, b = 0;
    if (h < 60)       { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else              { r = c; g = 0; b = x; }
    return {
        static_cast<GLubyte>((r + m) * 255),
        static_cast<GLubyte>((g + m) * 255),
        static_cast<GLubyte>((b + m) * 255)
    };
}

static const cocos2d::ccColor3B kMenuThemes[] = {
    {20, 24, 45},
    {46, 16, 36},
    {15, 42, 38},
    {26, 26, 32},
    {55, 34, 16}
};

static void tintLayerBackground(cocos2d::CCNode* layer) {
    if (!layer || !g_hack.customBackground) return;
    auto col = kMenuThemes[g_hack.bgPresetIndex % 5];
    if (auto bg = typeinfo_cast<cocos2d::CCRGBAProtocol*>(layer->getChildByID("background"))) {
        bg->setColor(col);
    }
    CCARRAY_FOREACH_B_TYPE(layer->getChildren(), node, cocos2d::CCSprite) {
        if (node->getZOrder() <= -1) {
            node->setColor(col);
        }
    }
}

static void updateFmodMasterPitch() {
    auto engine = FMODAudioEngine::sharedEngine();
    if (!engine || !engine->m_system) return;
    FMOD::ChannelGroup* master = nullptr;
    if (engine->m_system->getMasterChannelGroup(&master) == FMOD_OK && master) {
        float pitch = (g_hack.speedhackEnabled && g_hack.speedhackAudio) ? g_hack.speedhackValue : 1.0f;
        master->setPitch(std::clamp(pitch, 0.1f, 5.0f));
    }
}

static void applyLayoutModeToPlayLayer(PlayLayer* pl) {
    if (!pl || !pl->m_objects) return;
    CCARRAY_FOREACH_B_TYPE(pl->m_objects, obj, GameObject) {
        if (!obj) continue;
        bool isEssential =
            obj->m_objectType == GameObjectType::Solid ||
            obj->m_objectType == GameObjectType::Hazard ||
            obj->m_objectType == GameObjectType::AnimatedHazard ||
            obj->m_objectType == GameObjectType::Slope ||
            obj->m_objectType == GameObjectType::YellowJumpPad ||
            obj->m_objectType == GameObjectType::PinkJumpPad ||
            obj->m_objectType == GameObjectType::RedJumpPad ||
            obj->m_objectType == GameObjectType::GravityPad ||
            obj->m_objectType == GameObjectType::YellowJumpRing ||
            obj->m_objectType == GameObjectType::PinkJumpRing ||
            obj->m_objectType == GameObjectType::GravityRing ||
            obj->m_objectType == GameObjectType::DropRing ||
            obj->m_objectType == GameObjectType::GreenRing ||
            obj->m_objectType == GameObjectType::DashRing ||
            obj->m_objectType == GameObjectType::GravityDashRing ||
            obj->m_objectType == GameObjectType::SpiderOrb ||
            obj->m_objectType == GameObjectType::SpiderPad ||
            (obj->m_objectID >= 10 && obj->m_objectID <= 111);

        if (g_hack.layoutMode && obj->m_objectType == GameObjectType::Decoration && !isEssential) {
            obj->setVisible(false);
        } else if (!g_hack.layoutMode) {
            obj->setVisible(true);
        }
    }
}

class MegaHackV9Overlay : public cocos2d::CCLayerColor {
public:
    std::vector<cocos2d::CCNode*> m_columns;
    cocos2d::CCNode* m_particleModal = nullptr;
    cocos2d::CCLabelBMFont* m_speedLbl = nullptr;
    cocos2d::CCLabelBMFont* m_hitboxLbl = nullptr;
    cocos2d::CCLabelBMFont* m_fpsLbl = nullptr;
    cocos2d::CCLabelBMFont* m_seedLbl = nullptr;
    cocos2d::CCLabelBMFont* m_botLbl = nullptr;
    cocos2d::CCLabelBMFont* m_anchorLbl = nullptr;
    cocos2d::CCLabelBMFont* m_animLbl = nullptr;

    static MegaHackV9Overlay* getOrCreate() {
        auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
        if (!scene) return nullptr;
        if (auto existing = typeinfo_cast<MegaHackV9Overlay*>(scene->getChildByID("mhv9-pro-overlay"_spr))) {
            return existing;
        }
        auto ret = new MegaHackV9Overlay();
        if (ret && ret->init()) {
            ret->autorelease();
            ret->setID("mhv9-pro-overlay"_spr);
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
        float totalW = 6 * colW + 5 * gap;
        float startX = (winSize.width - totalW) * 0.5f + colW * 0.5f;
        float topY = winSize.height - 14.0f;

        buildCosmeticPanel({startX + 0 * (colW + gap), topY}, colW);
        buildLevelPanel({startX + 1 * (colW + gap), topY}, colW);
        buildUniversalPanel({startX + 2 * (colW + gap), topY}, colW);
        buildDisplaySpeedPanel({startX + 3 * (colW + gap), topY}, colW);
        buildStatusHudPanel({startX + 4 * (colW + gap), topY}, colW);
        buildBypassBotPanel({startX + 5 * (colW + gap), topY}, colW);

        buildParticleModal({winSize.width * 0.5f, winSize.height * 0.5f});
        return true;
    }

    cocos2d::CCNode* createColumnWindow(const char* title, cocos2d::CCPoint topCenter, float width, float height) {
        auto panel = cocos2d::CCNode::create();
        panel->setPosition(topCenter);
        panel->setContentSize({width, height});
        panel->setAnchorPoint({0.5f, 1.0f});
        panel->setCascadeOpacityEnabled(true);

        auto outline = cocos2d::CCLayerColor::create({60, 62, 76, 255}, width + 1.5f, height + 1.5f);
        outline->ignoreAnchorPointForPosition(false);
        outline->setAnchorPoint({0.5f, 1.0f});
        outline->setPosition({width * 0.5f, height + 0.75f});
        panel->addChild(outline, -1);

        auto body = cocos2d::CCLayerColor::create({17, 17, 22, 248}, width, height);
        body->ignoreAnchorPointForPosition(false);
        body->setAnchorPoint({0.5f, 1.0f});
        body->setPosition({width * 0.5f, height});
        panel->addChild(body, 0);

        float headerH = 14.0f;
        auto header = cocos2d::CCLayerColor::create({208, 38, 38, 255}, width, headerH);
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

    struct ToggleBinding : public cocos2d::CCObject {
        bool* ptr;
        cocos2d::CCLayerColor* bg;
        cocos2d::CCLayerColor* bar;
        cocos2d::CCLabelBMFont* lbl;
        std::function<void()> fn;
    };

    struct ActionBinding : public cocos2d::CCObject {
        std::function<void()> fn;
    };

    void addToggle(cocos2d::CCNode* col, float& curY, const char* name, bool* statePtr, std::function<void()> onChange = nullptr, std::function<void()> onSubmenu = nullptr) {
        float w = col->getContentSize().width;
        float rowH = 13.0f;
        auto menu = typeinfo_cast<cocos2d::CCMenu*>(col->getChildByID("column-menu"));
        if (!menu) return;

        auto bg = cocos2d::CCLayerColor::create(
            *statePtr ? cocos2d::ccColor4B{208, 38, 38, 98} : cocos2d::ccColor4B{29, 29, 37, 230},
            w - 5.0f, rowH - 1.3f
        );
        bg->ignoreAnchorPointForPosition(false);
        bg->setAnchorPoint({0.5f, 0.5f});

        auto bar = cocos2d::CCLayerColor::create(
            *statePtr ? cocos2d::ccColor4B{245, 60, 60, 255} : cocos2d::ccColor4B{68, 68, 80, 255},
            2.4f, rowH - 1.3f
        );
        bar->ignoreAnchorPointForPosition(false);
        bar->setAnchorPoint({0.0f, 0.0f});
        bar->setPosition({0.0f, 0.0f});
        bg->addChild(bar);

        auto lbl = cocos2d::CCLabelBMFont::create(name, "chatFont.fnt");
        lbl->setScale(0.35f);
        lbl->setAnchorPoint({0.0f, 0.5f});
        lbl->setPosition({5.2f, (rowH - 1.3f) * 0.5f});
        lbl->setColor(*statePtr ? cocos2d::ccColor3B{255, 255, 255} : cocos2d::ccColor3B{192, 194, 206});
        bg->addChild(lbl);

        auto btn = CCMenuItemSpriteExtra::create(bg, this, menu_selector(MegaHackV9Overlay::onToggleClicked));
        btn->setPosition({w * 0.5f, curY});
        btn->m_scaleMultiplier = 1.02f;

        auto bind = new ToggleBinding();
        bind->autorelease();
        bind->ptr = statePtr;
        bind->bg = bg;
        bind->bar = bar;
        bind->lbl = lbl;
        bind->fn = onChange;
        btn->setUserObject(bind);
        menu->addChild(btn);

        if (onSubmenu) {
            auto subLbl = cocos2d::CCLabelBMFont::create("<", "bigFont.fnt");
            subLbl->setScale(0.20f);
            subLbl->setColor({245, 192, 78});
            auto subBtn = CCMenuItemSpriteExtra::create(subLbl, this, menu_selector(MegaHackV9Overlay::onActionClicked));
            subBtn->setPosition({w - 8.5f, curY});
            auto act = new ActionBinding();
            act->autorelease();
            act->fn = onSubmenu;
            subBtn->setUserObject(act);
            menu->addChild(subBtn, 5);
        }

        curY -= rowH;
    }

    void addStepper(cocos2d::CCNode* col, float& curY, const char* initText, cocos2d::CCLabelBMFont** outLbl, std::function<void(int)> onStep) {
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

        auto makeBtn = [&](const char* txt, float xPos, int dir) {
            auto bLbl = cocos2d::CCLabelBMFont::create(txt, "bigFont.fnt");
            bLbl->setScale(0.23f);
            auto btn = CCMenuItemSpriteExtra::create(bLbl, this, menu_selector(MegaHackV9Overlay::onActionClicked));
            btn->setPosition({xPos, curY});
            auto act = new ActionBinding();
            act->autorelease();
            act->fn = [onStep, dir]() { onStep(dir); };
            btn->setUserObject(act);
            menu->addChild(btn);
        };

        makeBtn("-", 9.0f, -1);
        makeBtn("+", w - 9.0f, +1);
        curY -= rowH;
    }

    void addButton(cocos2d::CCNode* col, float& curY, const char* text, cocos2d::CCLabelBMFont** outLbl, std::function<void()> onClick) {
        float w = col->getContentSize().width;
        float rowH = 13.5f;
        auto menu = typeinfo_cast<cocos2d::CCMenu*>(col->getChildByID("column-menu"));
        if (!menu) return;

        auto bg = cocos2d::CCLayerColor::create({40, 44, 60, 240}, w - 5.5f, rowH - 1.5f);
        bg->ignoreAnchorPointForPosition(false);
        bg->setAnchorPoint({0.5f, 0.5f});

        auto lbl = cocos2d::CCLabelBMFont::create(text, "chatFont.fnt");
        lbl->setScale(0.35f);
        lbl->setPosition({(w - 5.5f) * 0.5f, (rowH - 1.5f) * 0.5f});
        lbl->setColor({135, 222, 255});
        bg->addChild(lbl);
        if (outLbl) *outLbl = lbl;

        auto btn = CCMenuItemSpriteExtra::create(bg, this, menu_selector(MegaHackV9Overlay::onActionClicked));
        btn->setPosition({w * 0.5f, curY});
        auto act = new ActionBinding();
        act->autorelease();
        act->fn = onClick;
        btn->setUserObject(act);
        menu->addChild(btn);

        curY -= rowH;
    }

    void onToggleClicked(cocos2d::CCObject* sender) {
        auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);
        auto bind = static_cast<ToggleBinding*>(btn->getUserObject());
        if (!bind || !bind->ptr) return;

        *(bind->ptr) = !(*(bind->ptr));
        bool on = *(bind->ptr);

        bind->bg->stopAllActions();
        bind->bg->runAction(cocos2d::CCTintTo::create(0.12f, on ? 208 : 29, on ? 38 : 29, on ? 38 : 37));
        bind->bg->setOpacity(on ? 98 : 230);
        bind->bar->setColor(on ? cocos2d::ccColor3B{245, 60, 60} : cocos2d::ccColor3B{68, 68, 80});
        bind->lbl->setColor(on ? cocos2d::ccColor3B{255, 255, 255} : cocos2d::ccColor3B{192, 194, 206});

        if (bind->fn) bind->fn();
    }

    void onActionClicked(cocos2d::CCObject* sender) {
        auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);
        auto act = static_cast<ActionBinding*>(btn->getUserObject());
        if (act && act->fn) act->fn();
    }

    void buildCosmeticPanel(cocos2d::CCPoint pos, float w) {
        float h = 242.0f;
        auto col = createColumnWindow("Cosmetic", pos, w, h);
        float y = h - 21.0f;

        addToggle(col, y, "Accurate %", &g_hack.accuratePercentage);
        addToggle(col, y, "Ball Rotation Bug", &g_hack.ballRotationBug);
        addToggle(col, y, "No Glow", &g_hack.noGlow, []() {
            if (auto pl = PlayLayer::get()) {
                if (pl->m_glowBatchNode) pl->m_glowBatchNode->setVisible(!g_hack.noGlow);
            }
        });
        addToggle(col, y, "No Particles", &g_hack.noParticles, nullptr, [this]() {
            if (m_particleModal) {
                bool show = !m_particleModal->isVisible();
                m_particleModal->setVisible(show);
                if (show) {
                    m_particleModal->setScale(0.82f);
                    m_particleModal->runAction(cocos2d::CCEaseBackOut::create(cocos2d::CCScaleTo::create(0.18f, 1.0f)));
                }
            }
        });
        addToggle(col, y, "No Portal Circle", &g_hack.noPortalCircle);
        addToggle(col, y, "No Mirror Portal", &g_hack.noMirrorTransition);
        addToggle(col, y, "No Camera Shake", &g_hack.noCameraShake);
        addToggle(col, y, "Layout Mode", &g_hack.layoutMode, []() {
            if (auto pl = PlayLayer::get()) applyLayoutModeToPlayLayer(pl);
        });
        addToggle(col, y, "Player on Top", &g_hack.playerOnTop);
        addToggle(col, y, "No Wave Trail", &g_hack.noWaveTrail);
        addToggle(col, y, "No Wave Pulse", &g_hack.noWavePulse);
        addToggle(col, y, "Solid Wave Trail", &g_hack.solidWaveTrail);
        addToggle(col, y, "P1 RGB Icons", &g_hack.p1RgbIcons);
        addToggle(col, y, "P2 RGB Icons", &g_hack.p2RgbIcons);
        addToggle(col, y, "P1 RGB Trail", &g_hack.p1RgbTrail);
        addToggle(col, y, "P2 RGB Trail", &g_hack.p2RgbTrail);
    }

    void buildLevelPanel(cocos2d::CCPoint pos, float w) {
        float h = 268.0f;
        auto col = createColumnWindow("Level", pos, w, h);
        float y = h - 21.0f;

        addToggle(col, y, "Noclip", &g_hack.noclip);
        addToggle(col, y, "No Collision", &g_hack.noCollision);
        addToggle(col, y, "Safe Mode", &g_hack.safeMode);
        addToggle(col, y, "Auto Safe Mode", &g_hack.autoSafeMode);
        addToggle(col, y, "Force Ice", &g_hack.forceIce);
        addToggle(col, y, "Shipcopter", &g_hack.shipcopter);
        addToggle(col, y, "Show Trajectory", &g_hack.showTrajectory);
        addToggle(col, y, "Show Hitboxes", &g_hack.showHitboxes);
        addToggle(col, y, "Hitboxes on Death", &g_hack.showHitboxesOnDeath);
        addToggle(col, y, "Hitbox Trail", &g_hack.hitboxTrail, []() {
            g_hack.playerHitboxHistory.clear();
        });
        addToggle(col, y, "Hitbox Multiplier", &g_hack.hitboxMultiplier);
        addStepper(col, y, "Hitbox: 0.50x", &m_hitboxLbl, [this](int dir) {
            g_hack.hitboxScale = std::clamp(g_hack.hitboxScale + dir * 0.1f, 0.1f, 3.0f);
            if (m_hitboxLbl) {
                std::ostringstream ss;
                ss << "Hitbox: " << std::fixed << std::setprecision(2) << g_hack.hitboxScale << "x";
                m_hitboxLbl->setString(ss.str().c_str());
            }
        });
        addToggle(col, y, "StartPos Switch [Q/E]", &g_hack.startPosSwitcher);
        addToggle(col, y, "Random Seed", &g_hack.fixedRandomSeed);
        addStepper(col, y, "Seed: 1337", &m_seedLbl, [this](int dir) {
            g_hack.randomSeed = static_cast<unsigned int>(std::max(1, static_cast<int>(g_hack.randomSeed) + dir * 111));
            if (m_seedLbl) m_seedLbl->setString(fmt::format("Seed: {}", g_hack.randomSeed).c_str());
        });
        addToggle(col, y, "Instant Complete", &g_hack.instantComplete, []() {
            if (g_hack.instantComplete) {
                if (auto pl = PlayLayer::get()) pl->playEndAnimationToPos({0.f, 0.f});
            }
        });
        addToggle(col, y, "Practice Music", &g_hack.practiceMusicSync);
        addToggle(col, y, "Frame Step [C]", &g_hack.frameStepper);
    }

    void buildUniversalPanel(cocos2d::CCPoint pos, float w) {
        float h = 180.0f;
        auto col = createColumnWindow("Universal", pos, w, h);
        float y = h - 21.0f;

        addToggle(col, y, "Custom BG", &g_hack.customBackground, []() {
            auto sc = cocos2d::CCDirector::sharedDirector()->getRunningScene();
            if (sc && sc->getChildrenCount() > 0) {
                tintLayerBackground(static_cast<cocos2d::CCNode*>(sc->getChildren()->objectAtIndex(0)));
            }
        }, []() {
            g_hack.bgPresetIndex = (g_hack.bgPresetIndex + 1) % 5;
            auto sc = cocos2d::CCDirector::sharedDirector()->getRunningScene();
            if (sc && sc->getChildrenCount() > 0) {
                tintLayerBackground(static_cast<cocos2d::CCNode*>(sc->getChildren()->objectAtIndex(0)));
            }
        });
        addToggle(col, y, "Fast Chests", &g_hack.fastChests);
        addButton(col, y, "HUD: Top-Left", &m_anchorLbl, [this]() {
            g_hack.hudAnchor = (g_hack.hudAnchor + 1) % 4;
            const char* anchors[] = {"HUD: Top-Left", "HUD: Top-Right", "HUD: Bottom-Left", "HUD: Top-Center"};
            if (m_anchorLbl) m_anchorLbl->setString(anchors[g_hack.hudAnchor]);
        });
        addButton(col, y, "Anim: Smooth Expo", &m_animLbl, [this]() {
            g_hack.animStyle = (g_hack.animStyle + 1) % 3;
            const char* styles[] = {"Anim: Smooth Expo", "Anim: Slide Down", "Anim: Elastic Pop"};
            if (m_animLbl) m_animLbl->setString(styles[g_hack.animStyle]);
        });
        addButton(col, y, "Disable All Hacks", nullptr, []() {
            g_hack = EngineState();
            g_hack.menuVisible = true;
            updateFmodMasterPitch();
            cocos2d::CCApplication::sharedApplication()->setAnimationInterval(1.0 / 240.0);
            Notification::create("All Hacks Reset to OFF", NotificationIcon::Success)->show();
        });
    }

    void buildDisplaySpeedPanel(cocos2d::CCPoint pos, float w) {
        float h = 180.0f;
        auto col = createColumnWindow("Display / Speed", pos, w, h);
        float y = h - 21.0f;

        addToggle(col, y, "Speedhack", &g_hack.speedhackEnabled, []() {
            updateFmodMasterPitch();
        });
        addStepper(col, y, "Speed: 1.00x", &m_speedLbl, [this](int dir) {
            g_hack.speedhackValue = std::clamp(g_hack.speedhackValue + dir * 0.1f, 0.1f, 5.0f);
            if (m_speedLbl) {
                std::ostringstream ss;
                ss << "Speed: " << std::fixed << std::setprecision(2) << g_hack.speedhackValue << "x";
                m_speedLbl->setString(ss.str().c_str());
            }
            updateFmodMasterPitch();
        });
        addToggle(col, y, "Speedhack Audio", &g_hack.speedhackAudio, []() {
            updateFmodMasterPitch();
        });
        addToggle(col, y, "Frame Extrap.", &g_hack.frameExtrapolation, []() {
            float fps = g_hack.frameExtrapolation ? g_hack.targetFps : 240.0f;
            cocos2d::CCApplication::sharedApplication()->setAnimationInterval(1.0 / fps);
        });
        addStepper(col, y, "FPS: 480", &m_fpsLbl, [this](int dir) {
            g_hack.targetFps = std::clamp(g_hack.targetFps + dir * 120.0f, 60.0f, 1440.0f);
            if (m_fpsLbl) m_fpsLbl->setString(fmt::format("FPS: {}", static_cast<int>(g_hack.targetFps)).c_str());
            if (g_hack.frameExtrapolation) {
                cocos2d::CCApplication::sharedApplication()->setAnimationInterval(1.0 / g_hack.targetFps);
            }
        });
    }

    void buildStatusHudPanel(cocos2d::CCPoint pos, float w) {
        float h = 180.0f;
        auto col = createColumnWindow("Status HUD", pos, w, h);
        float y = h - 21.0f;

        addToggle(col, y, "Cheat Indicator", &g_hack.cheatIndicator);
        addToggle(col, y, "FPS Counter", &g_hack.showFps);
        addToggle(col, y, "CPS Counter", &g_hack.showCps);
        addToggle(col, y, "Noclip Accuracy", &g_hack.showNoclipAccuracy);
        addToggle(col, y, "Noclip Deaths", &g_hack.showNoclipDeaths);
        addToggle(col, y, "Attempts", &g_hack.showAttempts);
        addToggle(col, y, "Best Run", &g_hack.showBestRun);
    }

    void buildBypassBotPanel(cocos2d::CCPoint pos, float w) {
        float h = 205.0f;
        auto col = createColumnWindow("Bypass & Bot", pos, w, h);
        float y = h - 21.0f;

        addToggle(col, y, "Unlock Icons", &g_hack.iconBypass);
        addToggle(col, y, "Copy Hack", &g_hack.copyHack);
        addToggle(col, y, "Verify Hack", &g_hack.verifyHack);
        addToggle(col, y, "Level Edit", &g_hack.levelEditBypass);

        addButton(col, y, "Bot: Disabled", &m_botLbl, [this]() {
            g_hack.macroMode = (g_hack.macroMode + 1) % 3;
            if (g_hack.macroMode == 1) {
                g_hack.macroActions.clear();
                if (m_botLbl) m_botLbl->setString("Bot: Recording");
            } else if (g_hack.macroMode == 2) {
                g_hack.macroCursor = 0;
                if (m_botLbl) m_botLbl->setString("Bot: Playback");
            } else {
                if (m_botLbl) m_botLbl->setString("Bot: Disabled");
            }
        });

        addButton(col, y, "Clear Macro", nullptr, [this]() {
            g_hack.macroActions.clear();
            g_hack.macroCursor = 0;
            g_hack.macroMode = 0;
            if (m_botLbl) m_botLbl->setString("Bot: Disabled");
            Notification::create("Macro Buffer Cleared", NotificationIcon::Info)->show();
        });
    }

    void buildParticleModal(cocos2d::CCPoint centerPos) {
        float w = 140.0f;
        float h = 110.0f;
        m_particleModal = cocos2d::CCNode::create();
        m_particleModal->setPosition(centerPos);
        m_particleModal->setContentSize({w, h});
        m_particleModal->setAnchorPoint({0.5f, 0.5f});
        m_particleModal->setVisible(false);

        auto bg = cocos2d::CCLayerColor::create({14, 14, 18, 252}, w, h);
        bg->ignoreAnchorPointForPosition(false);
        bg->setAnchorPoint({0.f, 0.f});
        m_particleModal->addChild(bg, 0);

        auto header = cocos2d::CCLayerColor::create({208, 38, 38, 255}, w, 14.0f);
        header->ignoreAnchorPointForPosition(false);
        header->setAnchorPoint({0.f, 1.f});
        header->setPosition({0.f, h});
        m_particleModal->addChild(header, 1);

        auto title = cocos2d::CCLabelBMFont::create("Particle Filter", "bigFont.fnt");
        title->setScale(0.22f);
        title->setPosition({w * 0.5f, h - 7.0f});
        m_particleModal->addChild(title, 2);

        auto menu = cocos2d::CCMenu::create();
        menu->setPosition({0.f, 0.f});
        menu->setID("column-menu");
        m_particleModal->addChild(menu, 3);

        float y = h - 22.0f;
        addToggle(m_particleModal, y, "Portal Effect (Keep)", &g_hack.keepPortalParticles);
        addToggle(m_particleModal, y, "Hide Ground Part.", &g_hack.hideGroundParticles);
        addToggle(m_particleModal, y, "Hide Trail Part.", &g_hack.hideTrailParticles);
        addToggle(m_particleModal, y, "Hide Click Part.", &g_hack.hideClickParticles);

        this->addChild(m_particleModal, 100);
    }

    void toggleMenu() {
        g_hack.menuVisible = !g_hack.menuVisible;
        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        float topY = winSize.height - 14.0f;

        this->stopAllActions();

        if (g_hack.menuVisible) {
            this->setVisible(true);
            this->setTouchEnabled(true);
            this->setOpacity(0);
            this->runAction(cocos2d::CCEaseSineOut::create(cocos2d::CCFadeTo::create(0.18f, 165)));

            for (size_t i = 0; i < m_columns.size(); ++i) {
                auto col = m_columns[i];
                col->stopAllActions();
                cocos2d::CCPoint dest = {col->getPositionX(), topY};
                float delay = static_cast<float>(i) * 0.018f;

                if (g_hack.animStyle == 0) {
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
                } else if (g_hack.animStyle == 1) {
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
            if (m_particleModal) m_particleModal->setVisible(false);

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

class $modify(MHProKeyboardHook, cocos2d::CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(cocos2d::enumKeyCodes key, bool isKeyDown, bool isKeyRepeat) {
        if (key == cocos2d::enumKeyCodes::KEY_Tab && isKeyDown && !isKeyRepeat) {
            if (auto overlay = MegaHackV9Overlay::getOrCreate()) {
                overlay->toggleMenu();
                return true;
            }
        }
        if (g_hack.frameStepper && key == cocos2d::enumKeyCodes::KEY_C && isKeyDown) {
            g_hack.stepAdvanceOnce = true;
            return true;
        }
        if (g_hack.startPosSwitcher && isKeyDown && !isKeyRepeat && PlayLayer::get()) {
            if (key == cocos2d::enumKeyCodes::KEY_Q) {
                switchStartPosByDirection(-1);
                return true;
            }
            if (key == cocos2d::enumKeyCodes::KEY_E) {
                switchStartPosByDirection(+1);
                return true;
            }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat);
    }
};

class $modify(MHProGameManagerHook, GameManager) {
    bool isIconUnlocked(int id, IconType type) {
        if (g_hack.iconBypass) return true;
        return GameManager::isIconUnlocked(id, type);
    }
    bool isColorUnlocked(int id, UnlockType type) {
        if (g_hack.iconBypass) return true;
        return GameManager::isColorUnlocked(id, type);
    }
};

class $modify(MHProStatsManagerHook, GameStatsManager) {
    bool isItemEnabled(UnlockType type, int id) {
        if (g_hack.practiceMusicSync && static_cast<int>(type) == 12 && id == 17) {
            return true;
        }
        return GameStatsManager::isItemEnabled(type, id);
    }
};

class $modify(MHProLevelInfoHook, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (g_hack.copyHack && level) {
            level->m_isEditable = true;
            level->m_password = 1;
            level->m_originalLevel = 0;
        }
        return LevelInfoLayer::init(level, challenge);
    }
};

class $modify(MHProEditLevelHook, EditLevelLayer) {
    bool init(GJGameLevel* level) {
        if (level) {
            if (g_hack.verifyHack) {
                level->m_isVerified = true;
                level->m_isVerifiedRaw = true;
            }
            if (g_hack.copyHack) {
                level->m_originalLevel = 0;
            }
        }
        return EditLevelLayer::init(level);
    }
};

class $modify(MHProRewardUnlockHook, RewardUnlockLayer) {
    void showCollectReward(GJRewardItem* item) {
        RewardUnlockLayer::showCollectReward(item);
        if (g_hack.fastChests) {
            this->stopAllActions();
            if (this->m_rewardbut) {
                this->m_rewardbut->setVisible(true);
                this->m_rewardbut->setEnabled(true);
            }
        }
    }
};

class $modify(MHProSchedulerHook, cocos2d::CCScheduler) {
    void update(float dt) {
        if (g_hack.speedhackEnabled && g_hack.speedhackValue > 0.01f) {
            dt *= g_hack.speedhackValue;
        }
        CCScheduler::update(dt);
    }
};

class $modify(MHProCircleWaveHook, CCCircleWave) {
    void draw() {
        if (g_hack.noPortalCircle && PlayLayer::get()) return;
        CCCircleWave::draw();
    }
};

class $modify(MHProGameObjectHook, GameObject) {
    cocos2d::CCRect getObjectRect(float fl1, float fl2) {
        if (g_hack.hitboxMultiplier && PlayLayer::get()) {
            if (this->m_objectType == GameObjectType::Hazard ||
                this->m_objectType == GameObjectType::AnimatedHazard) {
                return GameObject::getObjectRect(fl1 * g_hack.hitboxScale, fl2 * g_hack.hitboxScale);
            }
        }
        return GameObject::getObjectRect(fl1, fl2);
    }
};

class $modify(MHProPlayerHook, PlayerObject) {
    bool pushButton(PlayerButton p0) {
        if (p0 == PlayerButton::Jump && PlayLayer::get() && !g_hack.holdingJumpBtn) {
            g_hack.holdingJumpBtn = true;
            float now = cocos2d::CCDirector::sharedDirector()->getTotalFrames() / 60.0f;
            g_hack.clickTimes.push_back(now);
        }
        if (g_hack.shipcopter && this->m_isSwing && p0 == PlayerButton::Jump) {
            this->m_isHolding = true;
            return true;
        }
        return PlayerObject::pushButton(p0);
    }

    bool releaseButton(PlayerButton p0) {
        if (p0 == PlayerButton::Jump) {
            g_hack.holdingJumpBtn = false;
        }
        if (g_hack.shipcopter && this->m_isSwing && p0 == PlayerButton::Jump) {
            this->m_isHolding = false;
            return true;
        }
        return PlayerObject::releaseButton(p0);
    }

    void update(float dt) {
        if (g_hack.forceIce) {
            this->m_isOnIce = true;
        }
        if (g_hack.shipcopter && this->m_isSwing) {
            float accel = this->m_isHolding ? 0.88f : -0.75f;
            if (this->m_isUpsideDown) accel = -accel;
            this->m_yVelocity = std::clamp(this->m_yVelocity + accel * dt * 2.25f, -14.0f, 14.0f);
        }

        PlayerObject::update(dt);

        if (g_hack.ballRotationBug && this->m_isBall) {
            this->m_mainSprite->setRotation(this->m_mainSprite->getRotation() + (this->m_isUpsideDown ? -19.0f : 19.0f) * dt);
        }

        if (g_hack.noParticles) {
            bool hideGround = g_hack.hideGroundParticles || !g_hack.keepPortalParticles;
            bool hideTrail  = g_hack.hideTrailParticles  || !g_hack.keepPortalParticles;
            bool hideClick  = g_hack.hideClickParticles  || !g_hack.keepPortalParticles;

            if (this->m_playerGroundParticles) this->m_playerGroundParticles->setVisible(!hideGround);
            if (this->m_trailingParticles)     this->m_trailingParticles->setVisible(!hideTrail);
            if (this->m_shipClickParticles)    this->m_shipClickParticles->setVisible(!hideClick);
            if (this->m_ufoClickParticles)     this->m_ufoClickParticles->setVisible(!hideClick);
        }

        if (this->m_waveTrail) {
            if (g_hack.noWaveTrail) {
                this->m_waveTrail->setVisible(false);
            } else {
                this->m_waveTrail->setVisible(true);
                if (g_hack.noWavePulse) {
                    this->m_waveTrail->m_pulseSize = 0.5f;
                }
                if (g_hack.solidWaveTrail) {
                    this->m_waveTrail->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});
                    this->m_waveTrail->setOpacity(255);
                }
            }
        }
    }
};

class $modify(MHProBaseGameHook, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (g_hack.macroMode == 1 && PlayLayer::get()) {
            g_hack.macroActions.push_back({
                g_hack.physicsTick,
                down,
                button,
                isPlayer1
            });
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void update(float dt) {
        if (g_hack.frameStepper && PlayLayer::get()) {
            if (!g_hack.stepAdvanceOnce) return;
            g_hack.stepAdvanceOnce = false;
            dt = 1.0f / 240.0f;
        }

        g_hack.physicsTick++;

        if (g_hack.macroMode == 2 && PlayLayer::get()) {
            while (g_hack.macroCursor < g_hack.macroActions.size() &&
                   g_hack.macroActions[g_hack.macroCursor].frame <= g_hack.physicsTick) {
                const auto& act = g_hack.macroActions[g_hack.macroCursor];
                GJBaseGameLayer::handleButton(act.down, act.button, act.isPlayer1);
                g_hack.macroCursor++;
            }
        }

        GJBaseGameLayer::update(dt);
    }

    void collisionCheckObjects(PlayerObject* player, gd::vector<GameObject*>* objs, int count, float dt) {
        if (g_hack.noCollision) return;
        GJBaseGameLayer::collisionCheckObjects(player, objs, count, dt);
    }

    void shakeCamera(float duration, float strength, float interval) {
        if (g_hack.noCameraShake) return;
        GJBaseGameLayer::shakeCamera(duration, strength, interval);
    }

    cocos2d::CCParticleSystemQuad* spawnParticle(char const* plist, int zOrder, cocos2d::tCCPositionType posType, cocos2d::CCPoint pos) {
        if (g_hack.noParticles) {
            std::string name = plist ? plist : "";
            bool isPortal = (name.find("portal") != std::string::npos || name.find("Portal") != std::string::npos);
            if (!(isPortal && g_hack.keepPortalParticles)) {
                return nullptr;
            }
        }
        return GJBaseGameLayer::spawnParticle(plist, zOrder, posType, pos);
    }
};

class $modify(MHProPlayLayerHook, PlayLayer) {
    struct Fields {
        cocos2d::CCDrawNode* m_drawNode = nullptr;
        cocos2d::CCLabelBMFont* m_hudText = nullptr;
        cocos2d::CCLayerColor* m_statusDot = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        m_fields->m_drawNode = cocos2d::CCDrawNode::create();
        if (this->m_objectLayer) {
            this->m_objectLayer->addChild(m_fields->m_drawNode, 9999);
        }

        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        m_fields->m_hudText = cocos2d::CCLabelBMFont::create("", "chatFont.fnt");
        m_fields->m_hudText->setScale(0.44f);
        m_fields->m_hudText->setAnchorPoint({0.0f, 1.0f});
        m_fields->m_hudText->setPosition({14.0f, winSize.height - 6.0f});
        m_fields->m_hudText->setOpacity(225);
        this->addChild(m_fields->m_hudText, 9998);

        m_fields->m_statusDot = cocos2d::CCLayerColor::create({46, 204, 113, 255}, 5.0f, 5.0f);
        m_fields->m_statusDot->setPosition({6.0f, winSize.height - 11.0f});
        m_fields->m_statusDot->setVisible(false);
        this->addChild(m_fields->m_statusDot, 9998);

        g_hack.startPositions.clear();
        g_hack.currentStartPosIdx = -1;
        if (this->m_objects) {
            CCARRAY_FOREACH_B_TYPE(this->m_objects, obj, GameObject) {
                if (auto sp = typeinfo_cast<StartPosObject*>(obj)) {
                    g_hack.startPositions.push_back(sp);
                }
            }
            std::sort(g_hack.startPositions.begin(), g_hack.startPositions.end(), [](StartPosObject* a, StartPosObject* b) {
                return a->getPositionX() < b->getPositionX();
            });
        }

        if (g_hack.layoutMode) {
            applyLayoutModeToPlayLayer(this);
        }

        return true;
    }

    void toggleFlipped(bool p0, bool p1) {
        if (g_hack.noMirrorTransition) return;
        PlayLayer::toggleFlipped(p0, p1);
    }

    void resetLevel() {
        if (g_hack.fixedRandomSeed) {
            std::srand(g_hack.randomSeed);
        }
        PlayLayer::resetLevel();

        g_hack.totalFrames = 0;
        g_hack.noclipHitFrames = 0;
        g_hack.noclipDeaths = 0;
        g_hack.collidingThisTick = false;
        g_hack.playerIsDead = false;
        g_hack.physicsTick = 0;
        g_hack.macroCursor = 0;
        g_hack.playerHitboxHistory.clear();

        if (this->m_player1 && this->m_levelLength > 0.f) {
            g_hack.runStartPct = std::clamp((this->m_player1->getPositionX() / this->m_levelLength) * 100.0f, 0.f, 100.f);
        } else {
            g_hack.runStartPct = 0.f;
        }

        if (this->m_glowBatchNode) {
            this->m_glowBatchNode->setVisible(!g_hack.noGlow);
        }

        if (g_hack.instantComplete && !this->m_isPracticeMode) {
            this->playEndAnimationToPos({0.f, 0.f});
        }
    }

    void levelComplete() {
        if (g_hack.isSafeModeActive() && this->m_level) {
            int prevBest = this->m_level->m_normalPercent.value();
            PlayLayer::levelComplete();
            this->m_level->m_normalPercent = prevBest;
            return;
        }
        PlayLayer::levelComplete();
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        if (obj == this->m_anticheatSpike) {
            PlayLayer::destroyPlayer(player, obj);
            return;
        }
        if (g_hack.noclip || g_hack.noCollision) {
            g_hack.noclipHitFrames++;
            if (!g_hack.collidingThisTick) {
                g_hack.noclipDeaths++;
                g_hack.collidingThisTick = true;
            }
            return;
        }

        g_hack.playerIsDead = true;
        if (this->m_player1 && this->m_levelLength > 0.f) {
            float endPct = std::clamp((this->m_player1->getPositionX() / this->m_levelLength) * 100.0f, 0.f, 100.f);
            if ((endPct - g_hack.runStartPct) > (g_hack.bestRunEnd - g_hack.bestRunStart)) {
                g_hack.bestRunStart = g_hack.runStartPct;
                g_hack.bestRunEnd = endPct;
            }
        }
        PlayLayer::destroyPlayer(player, obj);
    }

    void postUpdate(float dt) {
        g_hack.collidingThisTick = false;
        PlayLayer::postUpdate(dt);
        g_hack.totalFrames++;

        if (g_hack.frameExtrapolation && this->m_player1 && !g_hack.playerIsDead) {
            cocos2d::CCPoint cur = this->m_player1->getPosition();
            cocos2d::CCPoint delta = {cur.x - g_hack.p1LastPos.x, cur.y - g_hack.p1LastPos.y};
            if (std::hypot(delta.x, delta.y) < 45.0f) {
                g_hack.p1SmoothVel = {
                    g_hack.p1SmoothVel.x * 0.65f + delta.x * 0.35f,
                    g_hack.p1SmoothVel.y * 0.65f + delta.y * 0.35f
                };
                float subAlpha = std::clamp((dt * g_hack.targetFps) - 1.0f, 0.0f, 0.85f);
                this->m_player1->setPosition({
                    cur.x + g_hack.p1SmoothVel.x * subAlpha * 0.25f,
                    cur.y + g_hack.p1SmoothVel.y * subAlpha * 0.25f
                });
            }
            g_hack.p1LastPos = cur;
        }

        if (g_hack.playerOnTop) {
            if (this->m_player1) {
                this->m_player1->setZOrder(10000);
                if (this->m_player1->m_waveTrail) this->m_player1->m_waveTrail->setZOrder(9999);
            }
            if (this->m_player2) {
                this->m_player2->setZOrder(10000);
                if (this->m_player2->m_waveTrail) this->m_player2->m_waveTrail->setZOrder(9999);
            }
        }

        g_hack.rgbHue = std::fmod(g_hack.rgbHue + dt * 145.0f, 360.0f);
        auto col1 = hsvToRgb(g_hack.rgbHue, 0.85f, 1.0f);
        auto col2 = hsvToRgb(std::fmod(g_hack.rgbHue + 180.0f, 360.0f), 0.85f, 1.0f);

        if (this->m_player1) {
            if (g_hack.p1RgbIcons) this->m_player1->setColor(col1);
            if (g_hack.p1RgbTrail && this->m_player1->m_waveTrail) this->m_player1->m_waveTrail->setColor(col1);
            if (g_hack.p1RgbTrail && this->m_player1->m_regularTrail) this->m_player1->m_regularTrail->setColor(col1);
        }
        if (this->m_player2) {
            if (g_hack.p2RgbIcons) this->m_player2->setColor(col2);
            if (g_hack.p2RgbTrail && this->m_player2->m_waveTrail) this->m_player2->m_waveTrail->setColor(col2);
            if (g_hack.p2RgbTrail && this->m_player2->m_regularTrail) this->m_player2->m_regularTrail->setColor(col2);
        }

        if (g_hack.accuratePercentage && this->m_percentageLabel && this->m_player1 && this->m_levelLength > 0.f) {
            float pct = std::clamp((this->m_player1->getPositionX() / this->m_levelLength) * 100.0f, 0.0f, 100.0f);
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2) << pct << "%";
            this->m_percentageLabel->setString(ss.str().c_str());
        }

        if (g_hack.hitboxTrail && this->m_player1 && !g_hack.playerIsDead) {
            g_hack.playerHitboxHistory.push_back(this->m_player1->getObjectRect());
            if (g_hack.playerHitboxHistory.size() > 45) {
                g_hack.playerHitboxHistory.pop_front();
            }
        }

        if (m_fields->m_drawNode) {
            m_fields->m_drawNode->clear();
            if (g_hack.showTrajectory && this->m_player1 && !g_hack.playerIsDead) {
                renderTrajectory(this->m_player1);
            }
            if ((g_hack.showHitboxes || (g_hack.showHitboxesOnDeath && g_hack.playerIsDead)) && this->m_player1) {
                renderHitboxes();
            }
        }

        renderHud(dt);
    }

    void renderTrajectory(PlayerObject* p) {
        auto draw = m_fields->m_drawNode;
        cocos2d::CCPoint start = p->getPosition();
        float vx = p->m_playerSpeed * 5.77f;
        float vyHold = static_cast<float>(p->m_yVelocity);
        float vyRel  = static_cast<float>(p->m_yVelocity);
        float grav   = p->m_isUpsideDown ? 0.88f : -0.88f;

        cocos2d::CCPoint prevHold = start;
        cocos2d::CCPoint prevRel  = start;

        for (int i = 1; i <= 48; ++i) {
            if (p->m_isShip || (p->m_isSwing && g_hack.shipcopter)) {
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

    void renderHitboxes() {
        auto draw = m_fields->m_drawNode;
        auto drawBox = [&](cocos2d::CCRect r, cocos2d::ccColor4F col) {
            cocos2d::CCPoint pts[4] = {
                {r.getMinX(), r.getMinY()},
                {r.getMaxX(), r.getMinY()},
                {r.getMaxX(), r.getMaxY()},
                {r.getMinX(), r.getMaxY()}
            };
            draw->drawPolygon(pts, 4, {col.r, col.g, col.b, 0.14f}, 0.65f, col);
        };

        if (g_hack.hitboxTrail) {
            for (const auto& oldRect : g_hack.playerHitboxHistory) {
                drawBox(oldRect, {0.15f, 0.9f, 0.5f, 0.45f});
            }
        }

        if (this->m_player1) {
            drawBox(this->m_player1->getObjectRect(), {0.1f, 1.0f, 0.3f, 0.95f});
        }

        if (this->m_objects) {
            float px = this->m_player1 ? this->m_player1->getPositionX() : 0.f;
            CCARRAY_FOREACH_B_TYPE(this->m_objects, obj, GameObject) {
                if (std::abs(obj->getPositionX() - px) < 275.0f) {
                    if (obj->m_objectType == GameObjectType::Hazard || obj->m_objectType == GameObjectType::AnimatedHazard) {
                        drawBox(obj->getObjectRect(), {1.0f, 0.18f, 0.18f, 0.92f});
                    } else if (obj->m_objectType == GameObjectType::Solid || obj->m_objectType == GameObjectType::Slope) {
                        drawBox(obj->getObjectRect(), {0.2f, 0.55f, 1.0f, 0.85f});
                    }
                }
            }
        }
    }

    void renderHud(float dt) {
        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();

        if (m_fields->m_statusDot) {
            m_fields->m_statusDot->setVisible(g_hack.cheatIndicator);
            if (g_hack.cheatIndicator) {
                cocos2d::ccColor3B c = {46, 204, 113};
                if (g_hack.isCheating()) {
                    c = g_hack.isSafeModeActive() ? cocos2d::ccColor3B{241, 196, 15} : cocos2d::ccColor3B{231, 76, 60};
                }
                m_fields->m_statusDot->setColor(c);
            }
        }

        if (!m_fields->m_hudText) return;

        if (g_hack.hudAnchor == 0) {
            m_fields->m_hudText->setAnchorPoint({0.0f, 1.0f});
            m_fields->m_hudText->setPosition({14.0f, winSize.height - 6.0f});
        } else if (g_hack.hudAnchor == 1) {
            m_fields->m_hudText->setAnchorPoint({1.0f, 1.0f});
            m_fields->m_hudText->setPosition({winSize.width - 10.0f, winSize.height - 6.0f});
        } else if (g_hack.hudAnchor == 2) {
            m_fields->m_hudText->setAnchorPoint({0.0f, 0.0f});
            m_fields->m_hudText->setPosition({14.0f, 8.0f});
        } else {
            m_fields->m_hudText->setAnchorPoint({0.5f, 1.0f});
            m_fields->m_hudText->setPosition({winSize.width * 0.5f, winSize.height - 20.0f});
        }

        std::ostringstream ss;
        if (g_hack.showFps) {
            int fps = g_hack.frameExtrapolation
                ? static_cast<int>(g_hack.targetFps)
                : static_cast<int>(std::round(1.0f / std::max(dt, 0.0005f)));
            ss << "FPS: " << fps << " (240 TPS)\n";
        }
        if (g_hack.showCps) {
            float now = cocos2d::CCDirector::sharedDirector()->getTotalFrames() / 60.0f;
            std::erase_if(g_hack.clickTimes, [now](float t) { return (now - t) > 1.0f; });
            ss << "CPS: " << g_hack.clickTimes.size() << "\n";
        }
        if (g_hack.showNoclipAccuracy) {
            float acc = g_hack.totalFrames > 0
                ? (1.0f - static_cast<float>(g_hack.noclipHitFrames) / static_cast<float>(g_hack.totalFrames)) * 100.0f
                : 100.0f;
            ss << "Accuracy: " << std::fixed << std::setprecision(2) << std::clamp(acc, 0.0f, 100.0f) << "%\n";
        }
        if (g_hack.showNoclipDeaths) {
            ss << "Noclip Deaths: " << g_hack.noclipDeaths << "\n";
        }
        if (g_hack.showAttempts && this->m_level) {
            ss << "Attempt " << this->m_level->m_attempts.value() << "\n";
        }
        if (g_hack.showBestRun) {
            ss << "Best Run: " << static_cast<int>(g_hack.bestRunStart) << "% - " << static_cast<int>(g_hack.bestRunEnd) << "%\n";
        }
        if (g_hack.startPosSwitcher && !g_hack.startPositions.empty()) {
            ss << "StartPos [Q/E]: " << (g_hack.currentStartPosIdx + 1) << "/" << g_hack.startPositions.size() << "\n";
        }

        m_fields->m_hudText->setString(ss.str().c_str());
    }
};

class $modify(MHProPauseHook, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        auto menu = cocos2d::CCMenu::create();
        menu->setPosition({0.f, 0.f});
        this->addChild(menu, 100);

        auto mhSpr = ButtonSprite::create("MHv9 [Tab]", "bigFont.fnt", "GJ_button_06.png", 0.5f);
        mhSpr->setScale(0.6f);
        auto mhBtn = CCMenuItemSpriteExtra::create(mhSpr, this, menu_selector(MHProPauseHook::onOpenMod));
        mhBtn->setPosition({winSize.width - 52.0f, winSize.height - 22.0f});
        menu->addChild(mhBtn);

        if (g_hack.levelEditBypass) {
            auto editSpr = ButtonSprite::create("Edit Level", "bigFont.fnt", "GJ_button_01.png", 0.5f);
            editSpr->setScale(0.6f);
            auto editBtn = CCMenuItemSpriteExtra::create(editSpr, this, menu_selector(MHProPauseHook::onOpenEditor));
            editBtn->setPosition({winSize.width - 52.0f, winSize.height - 46.0f});
            menu->addChild(editBtn);
        }
    }

    void onOpenMod(cocos2d::CCObject*) {
        if (auto overlay = MegaHackV9Overlay::getOrCreate()) overlay->toggleMenu();
    }

    void onOpenEditor(cocos2d::CCObject*) {
        if (auto pl = PlayLayer::get()) {
            if (pl->m_level) {
                auto sc = EditLevelLayer::scene(pl->m_level);
                cocos2d::CCDirector::sharedDirector()->replaceScene(cocos2d::CCTransitionFade::create(0.5f, sc));
            }
        }
    }
};

class $modify(MHProMenuHook, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        tintLayerBackground(this);
        return true;
    }
};

class $modify(MHProCreatorHook, CreatorLayer) {
    bool init() {
        if (!CreatorLayer::init()) return false;
        tintLayerBackground(this);
        return true;
    }
};

class $modify(MHProGarageHook, GJGarageLayer) {
    bool init() {
        | GJGarageLayer::init() return false;
        tintLayerBackground(this);

        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        auto menu = cocos2d::CCMenu::create();
        menu->setPosition({0.f, 0.f});
        this->addChild(menu, 100);

        auto spr = ButtonSprite::create("MHv9 Effects", "bigFont.fnt", "GJ_button_06.png", 0.55f);
        spr->setScale(0.62f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(MHProGarageHook::onOpenEffects));
        btn->setPosition({winSize.width - 54.0f, winSize.height - 22.0f});
        menu->addChild(btn);

        return true;
    }

    void onOpenEffects(cocos2d::CCObject*) {
        if (auto overlay = MegaHackV9Overlay::getOrCreate()) overlay->toggleMenu();
    }
};
