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
#include <Geode/modify/SecretRewardsLayer.hpp>
#include <Geode/modify/CCCircleWave.hpp>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>

using namespace geode::prelude;

// ============================================================================
// 1. COMPLETE MEGA HACK V9 STATE & MACRO SYSTEM (ALL DEFAULT = FALSE)
// ============================================================================
struct MacroAction {
    int frame;
    bool down;
    int button;
    bool isPlayer1;
};

struct HackState {
    // UI State
    bool menuOpen = false;
    int animStyle = 0; // 0: Expo+Back Staggered, 1: Smooth Slide, 2: Elastic Scale

    // 1. Cosmetic
    bool accuratePercentage = false;
    bool ballRotationBug = false;
    bool noGlow = false;
    bool noParticles = false;
    bool keepPortalParticles = true;
    bool hideGroundParticles = false;
    bool hideTrailParticles = false;
    bool hideClickParticles = false;
    bool noPortalCircle = false;
    bool playerOnTop = false;
    bool noWaveTrail = false;
    bool noWavePulse = false;
    bool solidWaveTrail = false;
    bool p1RgbIcons = false;
    bool p2RgbIcons = false;
    bool p1RgbTrail = false;
    bool p2RgbTrail = false;

    // 2. Level & Physics
    bool noclip = false;
    bool noCollision = false;
    bool safeMode = false;
    bool autoSafeMode = false;
    bool forceIce = false;
    bool shipcopter = false;
    bool showTrajectory = false;
    bool showHitboxes = false;
    bool showHitboxesOnDeath = false;
    bool hitboxMultiplier = false;
    float hitboxScale = 0.50f;
    bool fixedRandomSeed = false;
    unsigned int randomSeed = 1337;
    bool instantComplete = false;
    bool practiceMusicSync = false;
    bool frameStepper = false;
    bool stepAdvanceRequested = false;

    // 3. Universal, Bypass & Creator
    bool customBackground = false;
    int bgPresetIndex = 0;
    bool fastChests = false;
    bool iconBypass = false;
    bool copyHack = false;
    bool verifyHack = false;
    bool levelEditBypass = false;

    // 4. Display & Speedhack
    bool speedhackEnabled = false;
    float speedhackValue = 1.00f;
    bool speedhackAudio = false;
    bool frameExtrapolation = false;
    float targetFps = 480.0f;

    // 5. Status HUD
    bool cheatIndicator = false;
    bool showFps = false;
    bool showCps = false;
    bool showNoclipAccuracy = false;
    bool showNoclipDeaths = false;
    bool showAttempts = false;
    bool showBestRun = false;

    // 6. Macro Bot (Replay Engine)
    int macroMode = 0; // 0: Disabled, 1: Recording, 2: Playback
    std::vector<MacroAction> macroBuffer;
    size_t macroPlaybackCursor = 0;
    int physicsFrameCounter = 0;

    // Runtime Metrics
    int totalFrames = 0;
    int noclipHitFrames = 0;
    int noclipDeaths = 0;
    bool wasCollidingLastFrame = false;
    bool isPlayerDead = false;
    float runStartPercent = 0.0f;
    float bestRunFrom = 0.0f;
    float bestRunTo = 0.0f;
    std::vector<float> clickTimestamps;
    bool isCurrentlyHoldingCps = false;
    float rgbTimer = 0.0f;

    // Frame Extrapolation Vectors
    cocos2d::CCPoint p1PrevPos = {0.f, 0.f};
    cocos2d::CCPoint p1Velocity = {0.f, 0.f};
    cocos2d::CCPoint p2PrevPos = {0.f, 0.f};
    cocos2d::CCPoint p2Velocity = {0.f, 0.f};

    bool isAnyCheatActive() const {
        return noclip || noCollision || forceIce || shipcopter ||
               showTrajectory || showHitboxes || hitboxMultiplier ||
               fixedRandomSeed || instantComplete || frameStepper ||
               (macroMode == 2) ||
               (speedhackEnabled && std::abs(speedhackValue - 1.0f) > 0.001f);
    }

    bool shouldBlockProgress() const {
        return safeMode || (autoSafeMode && isAnyCheatActive());
    }
};

static HackState g_state;

// ============================================================================
// 2. COLOR UTILITIES & AUDIO PITCH SYNC
// ============================================================================
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

static const cocos2d::ccColor3B kBgPresets[] = {
    {22, 26, 48},   // Deep Midnight
    {48, 18, 38},   // Royal Crimson
    {16, 44, 40},   // Cyber Emerald
    {28, 28, 34},   // Matte Obsidian
    {58, 36, 18}    // Golden Dusk
};

static void applyCustomBackgroundToLayer(cocos2d::CCNode* layer) {
    if (!layer || !g_state.customBackground) return;
    auto col = kBgPresets[g_state.bgPresetIndex % 5];
    if (auto bg = typeinfo_cast<cocos2d::CCRGBAProtocol*>(layer->getChildByID("background"))) {
        bg->setColor(col);
    }
    CCARRAY_FOREACH_B_TYPE(layer->getChildren(), child, cocos2d::CCSprite) {
        if (child->getZOrder() <= -1) {
            child->setColor(col);
        }
    }
}

static void syncFmodAudioPitch() {
    auto fmod = FMODAudioEngine::sharedEngine();
    if (!fmod || !fmod->m_system) return;
    FMOD::ChannelGroup* masterGroup = nullptr;
    if (fmod->m_system->getMasterChannelGroup(&masterGroup) == FMOD_OK && masterGroup) {
        float pitch = (g_state.speedhackEnabled && g_state.speedhackAudio) ? g_state.speedhackValue : 1.0f;
        masterGroup->setPitch(std::clamp(pitch, 0.1f, 5.0f));
    }
}

// ============================================================================
// 3. MEGA HACK V9 OVERLAY UI (REFINED EASING ANIMATIONS & SUBMENUS)
// ============================================================================
class MegaHackV9Overlay : public cocos2d::CCLayerColor {
public:
    std::vector<cocos2d::CCNode*> m_columns;
    cocos2d::CCNode* m_particlePopup = nullptr;
    cocos2d::CCLabelBMFont* m_speedLabel = nullptr;
    cocos2d::CCLabelBMFont* m_hitboxLabel = nullptr;
    cocos2d::CCLabelBMFont* m_fpsTargetLabel = nullptr;
    cocos2d::CCLabelBMFont* m_seedLabel = nullptr;
    cocos2d::CCLabelBMFont* m_macroStatusLabel = nullptr;
    cocos2d::CCLabelBMFont* m_animStyleLabel = nullptr;

    static MegaHackV9Overlay* getOrCreate() {
        auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
        if (!scene) return nullptr;
        auto existing = typeinfo_cast<MegaHackV9Overlay*>(scene->getChildByID("megahack-v9-overlay"_spr));
        if (existing) return existing;

        auto ret = new MegaHackV9Overlay();
        if (ret && ret->init()) {
            ret->autorelease();
            ret->setID("megahack-v9-overlay"_spr);
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

        float colWidth = 89.0f;
        float gap = 4.5f;
        float totalW = 6 * colWidth + 5 * gap;
        float startX = (winSize.width - totalW) * 0.5f + colWidth * 0.5f;
        float topY = winSize.height - 16.0f;

        buildColumnCosmetic({startX + 0 * (colWidth + gap), topY}, colWidth);
        buildColumnLevel({startX + 1 * (colWidth + gap), topY}, colWidth);
        buildColumnUniversal({startX + 2 * (colWidth + gap), topY}, colWidth);
        buildColumnDisplaySpeed({startX + 3 * (colWidth + gap), topY}, colWidth);
        buildColumnStatus({startX + 4 * (colWidth + gap), topY}, colWidth);
        buildColumnBypassBot({startX + 5 * (colWidth + gap), topY}, colWidth);

        buildParticleSubmenu({winSize.width * 0.5f, winSize.height * 0.5f});

        return true;
    }

    cocos2d::CCNode* createWindowFrame(const char* title, cocos2d::CCPoint topCenter, float width, float height) {
        auto container = cocos2d::CCNode::create();
        container->setPosition(topCenter);
        container->setContentSize({width, height});
        container->setAnchorPoint({0.5f, 1.0f});
        container->setCascadeOpacityEnabled(true);

        // Outer subtle border
        auto border = cocos2d::CCLayerColor::create({55, 55, 68, 255}, width + 1.5f, height + 1.5f);
        border->ignoreAnchorPointForPosition(false);
        border->setAnchorPoint({0.5f, 1.0f});
        border->setPosition({width * 0.5f, height + 0.75f});
        container->addChild(border, -1);

        // Dark Body
        auto body = cocos2d::CCLayerColor::create({18, 18, 23, 246}, width, height);
        body->ignoreAnchorPointForPosition(false);
        body->setAnchorPoint({0.5f, 1.0f});
        body->setPosition({width * 0.5f, height});
        container->addChild(body, 0);

        // Mega Hack v9 Signature Crimson Header
        float headerH = 14.5f;
        auto header = cocos2d::CCLayerColor::create({206, 40, 40, 255}, width, headerH);
        header->ignoreAnchorPointForPosition(false);
        header->setAnchorPoint({0.5f, 1.0f});
        header->setPosition({width * 0.5f, height});
        container->addChild(header, 1);

        auto titleLbl = cocos2d::CCLabelBMFont::create(title, "bigFont.fnt");
        titleLbl->setScale(0.23f);
        titleLbl->setPosition({width * 0.5f, height - headerH * 0.5f});
        container->addChild(titleLbl, 2);

        auto menu = cocos2d::CCMenu::create();
        menu->setPosition({0.f, 0.f});
        menu->setID("column-menu");
        container->addChild(menu, 3);

        this->addChild(container);
        m_columns.push_back(container);
        return container;
    }

    struct ToggleData : public cocos2d::CCObject {
        bool* ptr;
        cocos2d::CCLayerColor* bg;
        cocos2d::CCLayerColor* bar;
        cocos2d::CCLabelBMFont* lbl;
        std::function<void()> callback;
    };

    struct ActionData : public cocos2d::CCObject {
        std::function<void()> fn;
    };

    void addToggleRow(cocos2d::CCNode* col, float& curY, const char* labelText, bool* statePtr, std::function<void()> onChange = nullptr, std::function<void()> onArrowClick = nullptr) {
        float width = col->getContentSize().width;
        float rowH = 13.8f;
        auto menu = typeinfo_cast<cocos2d::CCMenu*>(col->getChildByID("column-menu"));
        if (!menu) return;

        auto rowBg = cocos2d::CCLayerColor::create(
            *statePtr ? cocos2d::ccColor4B{206, 40, 40, 95} : cocos2d::ccColor4B{30, 30, 38, 225},
            width - 5.0f, rowH - 1.4f
        );
        rowBg->ignoreAnchorPointForPosition(false);
        rowBg->setAnchorPoint({0.5f, 0.5f});

        auto bar = cocos2d::CCLayerColor::create(
            *statePtr ? cocos2d::ccColor4B{240, 58, 58, 255} : cocos2d::ccColor4B{70, 70, 82, 255},
            2.5f, rowH - 1.4f
        );
        bar->ignoreAnchorPointForPosition(false);
        bar->setAnchorPoint({0.0f, 0.0f});
        bar->setPosition({0.0f, 0.0f});
        rowBg->addChild(bar);

        auto lbl = cocos2d::CCLabelBMFont::create(labelText, "chatFont.fnt");
        lbl->setScale(0.36f);
        lbl->setAnchorPoint({0.0f, 0.5f});
        lbl->setPosition({5.5f, (rowH - 1.4f) * 0.5f});
        lbl->setColor(*statePtr ? cocos2d::ccColor3B{255, 255, 255} : cocos2d::ccColor3B{190, 192, 205});
        rowBg->addChild(lbl);

        auto btn = CCMenuItemSpriteExtra::create(rowBg, this, menu_selector(MegaHackV9Overlay::onGenericToggle));
        btn->setPosition({width * 0.5f, curY});
        btn->m_scaleMultiplier = 1.02f;

        auto data = new ToggleData();
        data->autorelease();
        data->ptr = statePtr;
        data->bg = rowBg;
        data->bar = bar;
        data->lbl = lbl;
        data->callback = onChange;
        btn->setUserObject(data);
        menu->addChild(btn);

        if (onArrowClick) {
            auto arrowLbl = cocos2d::CCLabelBMFont::create("<", "bigFont.fnt");
            arrowLbl->setScale(0.21f);
            arrowLbl->setColor({245, 190, 75});
            auto arrowBtn = CCMenuItemSpriteExtra::create(arrowLbl, this, menu_selector(MegaHackV9Overlay::onGenericAction));
            arrowBtn->setPosition({width - 9.0f, curY});
            auto act = new ActionData();
            act->autorelease();
            act->fn = onArrowClick;
            arrowBtn->setUserObject(act);
            menu->addChild(arrowBtn, 5);
        }

        curY -= rowH;
    }

    void addAdjusterRow(cocos2d::CCNode* col, float& curY, const char* initText, cocos2d::CCLabelBMFont** outLabel, std::function<void(int)> onStep) {
        float width = col->getContentSize().width;
        float rowH = 13.8f;
        auto menu = typeinfo_cast<cocos2d::CCMenu*>(col->getChildByID("column-menu"));
        if (!menu) return;

        auto lbl = cocos2d::CCLabelBMFont::create(initText, "chatFont.fnt");
        lbl->setScale(0.35f);
        lbl->setAnchorPoint({0.5f, 0.5f});
        lbl->setPosition({width * 0.5f, curY});
        lbl->setColor({245, 215, 125});
        col->addChild(lbl, 4);
        if (outLabel) *outLabel = lbl;

        auto makeStepBtn = [&](const char* sym, float xPos, int dir) {
            auto sLbl = cocos2d::CCLabelBMFont::create(sym, "bigFont.fnt");
            sLbl->setScale(0.24f);
            auto btn = CCMenuItemSpriteExtra::create(sLbl, this, menu_selector(MegaHackV9Overlay::onGenericAction));
            btn->setPosition({xPos, curY});
            auto act = new ActionData();
            act->autorelease();
            act->fn = [onStep, dir]() { onStep(dir); };
            btn->setUserObject(act);
            menu->addChild(btn);
        };

        makeStepBtn("-", 9.5f, -1);
        makeStepBtn("+", width - 9.5f, +1);

        curY -= rowH;
    }

    void addActionButtonRow(cocos2d::CCNode* col, float& curY, const char* btnText, cocos2d::CCLabelBMFont** outLabel, std::function<void()> onClick) {
        float width = col->getContentSize().width;
        float rowH = 14.2f;
        auto menu = typeinfo_cast<cocos2d::CCMenu*>(col->getChildByID("column-menu"));
        if (!menu) return;

        auto rowBg = cocos2d::CCLayerColor::create({42, 46, 62, 240}, width - 6.0f, rowH - 1.6f);
        rowBg->ignoreAnchorPointForPosition(false);
        rowBg->setAnchorPoint({0.5f, 0.5f});

        auto lbl = cocos2d::CCLabelBMFont::create(btnText, "chatFont.fnt");
        lbl->setScale(0.36f);
        lbl->setPosition({(width - 6.0f) * 0.5f, (rowH - 1.6f) * 0.5f});
        lbl->setColor({130, 220, 255});
        rowBg->addChild(lbl);
        if (outLabel) *outLabel = lbl;

        auto btn = CCMenuItemSpriteExtra::create(rowBg, this, menu_selector(MegaHackV9Overlay::onGenericAction));
        btn->setPosition({width * 0.5f, curY});
        auto act = new ActionData();
        act->autorelease();
        act->fn = onClick;
        btn->setUserObject(act);
        menu->addChild(btn);

        curY -= rowH;
    }

    void onGenericToggle(cocos2d::CCObject* sender) {
        auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);
        auto data = static_cast<ToggleData*>(btn->getUserObject());
        if (!data || !data->ptr) return;

        *(data->ptr) = !(*(data->ptr));
        bool active = *(data->ptr);

        data->bg->stopAllActions();
        data->bg->runAction(cocos2d::CCTintTo::create(0.12f, active ? 206 : 30, active ? 40 : 30, active ? 40 : 38));
        data->bg->setOpacity(active ? 95 : 225);
        data->bar->setColor(active ? cocos2d::ccColor3B{240, 58, 58} : cocos2d::ccColor3B{70, 70, 82});
        data->lbl->setColor(active ? cocos2d::ccColor3B{255, 255, 255} : cocos2d::ccColor3B{190, 192, 205});

        if (data->callback) data->callback();
    }

    void onGenericAction(cocos2d::CCObject* sender) {
        auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);
        auto act = static_cast<ActionData*>(btn->getUserObject());
        if (act && act->fn) act->fn();
    }

    // ------------------------------------------------------------------------
    // Column Builders (All V9 Features)
    // ------------------------------------------------------------------------
    void buildColumnCosmetic(cocos2d::CCPoint pos, float w) {
        float h = 212.0f;
        auto col = createWindowFrame("Cosmetic", pos, w, h);
        float y = h - 22.0f;

        addToggleRow(col, y, "Accurate %", &g_state.accuratePercentage);
        addToggleRow(col, y, "Ball Rotation Bug", &g_state.ballRotationBug);
        addToggleRow(col, y, "No Glow", &g_state.noGlow, []() {
            if (auto pl = PlayLayer::get()) {
                if (pl->m_glowBatchNode) pl->m_glowBatchNode->setVisible(!g_state.noGlow);
            }
        });
        addToggleRow(col, y, "No Particles", &g_state.noParticles, nullptr, [this]() {
            if (m_particlePopup) {
                bool show = !m_particlePopup->isVisible();
                m_particlePopup->setVisible(show);
                if (show) {
                    m_particlePopup->setScale(0.82f);
                    m_particlePopup->runAction(cocos2d::CCEaseBackOut::create(cocos2d::CCScaleTo::create(0.18f, 1.0f)));
                }
            }
        });
        addToggleRow(col, y, "No Portal Circle", &g_state.noPortalCircle);
        addToggleRow(col, y, "Player on Top", &g_state.playerOnTop);
        addToggleRow(col, y, "No Wave Trail", &g_state.noWaveTrail);
        addToggleRow(col, y, "No Wave Pulse", &g_state.noWavePulse);
        addToggleRow(col, y, "Solid Wave Trail", &g_state.solidWaveTrail);
        addToggleRow(col, y, "P1 RGB Icons", &g_state.p1RgbIcons);
        addToggleRow(col, y, "P2 RGB Icons", &g_state.p2RgbIcons);
        addToggleRow(col, y, "P1 RGB Trail", &g_state.p1RgbTrail);
        addToggleRow(col, y, "P2 RGB Trail", &g_state.p2RgbTrail);
    }

    void buildColumnLevel(cocos2d::CCPoint pos, float w) {
        float h = 254.0f;
        auto col = createWindowFrame("Level", pos, w, h);
        float y = h - 22.0f;

        addToggleRow(col, y, "Noclip", &g_state.noclip);
        addToggleRow(col, y, "No Collision", &g_state.noCollision);
        addToggleRow(col, y, "Safe Mode", &g_state.safeMode);
        addToggleRow(col, y, "Auto Safe Mode", &g_state.autoSafeMode);
        addToggleRow(col, y, "Force Ice", &g_state.forceIce);
        addToggleRow(col, y, "Shipcopter", &g_state.shipcopter);
        addToggleRow(col, y, "Show Trajectory", &g_state.showTrajectory);
        addToggleRow(col, y, "Show Hitboxes", &g_state.showHitboxes);
        addToggleRow(col, y, "Hitboxes on Death", &g_state.showHitboxesOnDeath);
        addToggleRow(col, y, "Hitbox Multiplier", &g_state.hitboxMultiplier);
        addAdjusterRow(col, y, "Hitbox: 0.50x", &m_hitboxLabel, [this](int dir) {
            g_state.hitboxScale = std::clamp(g_state.hitboxScale + dir * 0.1f, 0.1f, 3.0f);
            if (m_hitboxLabel) {
                std::ostringstream ss;
                ss << "Hitbox: " << std::fixed << std::setprecision(2) << g_state.hitboxScale << "x";
                m_hitboxLabel->setString(ss.str().c_str());
            }
        });
        addToggleRow(col, y, "Random Seed", &g_state.fixedRandomSeed);
        addAdjusterRow(col, y, "Seed: 1337", &m_seedLabel, [this](int dir) {
            g_state.randomSeed = static_cast<unsigned int>(std::max(1, static_cast<int>(g_state.randomSeed) + dir * 111));
            if (m_seedLabel) {
                m_seedLabel->setString(fmt::format("Seed: {}", g_state.randomSeed).c_str());
            }
        });
        addToggleRow(col, y, "Instant Complete", &g_state.instantComplete, []() {
            if (g_state.instantComplete) {
                if (auto pl = PlayLayer::get()) {
                    pl->playEndAnimationToPos({0.f, 0.f});
                }
            }
        });
        addToggleRow(col, y, "Practice Music", &g_state.practiceMusicSync);
        addToggleRow(col, y, "Frame Step [C]", &g_state.frameStepper);
    }

    void buildColumnUniversal(cocos2d::CCPoint pos, float w) {
        float h = 165.0f;
        auto col = createWindowFrame("Universal", pos, w, h);
        float y = h - 22.0f;

        addToggleRow(col, y, "Custom BG", &g_state.customBackground, []() {
            auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
            if (scene && scene->getChildrenCount() > 0) {
                applyCustomBackgroundToLayer(static_cast<cocos2d::CCNode*>(scene->getChildren()->objectAtIndex(0)));
            }
        }, []() {
            g_state.bgPresetIndex = (g_state.bgPresetIndex + 1) % 5;
            auto scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
            if (scene && scene->getChildrenCount() > 0) {
                applyCustomBackgroundToLayer(static_cast<cocos2d::CCNode*>(scene->getChildren()->objectAtIndex(0)));
            }
        });
        addToggleRow(col, y, "Fast Chests", &g_state.fastChests);
        addActionButtonRow(col, y, "Anim: Smooth Expo", &m_animStyleLabel, [this]() {
            g_state.animStyle = (g_state.animStyle + 1) % 3;
            const char* names[] = {"Anim: Smooth Expo", "Anim: Slide Down", "Anim: Elastic Pop"};
            if (m_animStyleLabel) m_animStyleLabel->setString(names[g_state.animStyle]);
        });
        addActionButtonRow(col, y, "Reset All Hacks", nullptr, []() {
            g_state = HackState();
            g_state.menuOpen = true;
            syncFmodAudioPitch();
            cocos2d::CCApplication::sharedApplication()->setAnimationInterval(1.0 / 240.0);
            Notification::create("All Mega Hack v9 Toggles Reset to OFF", NotificationIcon::Success)->show();
        });
    }

    void buildColumnDisplaySpeed(cocos2d::CCPoint pos, float w) {
        float h = 185.0f;
        auto col = createWindowFrame("Display / Speed", pos, w, h);
        float y = h - 22.0f;

        addToggleRow(col, y, "Speedhack", &g_state.speedhackEnabled, []() {
            syncFmodAudioPitch();
        });
        addAdjusterRow(col, y, "Speed: 1.00x", &m_speedLabel, [this](int dir) {
            g_state.speedhackValue = std::clamp(g_state.speedhackValue + dir * 0.1f, 0.1f, 5.0f);
            if (m_speedLabel) {
                std::ostringstream ss;
                ss << "Speed: " << std::fixed << std::setprecision(2) << g_state.speedhackValue << "x";
                m_speedLabel->setString(ss.str().c_str());
            }
            syncFmodAudioPitch();
        });
        addToggleRow(col, y, "Speedhack Audio", &g_state.speedhackAudio, []() {
            syncFmodAudioPitch();
        });
        addToggleRow(col, y, "Frame Extrap.", &g_state.frameExtrapolation, []() {
            float fps = g_state.frameExtrapolation ? g_state.targetFps : 240.0f;
            cocos2d::CCApplication::sharedApplication()->setAnimationInterval(1.0 / fps);
        });
        addAdjusterRow(col, y, "FPS: 480", &m_fpsTargetLabel, [this](int dir) {
            g_state.targetFps = std::clamp(g_state.targetFps + dir * 120.0f, 60.0f, 1440.0f);
            if (m_fpsTargetLabel) {
                m_fpsTargetLabel->setString(fmt::format("FPS: {}", static_cast<int>(g_state.targetFps)).c_str());
            }
            if (g_state.frameExtrapolation) {
                cocos2d::CCApplication::sharedApplication()->setAnimationInterval(1.0 / g_state.targetFps);
            }
        });
    }

    void buildColumnStatus(cocos2d::CCPoint pos, float w) {
        float h = 185.0f;
        auto col = createWindowFrame("Status HUD", pos, w, h);
        float y = h - 22.0f;

        addToggleRow(col, y, "Cheat Indicator", &g_state.cheatIndicator);
        addToggleRow(col, y, "FPS Counter", &g_state.showFps);
        addToggleRow(col, y, "CPS Counter", &g_state.showCps);
        addToggleRow(col, y, "Noclip Accuracy", &g_state.showNoclipAccuracy);
        addToggleRow(col, y, "Noclip Deaths", &g_state.showNoclipDeaths);
        addToggleRow(col, y, "Attempts", &g_state.showAttempts);
        addToggleRow(col, y, "Best Run", &g_state.showBestRun);
    }

    void buildColumnBypassBot(cocos2d::CCPoint pos, float w) {
        float h = 212.0f;
        auto col = createWindowFrame("Bypass & Bot", pos, w, h);
        float y = h - 22.0f;

        addToggleRow(col, y, "Unlock Icons", &g_state.iconBypass);
        addToggleRow(col, y, "Copy Hack", &g_state.copyHack);
        addToggleRow(col, y, "Verify Hack", &g_state.verifyHack);
        addToggleRow(col, y, "Level Edit", &g_state.levelEditBypass);

        addActionButtonRow(col, y, "Bot: Disabled", &m_macroStatusLabel, [this]() {
            g_state.macroMode = (g_state.macroMode + 1) % 3;
            if (g_state.macroMode == 1) {
                g_state.macroBuffer.clear();
                if (m_macroStatusLabel) m_macroStatusLabel->setString("Bot: Recording");
            } else if (g_state.macroMode == 2) {
                g_state.macroPlaybackCursor = 0;
                if (m_macroStatusLabel) m_macroStatusLabel->setString("Bot: Playback");
            } else {
                if (m_macroStatusLabel) m_macroStatusLabel->setString("Bot: Disabled");
            }
        });

        addActionButtonRow(col, y, "Clear Macro", nullptr, [this]() {
            g_state.macroBuffer.clear();
            g_state.macroPlaybackCursor = 0;
            g_state.macroMode = 0;
            if (m_macroStatusLabel) m_macroStatusLabel->setString("Bot: Disabled");
            Notification::create("Macro Replay Cleared", NotificationIcon::Info)->show();
        });
    }

    void buildParticleSubmenu(cocos2d::CCPoint centerPos) {
        float w = 138.0f;
        float h = 112.0f;
        m_particlePopup = cocos2d::CCNode::create();
        m_particlePopup->setPosition(centerPos);
        m_particlePopup->setContentSize({w, h});
        m_particlePopup->setAnchorPoint({0.5f, 0.5f});
        m_particlePopup->setVisible(false);

        auto bg = cocos2d::CCLayerColor::create({14, 14, 18, 252}, w, h);
        bg->ignoreAnchorPointForPosition(false);
        bg->setAnchorPoint({0.f, 0.f});
        m_particlePopup->addChild(bg, 0);

        auto header = cocos2d::CCLayerColor::create({206, 40, 40, 255}, w, 14.5f);
        header->ignoreAnchorPointForPosition(false);
        header->setAnchorPoint({0.f, 1.f});
        header->setPosition({0.f, h});
        m_particlePopup->addChild(header, 1);

        auto title = cocos2d::CCLabelBMFont::create("Particle Filter", "bigFont.fnt");
        title->setScale(0.22f);
        title->setPosition({w * 0.5f, h - 7.2f});
        m_particlePopup->addChild(title, 2);

        auto menu = cocos2d::CCMenu::create();
        menu->setPosition({0.f, 0.f});
        menu->setID("column-menu");
        m_particlePopup->addChild(menu, 3);

        float y = h - 23.0f;
        addToggleRow(m_particlePopup, y, "Portal Effect (Keep)", &g_state.keepPortalParticles);
        addToggleRow(m_particlePopup, y, "Hide Ground Part.", &g_state.hideGroundParticles);
        addToggleRow(m_particlePopup, y, "Hide Trail Part.", &g_state.hideTrailParticles);
        addToggleRow(m_particlePopup, y, "Hide Click Part.", &g_state.hideClickParticles);

        this->addChild(m_particlePopup, 100);
    }

    void toggleMenu() {
        g_state.menuOpen = !g_state.menuOpen;
        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        float topY = winSize.height - 16.0f;

        this->stopAllActions();

        if (g_state.menuOpen) {
            this->setVisible(true);
            this->setTouchEnabled(true);
            this->setOpacity(0);
            this->runAction(cocos2d::CCEaseSineOut::create(cocos2d::CCFadeTo::create(0.20f, 165)));

            for (size_t i = 0; i < m_columns.size(); ++i) {
                auto col = m_columns[i];
                col->stopAllActions();
                cocos2d::CCPoint targetPos = {col->getPositionX(), topY};
                float delay = static_cast<float>(i) * 0.020f;

                if (g_state.animStyle == 0) {
                    col->setPosition({targetPos.x, topY + 48.0f});
                    col->setScale(0.88f);
                    auto moveAct = cocos2d::CCEaseBackOut::create(cocos2d::CCMoveTo::create(0.26f, targetPos));
                    auto scaleAct = cocos2d::CCEaseExponentialOut::create(cocos2d::CCScaleTo::create(0.24f, 1.0f));
                    col->runAction(cocos2d::CCSequence::create(
                        cocos2d::CCDelayTime::create(delay),
                        cocos2d::CCSpawn::createWithTwoActions(moveAct, scaleAct),
                        nullptr
                    ));
                } else if (g_state.animStyle == 1) {
                    col->setPosition({targetPos.x, topY + 90.0f});
                    col->setScale(1.0f);
                    col->runAction(cocos2d::CCSequence::create(
                        cocos2d::CCDelayTime::create(delay),
                        cocos2d::CCEaseExponentialOut::create(cocos2d::CCMoveTo::create(0.24f, targetPos)),
                        nullptr
                    ));
                } else {
                    col->setPosition(targetPos);
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
                cocos2d::CCPoint targetPos = {col->getPositionX(), topY + 36.0f};
                auto moveOut = cocos2d::CCEaseBackIn::create(cocos2d::CCMoveTo::create(0.16f, targetPos));
                auto scaleOut = cocos2d::CCEaseSineIn::create(cocos2d::CCScaleTo::create(0.16f, 0.90f));
                col->runAction(cocos2d::CCSpawn::createWithTwoActions(moveOut, scaleOut));
            }

            this->runAction(cocos2d::CCSequence::create(
                cocos2d::CCFadeTo::create(0.17f, 0),
                cocos2d::CCHide::create(),
                nullptr
            ));
        }
    }
};

// ============================================================================
// 4. KEYBOARD HOOKS (TAB MENU & 'C' FRAME STEPPER)
// ============================================================================
class $modify(MHKeyboardHook, cocos2d::CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(cocos2d::enumKeyCodes key, bool isKeyDown, bool isKeyRepeat) {
        if (key == cocos2d::enumKeyCodes::KEY_Tab && isKeyDown && !isKeyRepeat) {
            if (auto overlay = MegaHackV9Overlay::getOrCreate()) {
                overlay->toggleMenu();
                return true;
            }
        }
        if (g_state.frameStepper && key == cocos2d::enumKeyCodes::KEY_C && isKeyDown) {
            g_state.stepAdvanceRequested = true;
            return true;
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat);
    }
};

// ============================================================================
// 5. ICON BYPASS & PRACTICE MUSIC SYNC HOOKS (GameManager & GameStatsManager)
// ============================================================================
class $modify(MHGameManagerHook, GameManager) {
    bool isIconUnlocked(int id, IconType type) {
        if (g_state.iconBypass) return true;
        return GameManager::isIconUnlocked(id, type);
    }

    bool isColorUnlocked(int id, UnlockType type) {
        if (g_state.iconBypass) return true;
        return GameManager::isColorUnlocked(id, type);
    }
};

class $modify(MHGameStatsManagerHook, GameStatsManager) {
    bool isItemEnabled(UnlockType type, int id) {
        // UnlockType::GJItem (12) + ID 17 = Practice Music Sync diamond shop item
        if (g_state.practiceMusicSync && static_cast<int>(type) == 12 && id == 17) {
            return true;
        }
        return GameStatsManager::isItemEnabled(type, id);
    }
};

// ============================================================================
// 6. COPY HACK, VERIFY HACK, LEVEL EDIT & FAST CHESTS HOOKS
// ============================================================================
class $modify(MHLevelInfoLayerHook, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (g_state.copyHack && level) {
            level->m_isEditable = true;
            level->m_password = 1; // Free Copy
            level->m_originalLevel = 0; // No (C) Mark
        }
        return LevelInfoLayer::init(level, challenge);
    }
};

class $modify(MHEditLevelLayerHook, EditLevelLayer) {
    bool init(GJGameLevel* level) {
        if (level) {
            if (g_state.verifyHack) {
                level->m_isVerified = true;
                level->m_isVerifiedRaw = true;
            }
            if (g_state.copyHack) {
                level->m_originalLevel = 0;
            }
        }
        return EditLevelLayer::init(level);
    }
};

class $modify(MHRewardUnlockHook, RewardUnlockLayer) {
    void showCollectReward(GJRewardItem* item) {
        RewardUnlockLayer::showCollectReward(item);
        if (g_state.fastChests) {
            this->stopAllActions();
            if (this->m_rewardbut) {
                this->m_rewardbut->setVisible(true);
                this->m_rewardbut->setEnabled(true);
            }
        }
    }
};

// ============================================================================
// 7. SPEEDHACK & PORTAL CIRCLE & HITBOX MULTIPLIER HOOKS
// ============================================================================
class $modify(MHSchedulerHook, cocos2d::CCScheduler) {
    void update(float dt) {
        if (g_state.speedhackEnabled && g_state.speedhackValue > 0.01f) {
            dt *= g_state.speedhackValue;
        }
        CCScheduler::update(dt);
    }
};

class $modify(MHCircleWaveHook, CCCircleWave) {
    void draw() {
        if (g_state.noPortalCircle && PlayLayer::get()) return;
        CCCircleWave::draw();
    }
};

class $modify(MHGameObjectHook, GameObject) {
    cocos2d::CCRect getObjectRect(float fl1, float fl2) {
        if (g_state.hitboxMultiplier && PlayLayer::get()) {
            if (this->m_objectType == GameObjectType::Hazard ||
                this->m_objectType == GameObjectType::AnimatedHazard) {
                return GameObject::getObjectRect(fl1 * g_state.hitboxScale, fl2 * g_state.hitboxScale);
            }
        }
        return GameObject::getObjectRect(fl1, fl2);
    }
};

// ============================================================================
// 8. PLAYER OBJECT HOOKS (Shipcopter, Force Ice, Ball Bug, Wave & Particles)
// ============================================================================
class $modify(MHPlayerObjectHook, PlayerObject) {
    bool pushButton(PlayerButton p0) {
        if (p0 == PlayerButton::Jump && PlayLayer::get() && !g_state.isCurrentlyHoldingCps) {
            g_state.isCurrentlyHoldingCps = true;
            float now = cocos2d::CCDirector::sharedDirector()->getTotalFrames() / 60.0f;
            g_state.clickTimestamps.push_back(now);
        }

        if (g_state.shipcopter && this->m_isSwing && p0 == PlayerButton::Jump) {
            this->m_isHolding = true;
            return true;
        }
        return PlayerObject::pushButton(p0);
    }

    bool releaseButton(PlayerButton p0) {
        if (p0 == PlayerButton::Jump) {
            g_state.isCurrentlyHoldingCps = false;
        }
        if (g_state.shipcopter && this->m_isSwing && p0 == PlayerButton::Jump) {
            this->m_isHolding = false;
            return true;
        }
        return PlayerObject::releaseButton(p0);
    }

    void update(float dt) {
        if (g_state.forceIce) {
            this->m_isOnIce = true;
        }

        if (g_state.shipcopter && this->m_isSwing) {
            float accel = this->m_isHolding ? 0.88f : -0.75f;
            if (this->m_isUpsideDown) accel = -accel;
            this->m_yVelocity = std::clamp(this->m_yVelocity + accel * dt * 2.25f, -14.0f, 14.0f);
        }

        PlayerObject::update(dt);

        if (g_state.ballRotationBug && this->m_isBall) {
            this->m_mainSprite->setRotation(this->m_mainSprite->getRotation() + (this->m_isUpsideDown ? -19.0f : 19.0f) * dt);
        }

        if (g_state.noParticles) {
            bool hideGround = g_state.hideGroundParticles || !g_state.keepPortalParticles;
            bool hideTrail  = g_state.hideTrailParticles  || !g_state.keepPortalParticles;
            bool hideClick  = g_state.hideClickParticles  || !g_state.keepPortalParticles;

            if (this->m_playerGroundParticles) this->m_playerGroundParticles->setVisible(!hideGround);
            if (this->m_trailingParticles)     this->m_trailingParticles->setVisible(!hideTrail);
            if (this->m_shipClickParticles)    this->m_shipClickParticles->setVisible(!hideClick);
            if (this->m_ufoClickParticles)     this->m_ufoClickParticles->setVisible(!hideClick);
        }

        if (this->m_waveTrail) {
            if (g_state.noWaveTrail) {
                this->m_waveTrail->setVisible(false);
            } else {
                this->m_waveTrail->setVisible(true);
                if (g_state.noWavePulse) {
                    this->m_waveTrail->m_pulseSize = 0.5f;
                }
                if (g_state.solidWaveTrail) {
                    this->m_waveTrail->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});
                    this->m_waveTrail->setOpacity(255);
                }
            }
        }
    }
};

// ============================================================================
// 9. GJBASEGAMELAYER HOOKS (Frame Stepper, Macro Bot, No Collision, Particles)
// ============================================================================
class $modify(MHBaseGameLayerHook, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (g_state.macroMode == 1 && PlayLayer::get()) {
            g_state.macroBuffer.push_back({
                g_state.physicsFrameCounter,
                down,
                button,
                isPlayer1
            });
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void update(float dt) {
        if (g_state.frameStepper && PlayLayer::get()) {
            if (!g_state.stepAdvanceRequested) {
                return; // Freeze physics until 'C' is pressed
            }
            g_state.stepAdvanceRequested = false;
            dt = 1.0f / 240.0f;
        }

        g_state.physicsFrameCounter++;

        // Deterministic Macro Playback
        if (g_state.macroMode == 2 && PlayLayer::get()) {
            while (g_state.macroPlaybackCursor < g_state.macroBuffer.size() &&
                   g_state.macroBuffer[g_state.macroPlaybackCursor].frame <= g_state.physicsFrameCounter) {
                const auto& act = g_state.macroBuffer[g_state.macroPlaybackCursor];
                GJBaseGameLayer::handleButton(act.down, act.button, act.isPlayer1);
                g_state.macroPlaybackCursor++;
            }
        }

        GJBaseGameLayer::update(dt);
    }

    void collisionCheckObjects(PlayerObject* player, gd::vector<GameObject*>* objs, int count, float dt) {
        if (g_state.noCollision) return;
        GJBaseGameLayer::collisionCheckObjects(player, objs, count, dt);
    }

    cocos2d::CCParticleSystemQuad* spawnParticle(char const* plist, int zOrder, cocos2d::tCCPositionType posType, cocos2d::CCPoint pos) {
        if (g_state.noParticles) {
            std::string name = plist ? plist : "";
            bool isPortal = (name.find("portal") != std::string::npos || name.find("Portal") != std::string::npos);
            if (!(isPortal && g_state.keepPortalParticles)) {
                return nullptr;
            }
        }
        return GJBaseGameLayer::spawnParticle(plist, zOrder, posType, pos);
    }
};

// ============================================================================
// 10. PLAYLAYER HOOKS (Safe Mode, Noclip, Extrapolation, Trajectory, HUD)
// ============================================================================
class $modify(MHPlayLayerHook, PlayLayer) {
    struct Fields {
        cocos2d::CCDrawNode* m_overlayDraw = nullptr;
        cocos2d::CCLabelBMFont* m_hudLabel = nullptr;
        cocos2d::CCLayerColor* m_cheatDot = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        m_fields->m_overlayDraw = cocos2d::CCDrawNode::create();
        if (this->m_objectLayer) {
            this->m_objectLayer->addChild(m_fields->m_overlayDraw, 9999);
        }

        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        m_fields->m_hudLabel = cocos2d::CCLabelBMFont::create("", "chatFont.fnt");
        m_fields->m_hudLabel->setScale(0.44f);
        m_fields->m_hudLabel->setAnchorPoint({0.0f, 1.0f});
        m_fields->m_hudLabel->setPosition({14.0f, winSize.height - 6.0f});
        m_fields->m_hudLabel->setOpacity(220);
        this->addChild(m_fields->m_hudLabel, 9998);

        m_fields->m_cheatDot = cocos2d::CCLayerColor::create({46, 204, 113, 255}, 5.0f, 5.0f);
        m_fields->m_cheatDot->setPosition({6.0f, winSize.height - 11.0f});
        m_fields->m_cheatDot->setVisible(false);
        this->addChild(m_fields->m_cheatDot, 9998);

        return true;
    }

    void resetLevel() {
        if (g_state.fixedRandomSeed) {
            std::srand(g_state.randomSeed);
        }
        PlayLayer::resetLevel();

        g_state.totalFrames = 0;
        g_state.noclipHitFrames = 0;
        g_state.noclipDeaths = 0;
        g_state.wasCollidingLastFrame = false;
        g_state.isPlayerDead = false;
        g_state.physicsFrameCounter = 0;
        g_state.macroPlaybackCursor = 0;

        if (this->m_player1 && this->m_levelLength > 0.f) {
            g_state.runStartPercent = std::clamp((this->m_player1->getPositionX() / this->m_levelLength) * 100.0f, 0.f, 100.f);
        } else {
            g_state.runStartPercent = 0.f;
        }

        if (this->m_glowBatchNode) {
            this->m_glowBatchNode->setVisible(!g_state.noGlow);
        }

        if (g_state.instantComplete && !this->m_isPracticeMode) {
            this->playEndAnimationToPos({0.f, 0.f});
        }
    }

    void levelComplete() {
        if (g_state.shouldBlockProgress() && this->m_level) {
            // Safe Mode: Show end screen without saving illegal new best percentage or stars
            int savedPct = this->m_level->m_normalPercent.value();
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
        if (g_state.noclip || g_state.noCollision) {
            g_state.noclipHitFrames++;
            if (!g_state.wasCollidingLastFrame) {
                g_state.noclipDeaths++;
                g_state.wasCollidingLastFrame = true;
            }
            return;
        }

        g_state.isPlayerDead = true;
        if (this->m_player1 && this->m_levelLength > 0.f) {
            float endPct = std::clamp((this->m_player1->getPositionX() / this->m_levelLength) * 100.0f, 0.f, 100.f);
            if ((endPct - g_state.runStartPercent) > (g_state.bestRunTo - g_state.bestRunFrom)) {
                g_state.bestRunFrom = g_state.runStartPercent;
                g_state.bestRunTo = endPct;
            }
        }
        PlayLayer::destroyPlayer(player, obj);
    }

    void postUpdate(float dt) {
        g_state.wasCollidingLastFrame = false;
        PlayLayer::postUpdate(dt);
        g_state.totalFrames++;

        // 1. Sub-frame Extrapolation Smoothing
        if (g_state.frameExtrapolation && this->m_player1 && !g_state.isPlayerDead) {
            cocos2d::CCPoint cur = this->m_player1->getPosition();
            cocos2d::CCPoint delta = {cur.x - g_state.p1PrevPos.x, cur.y - g_state.p1PrevPos.y};
            if (std::hypot(delta.x, delta.y) < 45.0f) {
                g_state.p1Velocity = {
                    g_state.p1Velocity.x * 0.65f + delta.x * 0.35f,
                    g_state.p1Velocity.y * 0.65f + delta.y * 0.35f
                };
                float alpha = std::clamp((dt * g_state.targetFps) - 1.0f, 0.0f, 0.85f);
                this->m_player1->setPosition({
                    cur.x + g_state.p1Velocity.x * alpha * 0.25f,
                    cur.y + g_state.p1Velocity.y * alpha * 0.25f
                });
            }
            g_state.p1PrevPos = cur;
        }

        // 2. Player on Top
        if (g_state.playerOnTop) {
            if (this->m_player1) {
                this->m_player1->setZOrder(10000);
                if (this->m_player1->m_waveTrail) this->m_player1->m_waveTrail->setZOrder(9999);
            }
            if (this->m_player2) {
                this->m_player2->setZOrder(10000);
                if (this->m_player2->m_waveTrail) this->m_player2->m_waveTrail->setZOrder(9999);
            }
        }

        // 3. P1 & P2 RGB Icons / Trail
        g_state.rgbTimer = std::fmod(g_state.rgbTimer + dt * 145.0f, 360.0f);
        auto rgb1 = hsvToRgb(g_state.rgbTimer, 0.85f, 1.0f);
        auto rgb2 = hsvToRgb(std::fmod(g_state.rgbTimer + 180.0f, 360.0f), 0.85f, 1.0f);

        if (this->m_player1) {
            if (g_state.p1RgbIcons) this->m_player1->setColor(rgb1);
            if (g_state.p1RgbTrail && this->m_player1->m_waveTrail) this->m_player1->m_waveTrail->setColor(rgb1);
            if (g_state.p1RgbTrail && this->m_player1->m_regularTrail) this->m_player1->m_regularTrail->setColor(rgb1);
        }
        if (this->m_player2) {
            if (g_state.p2RgbIcons) this->m_player2->setColor(rgb2);
            if (g_state.p2RgbTrail && this->m_player2->m_waveTrail) this->m_player2->m_waveTrail->setColor(rgb2);
            if (g_state.p2RgbTrail && this->m_player2->m_regularTrail) this->m_player2->m_regularTrail->setColor(rgb2);
        }

        // 4. Accurate Percentage
        if (g_state.accuratePercentage && this->m_percentageLabel && this->m_player1 && this->m_levelLength > 0.f) {
            float pct = std::clamp((this->m_player1->getPositionX() / this->m_levelLength) * 100.0f, 0.0f, 100.0f);
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2) << pct << "%";
            this->m_percentageLabel->setString(ss.str().c_str());
        }

        // 5. Trajectory & Hitbox Overlay
        if (m_fields->m_overlayDraw) {
            m_fields->m_overlayDraw->clear();
            if (g_state.showTrajectory && this->m_player1 && !g_state.isPlayerDead) {
                drawPredictedTrajectory(this->m_player1);
            }
            if ((g_state.showHitboxes || (g_state.showHitboxesOnDeath && g_state.isPlayerDead)) && this->m_player1) {
                drawPlayerAndObjectHitboxes();
            }
        }

        // 6. Status HUD
        updateStatusHud(dt);
    }

    void drawPredictedTrajectory(PlayerObject* p) {
        auto draw = m_fields->m_overlayDraw;
        cocos2d::CCPoint start = p->getPosition();
        float vx = p->m_playerSpeed * 5.77f;
        float vyHold = static_cast<float>(p->m_yVelocity);
        float vyRel  = static_cast<float>(p->m_yVelocity);
        float grav   = p->m_isUpsideDown ? 0.88f : -0.88f;

        cocos2d::CCPoint prevHold = start;
        cocos2d::CCPoint prevRel  = start;

        for (int step = 1; step <= 48; ++step) {
            if (p->m_isShip || (p->m_isSwing && g_state.shipcopter)) {
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

    void drawPlayerAndObjectHitboxes() {
        auto draw = m_fields->m_overlayDraw;
        auto drawRectOutline = [&](cocos2d::CCRect r, cocos2d::ccColor4F col) {
            cocos2d::CCPoint verts[4] = {
                {r.getMinX(), r.getMinY()},
                {r.getMaxX(), r.getMinY()},
                {r.getMaxX(), r.getMaxY()},
                {r.getMinX(), r.getMaxY()}
            };
            draw->drawPolygon(verts, 4, {col.r, col.g, col.b, 0.14f}, 0.65f, col);
        };

        if (this->m_player1) {
            drawRectOutline(this->m_player1->getObjectRect(), {0.1f, 1.0f, 0.3f, 0.95f});
        }
        if (this->m_objects) {
            float px = this->m_player1 ? this->m_player1->getPositionX() : 0.f;
            CCARRAY_FOREACH_B_TYPE(this->m_objects, obj, GameObject) {
                if (std::abs(obj->getPositionX() - px) < 270.0f) {
                    if (obj->m_objectType == GameObjectType::Hazard || obj->m_objectType == GameObjectType::AnimatedHazard) {
                        drawRectOutline(obj->getObjectRect(), {1.0f, 0.18f, 0.18f, 0.9f});
                    } else if (obj->m_objectType == GameObjectType::Solid) {
                        drawRectOutline(obj->getObjectRect(), {0.2f, 0.55f, 1.0f, 0.85f});
                    }
                }
            }
        }
    }

    void updateStatusHud(float dt) {
        if (m_fields->m_cheatDot) {
            m_fields->m_cheatDot->setVisible(g_state.cheatIndicator);
            if (g_state.cheatIndicator) {
                cocos2d::ccColor3B dotCol = {46, 204, 113}; // Clean Green
                if (g_state.isAnyCheatActive()) {
                    dotCol = g_state.shouldBlockProgress() ? cocos2d::ccColor3B{241, 196, 15} : cocos2d::ccColor3B{231, 76, 60};
                }
                m_fields->m_cheatDot->setColor(dotCol);
            }
        }

        if (!m_fields->m_hudLabel) return;
        std::ostringstream hud;

        if (g_state.showFps) {
            int displayFps = g_state.frameExtrapolation
                ? static_cast<int>(g_state.targetFps)
                : static_cast<int>(std::round(1.0f / std::max(dt, 0.0005f)));
            hud << "FPS: " << displayFps << " (240 TPS)\n";
        }
        if (g_state.showCps) {
            float now = cocos2d::CCDirector::sharedDirector()->getTotalFrames() / 60.0f;
            std::erase_if(g_state.clickTimestamps, [now](float t) { return (now - t) > 1.0f; });
            hud << "CPS: " << g_state.clickTimestamps.size() << "\n";
        }
        if (g_state.showNoclipAccuracy) {
            float acc = g_state.totalFrames > 0
                ? (1.0f - static_cast<float>(g_state.noclipHitFrames) / static_cast<float>(g_state.totalFrames)) * 100.0f
                : 100.0f;
            hud << "Accuracy: " << std::fixed << std::setprecision(2) << std::clamp(acc, 0.0f, 100.0f) << "%\n";
        }
        if (g_state.showNoclipDeaths) {
            hud << "Noclip Deaths: " << g_state.noclipDeaths << "\n";
        }
        if (g_state.showAttempts && this->m_level) {
            hud << "Attempt " << this->m_level->m_attempts.value() << "\n";
        }
        if (g_state.showBestRun) {
            hud << "Best Run: " << static_cast<int>(g_state.bestRunFrom) << "% - " << static_cast<int>(g_state.bestRunTo) << "%\n";
        }

        m_fields->m_hudLabel->setString(hud.str().c_str());
    }
};

// ============================================================================
// 11. PAUSE LAYER, GARAGE & MENU HOOKS (Level Edit Bypass + Quick MHv9 Button)
// ============================================================================
class $modify(MHPauseLayerHook, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        auto menu = cocos2d::CCMenu::create();
        menu->setPosition({0.f, 0.f});
        this->addChild(menu, 100);

        // Quick MHv9 Toggle Button in Pause Menu
        auto mhSpr = ButtonSprite::create("MHv9 [Tab]", "bigFont.fnt", "GJ_button_06.png", 0.5f);
        mhSpr->setScale(0.6f);
        auto mhBtn = CCMenuItemSpriteExtra::create(mhSpr, this, menu_selector(MHPauseLayerHook::onOpenMHv9));
        mhBtn->setPosition({winSize.width - 52.0f, winSize.height - 22.0f});
        menu->addChild(mhBtn);

        // Level Edit Bypass Button in Pause Menu
        if (g_state.levelEditBypass) {
            auto editSpr = ButtonSprite::create("Edit Level", "bigFont.fnt", "GJ_button_01.png", 0.5f);
            editSpr->setScale(0.6f);
            auto editBtn = CCMenuItemSpriteExtra::create(editSpr, this, menu_selector(MHPauseLayerHook::onForceEditLevel));
            editBtn->setPosition({winSize.width - 52.0f, winSize.height - 46.0f});
            menu->addChild(editBtn);
        }
    }

    void onOpenMHv9(cocos2d::CCObject*) {
        if (auto overlay = MegaHackV9Overlay::getOrCreate()) {
            overlay->toggleMenu();
        }
    }

    void onForceEditLevel(cocos2d::CCObject*) {
        if (auto pl = PlayLayer::get()) {
            if (pl->m_level) {
                auto scene = EditLevelLayer::scene(pl->m_level);
                cocos2d::CCDirector::sharedDirector()->replaceScene(cocos2d::CCTransitionFade::create(0.5f, scene));
            }
        }
    }
};

class $modify(MHMenuLayerHook, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        applyCustomBackgroundToLayer(this);
        return true;
    }
};

class $modify(MHCreatorLayerHook, CreatorLayer) {
    bool init() {
        if (!CreatorLayer::init()) return false;
        applyCustomBackgroundToLayer(this);
        return true;
    }
};

class $modify(MHGarageLayerHook, GJGarageLayer) {
    bool init() {
        if (!GJGarageLayer::init()) return false;
        applyCustomBackgroundToLayer(this);

        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        auto menu = cocos2d::CCMenu::create();
        menu->setPosition({0.f, 0.f});
        this->addChild(menu, 100);

        auto spr = ButtonSprite::create("MHv9 Effects", "bigFont.fnt", "GJ_button_06.png", 0.55f);
        spr->setScale(0.62f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(MHGarageLayerHook::onOpenV9Effects));
        btn->setPosition({winSize.width - 54.0f, winSize.height - 22.0f});
        menu->addChild(btn);

        return true;
    }

    void onOpenV9Effects(cocos2d::CCObject*) {
        if (auto overlay = MegaHackV9Overlay::getOrCreate()) {
            overlay->toggleMenu();
        }
    }
};
