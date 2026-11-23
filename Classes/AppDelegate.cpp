#include "AppDelegate.h"
#include "MainMenuScene.h"
#include "RewardStore.h"
#include "LoadingScene.h"
#include "audio/include/AudioEngine.h"
USING_NS_CC;
using Audio = cocos2d::experimental::AudioEngine;
AppDelegate::AppDelegate() {}
AppDelegate::~AppDelegate() {
    Audio::end();
}
void AppDelegate::initGLContextAttrs() {
    GLContextAttrs attrs = {8, 8, 8, 8, 24, 8};
    GLView::setGLContextAttrs(attrs);
}
bool AppDelegate::applicationDidFinishLaunching() {
    auto d = Director::getInstance();
    auto view = d->getOpenGLView();
    if (!view) {
#if CC_TARGET_PLATFORM == CC_PLATFORM_WIN32
        view = GLViewImpl::createWithRect("Ice Adventure Quest", Rect(0, 0, 396, 704));
#else
        view = GLViewImpl::create("Ice Adventure Quest");
#endif
        d->setOpenGLView(view);
    }
    view->setDesignResolutionSize(720, 1280, ResolutionPolicy::SHOW_ALL);
    d->setAnimationInterval(1.0 / 60.0);
    d->setDisplayStats(false);
    Scene* first = LoadingScene::create();
    if (!first) first = MainMenuScene::create();
    d->runWithScene(first);
    market::refreshStore();
    return true;
}
void AppDelegate::applicationDidEnterBackground() {
    Director::getInstance()->stopAnimation();
    Audio::pauseAll();
}
void AppDelegate::applicationWillEnterForeground() {
    Director::getInstance()->startAnimation();
    Audio::resumeAll();
}
