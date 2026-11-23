#pragma once
#include "Puzzle.h"
#include "cocos2d.h"
#include <memory>
#include <map>
class GameScene : public cocos2d::Scene {
  public:
    static GameScene *createStage(int stage);

  private:
    bool start(int stage);
    void refresh();
    void drawPieces();
    cocos2d::Node *drawPiece(int index);
    void swapTiles(int a, int b);
    void cascade(int depth);
    void playCrushEffect(const ice::Crush &crush);
    void done();
    void dialog(bool finished);
    void hint();
    int cellAt(const cocos2d::Vec2 &world) const;
    cocos2d::Vec2 center(int index) const;
    void select(int index);
    std::unique_ptr<ice::Puzzle> game;
    int stage = 1, selected = -1, touched = -1;
    void update(float dt) override;
    float elapsed = 0;
    float cell = 0;
    int firstColumn = 0, lastRow = 0, visibleColumns = 0, visibleRows = 0;
    bool busy = false, finishedDialog = false;
    cocos2d::Vec2 touchStart;
    cocos2d::Node *effects = nullptr;
    std::map<std::string, cocos2d::Data> effectFiles;
    cocos2d::Node *board = nullptr, *floor = nullptr, *gems = nullptr, *hud = nullptr,
                  *controls = nullptr, *modal = nullptr;
    cocos2d::Label *message = nullptr;
    std::vector<cocos2d::Node *> sprites;
};
