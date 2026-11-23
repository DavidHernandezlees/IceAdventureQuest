#pragma once
#include "Campaign.h"
#include "cocos2d.h"
namespace gameUI {
int levelCount();
const ice::Level &levelAt(int stage);
std::string stageLabel(int stage);
bool originalCampaign();
void selectCampaign(bool original);
std::string tr(const std::string &key);
std::string lang();
void language(const std::string &id);
cocos2d::Label *label(cocos2d::Node *parent, const std::string &value, float size, float x, float y,
                      cocos2d::Color3B color = cocos2d::Color3B::WHITE);
cocos2d::Sprite *picture(cocos2d::Node *parent, const std::string &file, float x, float y, float w,
                         float h = 0);
void back(cocos2d::Node *parent, const std::string &file);
cocos2d::DrawNode *panel(cocos2d::Node *parent, float x, float y, float w, float h,
                         cocos2d::Color4F fill, float radius = 24);
cocos2d::MenuItem *button(cocos2d::Node *parent, const std::string &text, float x, float y, float w,
                          float h, const std::function<void()> &action, bool primary = false);
void music(bool map);
void effect(const std::string &file);
bool sound();
void toggleSound();
int unlocked();
int best(int stage, const char *field);
void saveWin(int stage, int stars, int score);
void go(cocos2d::Scene *scene);
} // namespace gameUI
