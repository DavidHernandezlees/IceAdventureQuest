#pragma once
#include "cocos2d.h"
#include "ui/UIScrollView.h"
#include <map>

enum class MapPage;
class MapFeatures;

class MainMenuScene : public cocos2d::Scene {
  public:
    CREATE_FUNC(MainMenuScene);
    static MainMenuScene *world(int stage, int previousStage = 0);
    bool init() override;

  private:
    bool initialize(int stage, int previousStage = 0);
    void title();
    void map(int stage, int previousStage = 0);
    cocos2d::Vec2 positionFor(int slot) const;
    void centerOn(int slot);
    void showChapters();
    void showStarProgress();
    void updateStarBar(cocos2d::Node *bar);
    void openOverlay();
    void settings();
    void feature(MapPage page);
    void refreshWallet(float);
    MapFeatures *features = nullptr;
    cocos2d::Node *overlay = nullptr;
    void closeSettings();
    void updateMap();
    void populateCycle(cocos2d::Node *layout, int cycle);
    cocos2d::Node *cycles = nullptr;
    cocos2d::RefPtr<cocos2d::Node> sceneryData;
    std::map<int,int> chapterStars;
    std::vector<int> stageStars;
    int totalStars = 0, frontier = 1;
    cocos2d::ui::ScrollView *scroll = nullptr;
    cocos2d::LayerColor *options = nullptr;
    std::map<int, int> stagesBySlot;
    std::vector<cocos2d::Vec2> stops;
    float cycleHeight = 0;
    int lastSlot = 0;
    int unlocked = 1;
    bool dragging = false;
};
