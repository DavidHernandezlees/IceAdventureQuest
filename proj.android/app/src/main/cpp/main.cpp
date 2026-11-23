#include "AppDelegate.h"
#include "cocos2d.h"
USING_NS_CC;
void cocos_android_app_init(JNIEnv* env) {
    CCLOG("Ice Adventure Quest reconstruction: native startup");
    auto app = new AppDelegate();
}
