#pragma once
#include "cocos2d.h"
class MainMenuScene : public cocos2d::Scene {
  public:
    CREATE_FUNC(MainMenuScene);
    static MainMenuScene *world(int page);
    bool init() override;

  private:
    void title();
    void map(int page);
    void settings();
    int current = -1;
};
