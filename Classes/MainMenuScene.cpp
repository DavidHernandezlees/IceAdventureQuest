#include "MainMenuScene.h"
#include "GameScene.h"
#include "MapFeatures.h"
#include "RewardStore.h"
#include <ctime>
#include "Interface.h"
#include "editor-support/cocostudio/CCComExtensionData.h"
#include "editor-support/cocostudio/ActionTimeline/CSLoader.h"
#include "editor-support/cocostudio/ActionTimeline/CCActionTimeline.h"
#include "editor-support/spine/SkeletonAnimation.h"
#include "ui/CocosGUI.h"
#include <algorithm>
#include <cmath>

USING_NS_CC;
namespace {
const char *mapFile = "dialog/LevelSelectScene.csb";
constexpr float viewHeight = 1280;
constexpr float baseHeight = 320;
constexpr int chapterSize = 30;

void bounceIcon(Node *icon, float delay) {
    float x = icon->getScaleX(), y = icon->getScaleY();
    auto scale = [x,y](float seconds, float width, float height) {
        return ScaleTo::create(seconds, x*width, y*height);
    };
    icon->runAction(RepeatForever::create(Sequence::create(DelayTime::create(delay),
        scale(.08f,1.2f,.8f), scale(.08f,.8f,1.2f), scale(.10f,1.15f,.85f),
        scale(.10f,.9f,1.1f), scale(.13f,1.1f,.9f), scale(.13f,.9f,1.1f),
        scale(.18f,1.05f,.95f), scale(.18f,1,1), nullptr)));
}

void disableTouches(Node *node) {
    if (auto widget = dynamic_cast<ui::Widget *>(node)) widget->setTouchEnabled(false);
    for (auto child : node->getChildren()) disableTouches(child);
}
}

bool MainMenuScene::init() { return initialize(0); }

MainMenuScene *MainMenuScene::world(int stage, int previousStage) {
    auto scene = new (std::nothrow) MainMenuScene;
    if (scene && scene->initialize(stage, previousStage)) {
        scene->autorelease();
        return scene;
    }
    CC_SAFE_DELETE(scene);
    return nullptr;
}

bool MainMenuScene::initialize(int stage, int previousStage) {
    if (!Scene::init()) return false;
    gameUI::music(true);
    if (stage) map(stage, previousStage);
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
    cycles = nullptr;
    sceneryData = nullptr;
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

Vec2 MainMenuScene::positionFor(int slot) const {
    int index = slot - 1;
    return stops[index % stops.size()] + Vec2(0,(index / stops.size()) * cycleHeight);
}

void MainMenuScene::centerOn(int slot) {
    float limit = scroll->getInnerContainerSize().height - viewHeight;
    scroll->stopAutoScroll();
    scroll->setInnerContainerPosition({0, -clampf(positionFor(slot).y - viewHeight / 3, 0, limit)});
}

void MainMenuScene::map(int stage, int previousStage) {
    scroll = nullptr;
    removeAllChildren();
    unlocked = gameUI::unlocked();
    stagesBySlot.clear();
    stops.clear();
    frontier = gameUI::mapSlot(unlocked);
    totalStars = 0;
    chapterStars.clear();
    stageStars.assign(unlocked+1,0);
    for (int i = 1; i <= gameUI::levelCount(); ++i) {
        int slot = gameUI::mapSlot(i);
        stagesBySlot.emplace(slot,i);
        if (i <= unlocked) {
            int stars = gameUI::best(i,"stars");
            stageStars[i] = stars;
            totalStars += stars;
            chapterStars[(slot-1)/chapterSize] += stars;
        }
    }
    lastSlot = stagesBySlot.rbegin()->first;
    auto layout = CSLoader::createNode(mapFile);
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
    int preview = std::min(lastSlot,frontier + 4);
    float height = std::max(viewHeight,std::floor(positionFor(preview).y) + viewHeight*2/3);
    scroll->setInnerContainerSize({720,height});
    auto inner = scroll->getInnerContainer();
    cycles = Node::create();
    inner->addChild(cycles);
    centerOn(gameUI::mapSlot(stage));

    auto fog = layout->getChildByName("maplock");
    fog->retain();
    fog->removeFromParent();
    inner->addChild(fog,3);
    fog->release();
    fog->setPosition(360,height - fog->getContentSize().height/2 + 100);
    fog->setVisible(true);
    static_cast<ui::Widget *>(fog)->setTouchEnabled(false);

    const std::string markerFile = "effect/ani_mark_top.csb";
    auto current = CSLoader::createNode(markerFile);
    inner->addChild(current,2);
    auto loop = CSLoader::createTimeline(markerFile);
    current->runAction(loop);
    loop->play("animation0",true);
    Vec2 target = positionFor(frontier) + Vec2(0,40);
    current->setPosition(target);
    if (previousStage > 0 && previousStage + 1 == unlocked) {
        Vec2 start = positionFor(gameUI::mapSlot(previousStage)) + Vec2(0,40);
        if (std::abs(target.y-start.y) < viewHeight) {
            current->setPosition(start);
            current->runAction(EaseSineInOut::create(MoveTo::create(1,target)));
        }
    }
    int firstCycle = int(std::max(0.f, -inner->getPositionY() - baseHeight) / cycleHeight);
    populateCycle(layout,firstCycle);
    updateMap();
    scroll->addEventListener([this](Ref *, ui::ScrollView::EventType type) {
        if (scroll && type == ui::ScrollView::EventType::CONTAINER_MOVED) updateMap();
    });
    // Let the scroll view own each gesture, even when it starts on a marker.
    scroll->addTouchEventListener([this](Ref *, ui::Widget::TouchEventType type) {
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
                scroll->stopAutoScroll();
                _eventDispatcher->pauseEventListenersForTarget(this,true);
                auto tile = cycles->getChildByTag(cycle);
                for (int segment=1; tile && segment<=4; ++segment) {
                    auto levels = tile->getChildByName(StringUtils::format("seg%d",segment))->getChildByName("level");
                    if (auto stop = levels->getChildByTag(slot)) {
                        stop->runAction(Sequence::create(ScaleBy::create(.07f,.90f),
                            ScaleBy::create(.11f,1/.90f),nullptr));
                        break;
                    }
                }
                int selected = it->second;
                runAction(Sequence::create(DelayTime::create(.18f),CallFunc::create([selected] {
                    gameUI::go(GameScene::createStage(selected));
                }),nullptr));
                return;
            }
        }
    });

    overlay = CSLoader::createNode("dialog/LevelSelect.csb");
    for (auto child : overlay->getChildren()) child->setVisible(false);
    addChild(overlay);
    for (auto name : {"Image_1_0","Image_1","Image_2"}) {
        auto panel = static_cast<ui::ImageView *>(overlay->getChildByName(name));
        panel->setTouchEnabled(false);
        panel->setVisible(true);
    }
    auto bind = [this](Node *parent, const char *id, MapPage page) {
        auto button = static_cast<ui::Button *>(parent->getChildByName(id));
        button->setVisible(true);
        button->setTouchEnabled(true);
        button->setPressedActionEnabled(true);
        button->addClickEventListener([this,page](Ref *) {feature(page);});
    };
    auto daily = overlay->getChildByName("btnDaily");
    daily->setVisible(true);
    bounceIcon(daily,2.8f);
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
    bounceIcon(overlay->getChildByName("btn_video"),2.4f);
    bounceIcon(overlay->getChildByName("btn_onetime"),2.2f);
    bounceIcon(pets,2.6f);
    if (gameUI::mapSlot(unlocked)<6)
        pets->loadTextures("pets/ui/petsLock.png","pets/ui/petsLock.png","",ui::Widget::TextureResType::PLIST);
    auto pin = static_cast<ui::Button *>(overlay->getChildByName("btn_map"));
    pin->setVisible(true);
    pin->setPressedActionEnabled(true);
    pin->addClickEventListener([this](Ref *) { if (!options && !features) showChapters(); });
    auto stars = static_cast<ui::Button *>(overlay->getChildByName("stars_bar"));
    stars->setVisible(true);
    stars->setPressedActionEnabled(true);
    updateStarBar(stars);
    stars->addClickEventListener([this](Ref *) { if (!options && !features) showStarProgress(); });
    auto setting = static_cast<ui::Button *>(overlay->getChildByName("btn_setting"));
    setting->setVisible(true);
    setting->setPressedActionEnabled(true);
    setting->addClickEventListener([this](Ref *) { if (!options && !features) settings(); });
    int seen = UserDefault::getInstance()->getIntegerForKey("map.seenPetUnlock",0);
    bool newPet = std::any_of(ice::petUnlocks.begin(),ice::petUnlocks.end(),
        [this,seen](int milestone) { return milestone > seen && milestone <= frontier; });
    pets->getChildByName("reminder")->setVisible(newPet);
    refreshWallet(0);
    schedule(CC_SCHEDULE_SELECTOR(MainMenuScene::refreshWallet),1.f);
}

void MainMenuScene::refreshWallet(float) {
    if (!overlay) return;
    auto label = static_cast<ui::Text *>(overlay->getChildByName("coin_bar")->getChildByName("lbl_coin"));
    label->setString(std::to_string(market::wallet().gold));
    auto badge = static_cast<ui::ImageView *>(overlay->getChildByName("btnDaily")->getChildByName("imgOk"));
    badge->setTouchEnabled(false);
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
        if (page == MapPage::Pets && frontier >= ice::petUnlocks[0]) {
            UserDefault::getInstance()->setIntegerForKey("map.seenPetUnlock",frontier);
            UserDefault::getInstance()->flush();
            overlay->getChildByName("btn_pets")->getChildByName("reminder")->setVisible(false);
        }
        scroll->stopAutoScroll();
        scroll->setTouchEnabled(false);
        addChild(features,5);
    }
}

void MainMenuScene::populateCycle(Node *layout, int cycle) {
    disableTouches(layout);
    layout->setTag(cycle);
    layout->setPositionY(cycle * cycleHeight);
    layout->getChildByName("seg0")->setVisible(cycle == 0);
    if (auto extraFog = layout->getChildByName("maplock")) extraFog->removeFromParent();
    int slot = cycle * static_cast<int>(stops.size());
    for (int i = 1; i <= 4; ++i) {
        auto segment = layout->getChildByName(StringUtils::format("seg%d", i));
        for (auto marker : segment->getChildByName("level")->getChildren()) {
            marker->setTag(++slot);
            if (slot > lastSlot) { marker->setVisible(false); continue; }
            auto entry = stagesBySlot.find(slot);
            bool open = entry != stagesBySlot.end() && entry->second <= unlocked;
            int stars = open ? stageStars[entry->second] : 0;
            auto button = static_cast<ui::Button *>(marker->getChildByName("btn"));
            if (!open) button->loadTextureNormal("images/plist/map/level_icon_0_locked.png", ui::Widget::TextureResType::PLIST);
            marker->getChildByName("key")->setVisible(slot >= 2 && slot <= 7 && slot >= frontier);
            auto label = static_cast<ui::TextBMFont *>(marker->getChildByName("lbl_level"));
            label->setVisible(open);
            if (open) label->setString(gameUI::stageLabel(entry->second));
            for (int n = 1; n <= 3; ++n)
                marker->getChildByName(StringUtils::format("star%d", n))->setVisible(n == stars);
        }
    }
    cycles->addChild(layout);
}

void MainMenuScene::updateMap() {
    auto inner = scroll->getInnerContainer();
    float bottom = -inner->getPositionY();
    int first = int(std::max(0.f,bottom-baseHeight) / cycleHeight);
    int last = std::min((lastSlot-1)/int(stops.size()),
                        int(std::max(0.f,bottom+viewHeight-baseHeight)/cycleHeight));
    for (ssize_t i = cycles->getChildrenCount(); i > 0; --i) {
        auto child = cycles->getChildren().at(i-1);
        if (child->getTag() < first || child->getTag() > last) child->removeFromParent();
    }
    for (int cycle = first; cycle <= last; ++cycle) {
        auto tile = cycles->getChildByTag(cycle);
        if (!tile) {
            tile = CSLoader::createNode(mapFile);
            populateCycle(tile,cycle);
        }
        for (int i=1; i<=4; ++i) {
            auto anchors = tile->getChildByName(StringUtils::format("seg%d",i))->getChildByName("ani");
            for (auto anchor : anchors->getChildren()) {
                float y = inner->convertToNodeSpace(anchor->convertToWorldSpace(Vec2::ZERO)).y;
                if (y < bottom-350 || y > bottom+viewHeight+350) {
                    if (anchor->getChildrenCount()) anchor->removeAllChildren();
                    continue;
                }
                if (anchor->getChildrenCount()) continue;
                auto meta = static_cast<cocostudio::ComExtensionData *>(
                    anchor->getComponent(cocostudio::ComExtensionData::COMPONENT_NAME));
                if (!meta) continue;
                if (!sceneryData)
                    sceneryData = spine::SkeletonAnimation::createWithJsonFile(
                        "spine/map_animation.json","spine/map_animation.atlas");
                auto owner = static_cast<spine::SkeletonAnimation *>(sceneryData.get());
                auto actor = spine::SkeletonAnimation::createWithData(owner->getSkeleton()->data);
                actor->setUserObject(owner); // Keep shared atlas and attachments alive through actor teardown.
                actor->setAnimation(0,meta->getCustomProperty(),true);
                anchor->addChild(actor);
            }
        }
    }
}

void MainMenuScene::openOverlay() {
    options = LayerColor::create(Color4B(8,34,57,210),720,viewHeight);
    addChild(options,5);
    scroll->stopAutoScroll();
    scroll->setTouchEnabled(false);
    auto blocker = ui::Layout::create();
    blocker->setContentSize(options->getContentSize());
    blocker->setTouchEnabled(true);
    options->addChild(blocker);
}

void MainMenuScene::showChapters() {
    gameUI::effect("Click-Button");
    openOverlay();
    auto drawer = CSLoader::createNode("dialog/MapDlgNew.csb");
    options->addChild(drawer);
    auto close = static_cast<ui::Button *>(drawer->getChildByName("btn_close"));
    close->addClickEventListener([this](Ref *) { closeSettings(); });
    auto list = static_cast<ui::ScrollView *>(drawer->getChildByName("scroll"));
    // Only show chapters with playable entries, plus the next locked chapter.
    std::vector<int> chapters;
    for (auto it = stagesBySlot.begin(); it != stagesBySlot.end();) {
        int chapter = (it->first-1)/chapterSize;
        chapters.push_back(chapter);
        if (it->first > frontier) break;
        it = stagesBySlot.upper_bound((chapter+1)*chapterSize);
    }
    float height = std::max(viewHeight,210.f*chapters.size());
    list->setInnerContainerSize({300,height});
    for (size_t i=0; i<chapters.size(); ++i) {
        int chapter = chapters[i], start = chapter*chapterSize+1;
        auto first = stagesBySlot.lower_bound(start);
        bool available = first->first <= frontier;
        auto row = CSLoader::createNode("dialog/MapNavNode.csb");
        list->getInnerContainer()->addChild(row);
        row->setPosition(150,120+i*210);
        row->getChildByName("img_lock")->setVisible(!available);
        auto button = static_cast<ui::Button *>(row->getChildByName("btn_mapnav"));
        button->setVisible(available);
        button->setPressedActionEnabled(true);
        int slot = first->first;
        button->loadTextureNormal(StringUtils::format("images/plist/mapnav/BC_landmark_symbol_%d.png",1+chapter%5),
                                  ui::Widget::TextureResType::PLIST);
        button->addClickEventListener([this,slot](Ref *) { closeSettings(); centerOn(slot); });
        auto bar = row->getChildByName("label_star");
        auto label = static_cast<ui::Text *>(bar->getChildByName("lbl_starnumber"));
        int entries = int(std::distance(first,stagesBySlot.upper_bound(start+chapterSize-1)));
        label->setString(StringUtils::format("%d/%d",chapterStars[chapter],entries*3));
        gameUI::label(row,StringUtils::format("%d–%d",start,std::min(lastSlot,start+chapterSize-1)),25,0,85);
    }
    list->jumpToTop();
}

void MainMenuScene::updateStarBar(Node *bar) {
    int claimed = market::wallet().starChests;
    int progress = totalStars - (claimed/3)*60;
    static_cast<ui::LoadingBar *>(bar->getChildByName("progress"))->setPercent(clampf(progress*100.f/60,0,100));
    for (int i=1; i<=3; ++i) {
        auto chest = bar->getChildByName(StringUtils::format("bonus%d",i));
        chest->stopAllActions();
        chest->setOpacity(i <= claimed%3 ? 100 : 255);
        if (i == claimed%3+1 && totalStars/20 > claimed)
            chest->runAction(RepeatForever::create(Sequence::create(FadeTo::create(.55f,150),
                FadeTo::create(.55f,255),nullptr)));
    }
}

void MainMenuScene::showStarProgress() {
    gameUI::effect("Click-Button");
    openOverlay();
    gameUI::panel(options,75,460,570,400,Color4F(.04f,.19f,.35f,.98f));
    int claimed = market::wallet().starChests;
    auto chest = Sprite::create("images/new/box/sbox.png");
    chest->setScale(.55f);
    chest->setPosition(360,780);
    options->addChild(chest);
    int next = (claimed+1)*20;
    gameUI::label(options,StringUtils::format("%d / %d",totalStars,next),40,360,680);
    if (totalStars >= next)
        gameUI::button(options,gameUI::tr("claim"),360,595,260,70,[this] {
            int first = RandomHelper::random_int(0,3), second = RandomHelper::random_int(0,3);
            bool awarded = market::change([this,first,second](ice::Rewards &r) {
                return r.claimStarChest(totalStars,first,second);
            });
            if (!awarded) {
                if (!options->getChildByName("saveError"))
                    gameUI::label(options,gameUI::tr("saveError"),25,360,445)->setName("saveError");
                return;
            }
            auto bar = overlay->getChildByName("stars_bar");
            updateStarBar(bar);
            closeSettings();
            const char *items[] = {"hammer","stripe","bomb","rainbow"};
            std::string rewards = gameUI::tr(items[first])+" +1";
            if (market::wallet().starChests%3==0) rewards += "   "+gameUI::tr(items[second])+" +1";
            auto receipt = gameUI::label(overlay,rewards,32,360,1118);
            receipt->enableOutline(Color4B(13,42,70,255),3);
            receipt->runAction(Sequence::create(DelayTime::create(1.5f),FadeOut::create(.35f),RemoveSelf::create(),nullptr));
        },true);
    gameUI::button(options,gameUI::tr("back"),360,505,220,65,[this] { closeSettings(); });
}

void MainMenuScene::closeSettings() {
    options->removeFromParent();
    options = nullptr;
    scroll->setTouchEnabled(true);
}

void MainMenuScene::settings() {
    if (options) closeSettings();
    openOverlay();
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
