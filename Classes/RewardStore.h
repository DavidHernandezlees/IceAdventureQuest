#pragma once
#include "Rewards.h"
#include <functional>

namespace market {
const ice::Rewards &wallet();
bool change(const std::function<bool(ice::Rewards &)> &operation);
// All wallet changes run on the Cocos game thread.
void refreshStore();
std::string price(int product);
bool buyProduct(int product);
bool watch();
void leaveVideo();
bool privacyRequired();
void privacyOptions();
const char *status();
}
