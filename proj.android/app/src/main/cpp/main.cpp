#include "AppDelegate.h"
#include "cocos2d.h"
USING_NS_CC;
void cocos_android_app_init(JNIEnv* env) {
    CCLOG("Ice Adventure Quest reconstruction: native startup");
    auto app = new AppDelegate();
}

extern "C" JNIEXPORT void JNICALL Java_com_iceadventurequest_game_GameActivity_scrollWheel(
    JNIEnv *, jclass, jfloat x, jfloat y, jfloat ticks) {
    auto director = Director::getInstance();
    auto view = director->getOpenGLView();
    if (!view || view->getScaleX() <= 0 || view->getScaleY() <= 0) return;
    const auto frame = view->getFrameSize();
    const auto viewport = view->getViewPortRect();
    EventMouse event(EventMouse::MouseEventType::MOUSE_SCROLL);
    event.setCursorPosition((x * frame.width - viewport.origin.x) / view->getScaleX(),
                            (y * frame.height - viewport.origin.y) / view->getScaleY());
    event.setScrollData(0, -ticks);
    director->getEventDispatcher()->dispatchEvent(&event);
}
