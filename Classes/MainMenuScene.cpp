#include "MainMenuScene.h"
#include "GameScene.h"
#include "MapFeatures.h"
#include "RewardStore.h"
#include <ctime>
#include "Interface.h"
#include "editor-support/cocostudio/ActionTimeline/CSLoader.h"
#include "editor-support/cocostudio/ActionTimeline/CCActionTimeline.h"
#include "editor-support/spine/SkeletonAnimation.h"
#include "ui/CocosGUI.h"
#include <algorithm>
#include <cmath>

USING_NS_CC;
namespace {
const char *mapFile = "dialog/LevelSelectScene.csb";

void disableTouches(Node *node) {
    if (auto widget = dynamic_cast<ui::Widget *>(node)) widget->setTouchEnabled(false);
    for (auto child : node->getChildren()) disableTouches(child);
}
}

bool MainMenuScene::init() { return initialize(0); }

MainMenuScene *MainMenuScene::world(int stage) {
    auto scene = new (std::nothrow) MainMenuScene;
    if (scene && scene->initialize(stage)) {
        scene->autorelease();
        return scene;
    }
    CC_SAFE_DELETE(scene);
    return nullptr;
}

bool MainMenuScene::initialize(int stage) {
    if (!Scene::init()) return false;
    gameUI::music(true);
    if (stage) map(stage);
    else title();
    auto key = EventListenerKeyboard::create();
    key->onKeyReleased = [this](EventKeyboard::KeyCode k, Event *) {
        if (k != EventKeyboard::KeyCode::KEY_ESCAPE && k != EventKeyboard::KeyCode::KEY_BACK) return;
        if (features) features->back();
        else if (options) closeSettings();
        else if (scroll) title();
        else Director::getInstance()->end();
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(key, this);
    return true;
}

void MainMenuScene::title() {
    overlay = nullptr;
    unschedule(CC_SCHEDULE_SELECTOR(MainMenuScene::refreshWallet));
    scroll = nullptr;
    removeAllChildren();
    auto cover = spine::SkeletonAnimation::createWithJsonFile(
        "spine/fengmian_logo.json", "spine/fengmian_logo.atlas");
    cover->setPosition(360, 640);
    addChild(cover);
    auto reveal = cover->setAnimation(0, "show", false);
    cover->setAnimation(1, "start", false);
    cover->setTrackCompleteListener(reveal, [cover](spTrackEntry *) {
        cover->setAnimation(1, "start2", true);
    });

    const std::string file = "dialog/FirstScene_0.csb";
    auto layout = CSLoader::createNode(file);
    addChild(layout);
    auto intro = CSLoader::createTimeline(file);
    layout->runAction(intro);
    intro->setAnimationEndCallFunc("active", [intro] { intro->play("last", true); });
    intro->play("active", false);
    auto play = static_cast<ui::Button *>(layout->getChildByName("btnPlay"));
    play->addClickEventListener([this, play](Ref *) {
        play->setTouchEnabled(false);
        gameUI::effect("Click-Button");
        int stage = gameUI::unlocked();
        if (stage == 1) gameUI::go(GameScene::createStage(1));
        else map(stage);
    });
}

void MainMenuScene::map(int stage) {
    scroll = nullptr;
    removeAllChildren();
    unlocked = gameUI::unlocked();
    stagesBySlot.clear();
    stops.clear();
    for (int i = 1; i <= gameUI::levelCount(); ++i) stagesBySlot.emplace(gameUI::mapSlot(i), i);
    lastSlot = stagesBySlot.rbegin()->first;
    auto layout = CSLoader::createNode(mapFile);
    auto base = layout->getChildByName("seg0");
    const float baseHeight = base->getContentSize().height;
    cycleHeight = 0;
    for (int i = 1; i <= 4; ++i) {
        auto segment = layout->getChildByName(StringUtils::format("seg%d", i));
        auto markers = segment->getChildByName("level");
        for (auto marker : markers->getChildren())
            stops.push_back(layout->convertToNodeSpace(marker->convertToWorldSpace(Vec2::ZERO)));
        cycleHeight += segment->getContentSize().height;
    }

    scroll = ui::ScrollView::create();
    scroll->setDirection(ui::ScrollView::Direction::VERTICAL);
    scroll->setContentSize({720, 1280});
    scroll->setScrollBarEnabled(false);
    scroll->setBounceEnabled(false);
    addChild(scroll);
    auto positionFor = [this](int slot) {
        int index = slot - 1;
        return stops[index % stops.size()] + Vec2(0, (index / stops.size()) * cycleHeight);
    };
    float height = std::max(1280.f, positionFor(lastSlot).y + baseHeight);
    scroll->setInnerContainerSize({720, height});
    scroll->setInnerContainerPosition({0, -clampf(positionFor(gameUI::mapSlot(stage)).y - 500, 0, height - 1280)});
    auto firstCycle = static_cast<int>(std::max(0.f, -scroll->getInnerContainerPosition().y - baseHeight) / cycleHeight);
    populateCycle(layout, firstCycle);
    updateMap();
    scroll->addEventListener([this](Ref *, ui::ScrollView::EventType type) {
        if (scroll && type == ui::ScrollView::EventType::CONTAINER_MOVED) updateMap();
    });
    // Let the scroll view own each gesture, even when it starts on a marker.
    scroll->addTouchEventListener([this, positionFor](Ref *, ui::Widget::TouchEventType type) {
        if (options || features || !scroll) return;
        if (type == ui::Widget::TouchEventType::BEGAN) { dragging = false; return; }
        if (type == ui::Widget::TouchEventType::MOVED) {
            dragging |= scroll->getTouchMovePosition().distance(scroll->getTouchBeganPosition()) > 12;
            return;
        }
        if (type != ui::Widget::TouchEventType::ENDED || dragging) return;
        auto point = scroll->getInnerContainer()->convertToNodeSpace(scroll->getTouchEndPosition());
        int cycle = std::max(0, static_cast<int>((point.y - stops.front().y + 48) / cycleHeight));
        for (int i = 0; i < static_cast<int>(stops.size()); ++i) {
            int slot = cycle * static_cast<int>(stops.size()) + i + 1;
            auto it = stagesBySlot.find(slot);
            if (it == stagesBySlot.end() || it->second > unlocked) continue;
            auto p = positionFor(slot);
            if (Rect(p.x - 69, p.y - 39, 138, 78).containsPoint(point)) {
                gameUI::effect("Click-Button");
                gameUI::go(GameScene::createStage(it->second));
                return;
            }
        }
    });

    overlay = CSLoader::createNode("dialog/LevelSelect.csb");
    for (auto child : overlay->getChildren()) child->setVisible(false);
    addChild(overlay);
    auto bind = [this](Node *parent, const char *id, MapPage page) {
        auto button = static_cast<ui::Button *>(parent->getChildByName(id));
        button->setVisible(true);
        button->setTouchEnabled(true);
        button->addClickEventListener([this,page](Ref *) {feature(page);});
    };
    auto daily = overlay->getChildByName("btnDaily");
    daily->setVisible(true);
    bind(daily,"_btn_daily",MapPage::Daily);
    if (gameUI::mapSlot(unlocked)<5) {
        auto b=static_cast<ui::Button *>(daily->getChildByName("_btn_daily"));
        b->loadTextures("images/plist/UI/daily1Lock.png","images/plist/UI/daily1Lock.png","",ui::Widget::TextureResType::PLIST);
    }
    bind(overlay,"btn_video",MapPage::Video);
    bind(overlay,"btn_onetime",MapPage::Shop);
    bind(overlay,"btn_pets",MapPage::Pets);
    bind(overlay,"coin_bar",MapPage::Coins);
    bind(overlay->getChildByName("coin_bar"),"btn_plus",MapPage::Coins);
    auto pets = static_cast<ui::Button *>(overlay->getChildByName("btn_pets"));
    pets->getChildByName("reminder")->setVisible(false);
    if (gameUI::mapSlot(unlocked)<6)
        pets->loadTextures("pets/ui/petsLock.png","pets/ui/petsLock.png","",ui::Widget::TextureResType::PLIST);
    auto setting = static_cast<ui::Button *>(overlay->getChildByName("btn_setting"));
    setting->setVisible(true);
    setting->addClickEventListener([this](Ref *) { if (!options && !features) settings(); });
    refreshWallet(0);
    schedule(CC_SCHEDULE_SELECTOR(MainMenuScene::refreshWallet),1.f);
}

void MainMenuScene::refreshWallet(float) {
    if (!overlay) return;
    auto label = static_cast<ui::Text *>(overlay->getChildByName("coin_bar")->getChildByName("lbl_coin"));
    label->setString(std::to_string(market::wallet().gold));
    auto badge = overlay->getChildByName("btnDaily")->getChildByName("imgOk");
    badge->setVisible(gameUI::mapSlot(unlocked)>=5 && market::wallet().nextDay(ice::localDay(std::time(nullptr)))>=0);
    badge->getChildByName("lbtime")->setVisible(false);
}

void MainMenuScene::feature(MapPage page) {
    if (options || features) return;
    gameUI::effect("Click-Button");
    features = MapFeatures::open(page,gameUI::mapSlot(unlocked),[this] {
        features->removeFromParent();
        features=nullptr;
        scroll->setTouchEnabled(true);
        refreshWallet(0);
    });
    if (features) {
        scroll->setTouchEnabled(false);
        addChild(features,5);
    }
}

void MainMenuScene::populateCycle(Node *layout, int cycle) {
    disableTouches(layout);
    layout->setTag(cycle);
    layout->setPositionY(cycle * cycleHeight);
    layout->getChildByName("seg0")->setVisible(cycle == 0);
    layout->getChildByName("maplock")->setVisible(false);
    int slot = cycle * static_cast<int>(stops.size());
    for (int i = 1; i <= 4; ++i) {
        auto segment = layout->getChildByName(StringUtils::format("seg%d", i));
        for (auto marker : segment->getChildByName("level")->getChildren()) {
            ++slot;
            if (slot > lastSlot) { marker->setVisible(false); continue; }
            auto entry = stagesBySlot.find(slot);
            bool open = entry != stagesBySlot.end() && entry->second <= unlocked;
            int stars = open ? gameUI::best(entry->second, "stars") : 0;
            auto button = static_cast<ui::Button *>(marker->getChildByName("btn"));
            if (!open) button->loadTextureNormal("images/plist/map/level_icon_0_locked.png", ui::Widget::TextureResType::PLIST);
            marker->getChildByName("key")->setVisible(false);
            auto label = static_cast<ui::TextBMFont *>(marker->getChildByName("lbl_level"));
            label->setString(entry == stagesBySlot.end() ? std::to_string(slot) : gameUI::stageLabel(entry->second));
            for (int n = 1; n <= 3; ++n)
                marker->getChildByName(StringUtils::format("star%d", n))->setVisible(n == stars);
        }
    }
    scroll->getInnerContainer()->addChild(layout);
}

void MainMenuScene::updateMap() {
    float bottom = std::max(0.f, -scroll->getInnerContainerPosition().y - 320);
    int first = static_cast<int>(bottom / cycleHeight);
    int last = std::min((lastSlot - 1) / static_cast<int>(stops.size()),
                        static_cast<int>((bottom + 1280) / cycleHeight));
    auto inner = scroll->getInnerContainer();
    for (ssize_t i = inner->getChildrenCount(); i > 0; --i) {
        auto child = inner->getChildren().at(i - 1);
        if (child->getTag() < first || child->getTag() > last) child->removeFromParent();
    }
    for (int cycle = first; cycle <= last; ++cycle)
        if (!inner->getChildByTag(cycle)) populateCycle(CSLoader::createNode(mapFile), cycle);
}

void MainMenuScene::closeSettings() {
    options->removeFromParent();
    options = nullptr;
    scroll->setTouchEnabled(true);
}

void MainMenuScene::settings() {
    if (options) closeSettings();
    options = LayerColor::create(Color4B(8, 34, 57, 210), 720, 1280);
    addChild(options, 5);
    scroll->setTouchEnabled(false);
    gameUI::label(options, gameUI::tr("settings"), 48, 360, 900);
    gameUI::button(options, gameUI::tr(gameUI::sound() ? "soundOn" : "soundOff"), 360, 775, 390, 72,
                   [this] { gameUI::toggleSound(); settings(); });
    gameUI::label(options, gameUI::tr("language"), 29, 360, 670);
    const char *ids[] = {"en", "zh-Hans", "zh-Hant", "ja"};
    const char *names[] = {"English", "ZH (CN)", "ZH (TW)", "JA"};
    for (int i = 0; i < 4; ++i) {
        std::string id = ids[i];
        gameUI::button(options, names[i], 248 + (i % 2) * 224, 598 - (i / 2) * 82, 196, 65,
                       [this, id] { gameUI::language(id); settings(); }, id == gameUI::lang());
    }
    gameUI::button(options, gameUI::tr("back"), 360, 410, 270, 72, [this] { closeSettings(); });
}
