#pragma once

#include "cocos2d.h"
#include "ui/UILoadingBar.h"
#include <string>
#include <vector>

class LoadingScene final : public cocos2d::Scene {
public:
    CREATE_FUNC(LoadingScene);
    bool init() override;
    void update(float dt) override;
    void onExit() override;

private:
    void loadNextTexture();
    void showRetry();

    cocos2d::ui::LoadingBar* bar = nullptr;
    std::vector<std::string> textures;
    std::size_t completed = 0;
    float displayedPercent = 0.0f;
    bool waiting = false;
    bool failed = false;
};
