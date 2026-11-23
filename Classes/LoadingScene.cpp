#include "LoadingScene.h"
#include "Interface.h"
#include "MainMenuScene.h"
#include "editor-support/cocostudio/ActionTimeline/CSLoader.h"
#include <algorithm>

using namespace cocos2d;

namespace {
// Only the initial scene uses this callback key. It lets onExit cancel callbacks.
const char* const preloadKey = "ice-adventure.boot-textures";
constexpr float fillPerSecond = 100.0f;
}

bool LoadingScene::init() {
    if (!Scene::init()) return false;

    const auto data = FileUtils::getInstance()->getDataFromFile("dialog/SplashScene.csb");
    auto layout = data.isNull() ? nullptr : CSLoader::createNode(data);
    if (layout) {
        bar = dynamic_cast<ui::LoadingBar*>(layout->getChildByName("loadingbar"));
    }
    if (!bar) {
        CCLOG("LoadingScene: missing SplashScene.csb or its loadingbar widget");
        return false;
    }

    addChild(layout);
    bar->setPercent(0.0f);

    // SplashScene already loads the penguin background and the UI atlas.
    // Preload a small set that the selected campaign will use next.
    if (gameUI::originalCampaign()) {
        textures = {
            "images/plist/img_candy.webp", "images/plist/img_target.webp",
            "images/plist/img_atlas2.webp", "images/plist/map.webp",
            "images/background/bg_1.jpg"
        };
    } else {
        textures = {
            "art/loading.jpg", "art/title-logo.png", "art/bg_1.jpg",
            "art/block_0.png", "art/block_1.png", "art/block_2.png",
            "art/block_3.png", "art/block_4.png", "art/block_5.png"
        };
    }
    scheduleUpdate();
    return true;
}

void LoadingScene::update(float dt) {
    // Reaching here on the following frame lets the full bar be drawn once.
    if (displayedPercent >= 100.0f) {
        unscheduleUpdate();
        Director::getInstance()->replaceScene(MainMenuScene::create());
        return;
    }

    if (!waiting && !failed && completed < textures.size()) {
        loadNextTexture();
    }
    const float actualPercent = textures.empty() ? 100.0f : 100.0f * completed / textures.size();
    displayedPercent = std::min(actualPercent, displayedPercent + fillPerSecond * dt);
    bar->setPercent(displayedPercent);
}

void LoadingScene::loadNextTexture() {
    // A cached image can call back immediately, so set waiting before requesting it.
    waiting = true;
    auto cache = Director::getInstance()->getTextureCache();
    cache->addImageAsync(textures[completed], [this](Texture2D* texture) {
        waiting = false;
        if (texture) {
            ++completed;
        } else {
            CCLOG("LoadingScene: could not load %s", textures[completed].c_str());
            failed = true;
            showRetry();
        }
    }, preloadKey);
}

void LoadingScene::showRetry() {
    auto button = MenuItemLabel::create(
        Label::createWithSystemFont("Load failed - tap to retry", "sans-serif", 29),
        [this](Ref* sender) {
            static_cast<MenuItem*>(sender)->setEnabled(false);
            getChildByName("retryMenu")->removeFromParent();
            failed = false; // Next update retries the same texture.
        });
    auto menu = Menu::create(button, nullptr);
    menu->setName("retryMenu");
    menu->setPosition(360.0f, 100.0f);
    addChild(menu, 1);
}

void LoadingScene::onExit() {
    Director::getInstance()->getTextureCache()->unbindImageAsync(preloadKey);
    Scene::onExit();
}
