#include "MainMenuScene.h"
#include "GameScene.h"
#include "Interface.h"
#include <algorithm>
USING_NS_CC;
namespace {
const Color4F blue(.035, .13, .26, .91);
const Color3B frost(163, 231, 251);
} // namespace
bool MainMenuScene::init() {
    if (!Scene::init())
        return false;
    gameUI::music(true);
    title();
    auto key = EventListenerKeyboard::create();
    key->onKeyReleased = [this](EventKeyboard::KeyCode k, Event *) {
        if (k != EventKeyboard::KeyCode::KEY_ESCAPE && k != EventKeyboard::KeyCode::KEY_BACK)
            return;
        if (current == -1)
            Director::getInstance()->end();
        else
            title();
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(key, this);
    return true;
}
MainMenuScene *MainMenuScene::world(int page) {
    auto s = create();
    s->map(page);
    return s;
}
void MainMenuScene::title() {
    removeAllChildren();
    current = -1;
    gameUI::back(this, "loading.jpg");
    auto logo = gameUI::picture(this, "title-logo.png", 360, 1020, 480);
    if (logo)
        logo->runAction(RepeatForever::create(
            Sequence::create(MoveBy::create(1.8f, {0, 8}), MoveBy::create(1.8f, {0, -8}), nullptr)));
    gameUI::panel(this, 94, 83, 532, 326, blue, 42);
    bool orig=gameUI::originalCampaign();
    gameUI::button(this,"2021",246,344,210,65,[this]{gameUI::selectCampaign(true); title();},orig);
    gameUI::button(this,gameUI::tr("custom"),474,344,210,65,[this]{gameUI::selectCampaign(false); title();},!orig);
    gameUI::button(this, gameUI::tr("play") + "   " + gameUI::stageLabel(gameUI::unlocked()), 360, 237, 412,
                   92, [] { gameUI::go(GameScene::createStage(gameUI::unlocked())); }, true);
    gameUI::button(this, gameUI::tr("map"), 248, 141, 211, 65,
                   [this] { map((gameUI::unlocked() - 1) / 6); });
    gameUI::button(this, gameUI::tr("settings"), 473, 141, 211, 65, [this] { settings(); });
}
void MainMenuScene::settings() {
    removeAllChildren();
    current = -2;
    gameUI::back(this, "bg_2.jpg");
    gameUI::panel(this, 68, 332, 584, 640, blue, 38);
    gameUI::label(this, gameUI::tr("settings"), 52, 360, 894);
    gameUI::button(this, gameUI::tr(gameUI::sound() ? "soundOn" : "soundOff"), 360, 783, 388, 74,
                   [this] {
                       gameUI::toggleSound();
                       settings();
                   });
    gameUI::label(this, gameUI::tr("language"), 26, 360, 680, frost);
    const char *ids[] = {"en", "zh-Hans", "zh-Hant", "ja"};
    const char *names[] = {"English", "ZH (CN)", "ZH (TW)", "JA"};
    for (int i = 0; i < 4; ++i) {
        std::string id = ids[i];
        gameUI::button(this, names[i], 248 + (i % 2) * 224, 604 - (i / 2) * 82, 196, 65,
                       [this, id] {
                           gameUI::language(id);
                           settings();
                       },
                       id == gameUI::lang());
    }
    gameUI::button(this, gameUI::tr("back"), 360, 416, 270, 72, [this] { title(); });
}
void MainMenuScene::map(int page) {
    removeAllChildren();
    int count = gameUI::levelCount(), pages = (count + 5) / 6;
    current = std::max(0, std::min(page, pages - 1));
    gameUI::back(this, StringUtils::format("map_background_%d.jpg", current % 3 + 1));
    gameUI::panel(this, 28, 1100, 664, 146, blue, 32);
    gameUI::label(this, gameUI::tr("map"), 25, 360, 1204, frost);
    gameUI::label(this, gameUI::tr(StringUtils::format("region%d", current % 3 + 1)), 46, 360, 1146);
    const float path[3][6] = {
        {383, 526, 355, 289, 435, 346}, {365, 495, 428, 294, 398, 477}, {485, 386, 424, 342, 329, 291}};
    for (int i = 0; i < 6 && current * 6 + i < count; ++i) {
        int stage = current * 6 + i + 1;
        bool open = stage <= gameUI::unlocked();
        float x = path[current % 3][i], y = 225 + i * 150;
        auto tile = MenuItem::create([stage](Ref *) {
            gameUI::effect("Click-Button");
            gameUI::go(GameScene::createStage(stage));
        });
        tile->setContentSize({125, 104});
        tile->setAnchorPoint({.5f, .5f});
        gameUI::picture(tile, open ? "level_icon_0.png" : "level_icon_0_locked.png", 62, 48, 118);
        auto num = gameUI::label(tile, gameUI::stageLabel(stage), 36, 62, 58);
        num->enableOutline(Color4B(30, 54, 95, 255), 3);
        int stars = gameUI::best(stage, "stars");
        for (int j = 0; j < stars; ++j)
            gameUI::picture(tile, "Icon_star_gold.png", 62 + (j - (stars - 1) * .5f) * 23, 96, 28);
        tile->setEnabled(open);
        auto menu = Menu::create(tile, nullptr);
        menu->setPosition(x, y);
        addChild(menu);
    }
    gameUI::panel(this, 35, 40, 650, 94, blue, 35);
    gameUI::button(this, gameUI::tr("home"), 152, 88, 176, 62, [this] { title(); });
    gameUI::label(this, std::to_string(current + 1) + " / " + std::to_string(pages), 30, 365, 88);
    if (current > 0)
        gameUI::button(this, "<", 504, 88, 80, 62, [this] { map(current - 1); });
    if (current + 1 < pages)
        gameUI::button(this, ">", 600, 88, 80, 62, [this] { map(current + 1); });
}
