#include "AppDelegate.h"
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    AppDelegate application;
    return cocos2d::Application::getInstance()->run();
}
