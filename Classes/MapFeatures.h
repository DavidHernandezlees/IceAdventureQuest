#pragma once
#include "cocos2d.h"

namespace spine { class SkeletonAnimation; }
enum class MapPage { Daily, Video, Shop, Coins, Supplies, Pets };

class MapFeatures : public cocos2d::LayerColor {
public:
    static MapFeatures *open(MapPage page, int stage, std::function<void()> close);
    void back();
private:
    void show(MapPage next);
    void refresh(float);
    void pets();
    void petAction(int action);
    void playPet(const char *clip);
    void selectActivity(int next);
    void updatePet();
    void supplies();
    void purchase(int index);
    void note(const std::string &text);
    cocos2d::Node *layout = nullptr;
    cocos2d::Label *notice = nullptr;
    spine::SkeletonAnimation *penguin = nullptr;
    cocos2d::Texture2D *baseHat = nullptr;
    cocos2d::RefPtr<cocos2d::Texture2D> wornHat;
    MapPage page = MapPage::Daily;
    int stage = 1, activity = 0;
    bool fromPet = false, coinsFromShop = false;
    std::string feedback;
    std::function<void()> close;
};
