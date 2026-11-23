#include "RewardStore.h"
#include "cocos2d.h"
#include <ctime>
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
#include "platform/android/jni/JniHelper.h"
#endif

namespace market {
namespace {
struct SavedWallet {
    ice::Rewards value;
    unsigned revision = 0;
    bool readable = true;
    std::string path(unsigned slot) const {
        return cocos2d::FileUtils::getInstance()->getWritablePath()+"quest-wallet-"+std::to_string(slot%2)+".json";
    }
    SavedWallet() {
        auto files = cocos2d::FileUtils::getInstance();
        bool found = false;
        for (unsigned slot = 0; slot < 2; ++slot) {
            if (!files->isFileExist(path(slot))) continue;
            found = true;
            ice::Rewards candidate;
            unsigned number = 0;
            if (candidate.decode(files->getStringFromFile(path(slot)),number) && number > revision) {
                revision = number;
                value = std::move(candidate);
            }
        }
        readable = !found || revision != 0;
    }
};
SavedWallet &saved() { static SavedWallet storage; return storage; }
const char *message = "";
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
const char *activity = "com.iceadventurequest.game.GameActivity";
#endif
}
const ice::Rewards &wallet() { return saved().value; }
bool change(const std::function<bool(ice::Rewards &)> &operation) {
    auto &s = saved();
    auto next = s.value;
    if (!s.readable || !operation(next)) return false;
    unsigned revision = s.revision+1;
    // Write the inactive slot. A crash leaves the preceding revision intact.
    auto files=cocos2d::FileUtils::getInstance();
    auto data=next.encode(revision);
    auto file=s.path(revision);
    if (!files->writeStringToFile(data,file) || files->getStringFromFile(file)!=data) return false;
    s.value=std::move(next); s.revision=revision;
    return true;
}
const char *status() { return message; }
void refreshStore() {
    message="";
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
    cocos2d::JniHelper::callStaticVoidMethod(activity,"refreshStore");
#endif
}
std::string price(int product) {
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
    return cocos2d::JniHelper::callStaticStringMethod(activity,"price",product);
#else
    return {};
#endif
}
bool buyProduct(int product) {
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
    return cocos2d::JniHelper::callStaticBooleanMethod(activity,"buy",product);
#else
    return false;
#endif
}
bool watch() {
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
    return cocos2d::JniHelper::callStaticBooleanMethod(activity,"watch");
#else
    return false;
#endif
}
void leaveVideo() {
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
    cocos2d::JniHelper::callStaticVoidMethod(activity,"leaveVideo");
#endif
}
bool privacyRequired() {
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
    return cocos2d::JniHelper::callStaticBooleanMethod(activity,"privacyRequired");
#else
    return false;
#endif
}
void privacyOptions() {
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
    cocos2d::JniHelper::callStaticVoidMethod(activity,"privacyOptions");
#endif
}
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
extern "C" JNIEXPORT void JNICALL Java_com_iceadventurequest_game_GameActivity_serviceStatus(
    JNIEnv *, jclass, jint code) {
    const char *keys[]={"","serviceBusy","serviceUnavailable","cancelled","received","pending","saveError"};
    if (code>=0 && code<7) message=keys[code];
}
extern "C" JNIEXPORT jboolean JNICALL Java_com_iceadventurequest_game_GameActivity_deliverPurchase(
    JNIEnv *env,jclass,jint index,jstring token,jint quantity) {
    if (!token) return false;
    const char *raw=env->GetStringUTFChars(token,nullptr);
    if (!raw) return false;
    std::string id(raw);
    env->ReleaseStringUTFChars(token,raw);
    if (wallet().receipts.count(id)) return true;
    return change([&](ice::Rewards &r){return r.purchase(index,id,quantity);});
}
extern "C" JNIEXPORT jboolean JNICALL Java_com_iceadventurequest_game_GameActivity_deliverVideo(
    JNIEnv *,jclass,jlong when) {
    if (when<=wallet().videoTime) return true;
    return change([&](ice::Rewards &r){return r.video(when);});
}
#endif
}
