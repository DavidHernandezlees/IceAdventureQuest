#pragma once
#include "cocos2d.h"
#include "ui/UIScrollView.h"
#include <map>

enum class MapPage;
class MapFeatures;

class MainMenuScene : public cocos2d::Scene {
  public:
    CREATE_FUNC(MainMenuScene);
    static MainMenuScene *world(int stage);
    bool init() override;

  private:
    bool initialize(int stage);
    void title();
    void map(int stage);
    void settings();
    void feature(MapPage page);
    void refreshWallet(float);
    MapFeatures *features = nullptr;
    cocos2d::Node *overlay = nullptr;
    void closeSettings();
    void updateMap();
    void populateCycle(cocos2d::Node *layout, int cycle);
    cocos2d::ui::ScrollView *scroll = nullptr;
    cocos2d::LayerColor *options = nullptr;
    std::map<int, int> stagesBySlot;
    std::vector<cocos2d::Vec2> stops;
    float cycleHeight = 0;
    int lastSlot = 0;
    int unlocked = 1;
    bool dragging = false;
};
