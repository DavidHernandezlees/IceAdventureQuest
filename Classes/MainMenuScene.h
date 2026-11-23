#pragma once
#include "cocos2d.h"
#include "ui/UIScrollView.h"
#include <map>

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
