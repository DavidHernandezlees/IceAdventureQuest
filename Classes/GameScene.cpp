#include "GameScene.h"
#include "Interface.h"
#include "MainMenuScene.h"
#include "editor-support/cocostudio/ActionTimeline/CSLoader.h"
#include "editor-support/cocostudio/ActionTimeline/CCActionTimeline.h"
#include <algorithm>
#include <chrono>
#include <cmath>
USING_NS_CC;
using ice::Power;
namespace {
const Color4F navy(.03f, .12f, .24f, .94f), tileA(.12f, .29f, .44f, .96f), tileB(.15f, .34f, .48f, .96f);
const Color3B pale(166, 230, 250), gold(255, 222, 105);
std::string crushFile(const ice::Piece &piece) {
    if (piece.power == Power::Bomb) return "effect/explode_bomb_new.csb";
    if (piece.power == Power::Rainbow) return "effect/explode_rainbow.csb";
    if (piece.blocker() || piece.color < 1 || piece.color > 6) return "effect/explode_simple.csb";
    const char *pattern = "effect/explode_simple_%d.csb";
    if (piece.power == Power::Row) pattern = "effect/explode_hori_new_%d.csb";
    if (piece.power == Power::Column) pattern = "effect/explode_vert_new_%d.csb";
    return StringUtils::format(pattern, piece.color - 1);
}
} // namespace
GameScene *GameScene::createStage(int n) {
    auto s = new (std::nothrow) GameScene;
    if (s && s->start(n)) {
        s->autorelease();
        return s;
    }
    CC_SAFE_DELETE(s);
    return nullptr;
}
bool GameScene::start(int n) {
    if (!Scene::init())
        return false;
    stage = n;
    auto seed = unsigned(std::chrono::steady_clock::now().time_since_epoch().count());
    game.reset(new ice::Puzzle(gameUI::levelAt(n), seed));
    gameUI::music(false);
    gameUI::back(this, StringUtils::format("bg_%d.jpg", 1 + ((n - 1) / 6) % 3));
    visibleColumns = game->width;
    visibleRows = game->height;
    lastRow = game->height - 1;
    if (gameUI::originalCampaign()) {
        firstColumn = game->width;
        int right = 0, top = game->height;
        lastRow = 0;
        for (int i = 0; i < int(game->pieces.size()); ++i) {
            if (!game->playable(i)) continue;
            firstColumn = std::min(firstColumn,i % game->width);
            right = std::max(right,i % game->width);
            top = std::min(top,i / game->width);
            lastRow = std::max(lastRow,i / game->width);
        }
        visibleColumns = right - firstColumn + 1;
        visibleRows = lastRow - top + 1;
    }
    cell = std::min(660.f / visibleColumns, 750.f / visibleRows);
    float w = cell * visibleColumns, h = cell * visibleRows;
    board = Node::create();
    board->setPosition((720 - w) / 2, 620 - h / 2);
    addChild(board);
    gameUI::panel(board, -8, -8, w + 16, h + 16, navy, 18);
    floor = Node::create();
    board->addChild(floor);
    gems = Node::create();
    board->addChild(gems);
    effects = Node::create();
    board->addChild(effects, 1);
    hud = Node::create();
    addChild(hud);
    controls = Node::create();
    addChild(controls);
    gameUI::button(controls, "II", 641, 1202, 91, 66, [this] {
        if (!busy && !modal)
            dialog(false);
    });
    gameUI::button(controls, gameUI::tr("hint"), 360, 111, 199, 70, [this] {
        if (!busy && !modal)
            hint();
    });
    message = gameUI::label(this, gameUI::tr(gameUI::originalCampaign() || n <= 4 ? "swapTip" : "iceTip"), 26, 360, 187);
    message->enableOutline(Color4B(13, 47, 79, 240), 3);
    drawPieces();
    refresh();
    auto touch = EventListenerTouchOneByOne::create();
    touch->onTouchBegan = [this](Touch *t, Event *) {
        if (busy || modal)
            return false;
        touchStart = t->getLocation();
        touched = cellAt(touchStart);
        return game->playable(touched) && !game->locked(touched) && !game->pieces[touched].fixed();
    };
    touch->onTouchEnded = [this](Touch *t, Event *) {
        if (busy || modal || touched < 0)
            return;
        auto d = t->getLocation() - touchStart;
        if (std::max(std::abs(d.x), std::abs(d.y)) > cell * .28f) {
            int offset = std::abs(d.x) > std::abs(d.y) ? (d.x > 0 ? 1 : -1)
                                                       : (d.y > 0 ? -game->width : game->width);
            int dest = touched + offset;
            if (game->adjacent(touched, dest))
                swapTiles(touched, dest);
            else
                select(-1);
        } else if (cellAt(t->getLocation()) == touched) {
            if (selected >= 0 && game->adjacent(selected, touched))
                swapTiles(selected, touched);
            else
                select(selected == touched ? -1 : touched);
        }
        touched = -1;
    };
    touch->onTouchCancelled = [this](Touch *, Event *) { touched = -1; };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);
    auto key = EventListenerKeyboard::create();
    key->onKeyReleased = [this](EventKeyboard::KeyCode k, Event *) {
        if (k != EventKeyboard::KeyCode::KEY_ESCAPE && k != EventKeyboard::KeyCode::KEY_BACK)
            return;
        if (!busy && !modal)
            dialog(false);
        else if (modal && !finishedDialog) {
            modal->removeFromParent();
            modal = nullptr;
            controls->setVisible(true);
        }
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(key, this);
    if (game->timed()) scheduleUpdate();
    if (gameUI::originalCampaign()) {
        busy=true;
        cascade(0);
    }
    return true;
}
void GameScene::update(float dt) {
    if (modal || game->moves == 0) return;
    elapsed += dt;
    int seconds=static_cast<int>(elapsed);
    if (seconds==0) return;
    elapsed-=seconds;
    game->tick(seconds);
    refresh();
    if (!busy && game->moves == 0) done();
}
Vec2 GameScene::center(int i) const {
    return {(i % game->width - firstColumn + .5f) * cell, (lastRow - i / game->width + .5f) * cell};
}
int GameScene::cellAt(const Vec2 &world) const {
    Vec2 p = board->convertToNodeSpace(world);
    if (p.x < 0 || p.y < 0 || p.x >= visibleColumns * cell || p.y >= visibleRows * cell)
        return -1;
    return (lastRow - int(p.y / cell)) * game->width + firstColumn + int(p.x / cell);
}
Node *GameScene::drawPiece(int i) {
    auto p = game->pieces[i];
    auto n = Node::create();
    n->setPosition(center(i));
    gems->addChild(n);
    std::string file;
    switch (p.power) {
    case Power::ColorBox: file=StringUtils::format("box_1-color_%d.png",p.color-1); break;
    case Power::Compass: file="nachimpan.png"; break;
    case Power::Stone: file="mbox_4.png"; break;
    case Power::Box: file=StringUtils::format("box_%d.png",p.strength); break;
    case Power::Bear: file=StringUtils::format("bear_%d.png",p.strength); break;
    case Power::Drop: file=StringUtils::format("drop_%d.png",p.strength); break;
    case Power::Jar: file=p.color ? StringUtils::format("pot-%d.png",p.color-1) : "pot-normal.png"; break;
    case Power::Rainbow: file="rainbow_big.png"; break;
    case Power::Bomb: file=StringUtils::format(gameUI::originalCampaign() ? "block_bomb_%d.png" : "block_%d.png",p.color-1); break;
    default: file=StringUtils::format("block_%d.png",p.color-1); break;
    }
    gameUI::picture(n, file, 0, 0, cell * .78f);
    if (gameUI::originalCampaign() && (p.power == Power::Column || p.power == Power::Row)) {
        auto stripe = gameUI::picture(n, "Line.png", 0, 0, cell*.57f);
        if (stripe && p.power == Power::Column) stripe->setRotation(90);
    } else if (!gameUI::originalCampaign() && (p.power == Power::Bomb || p.power == Power::Column || p.power == Power::Row)) {
        auto mark = DrawNode::create();
        n->addChild(mark);
        const Color4F white(1, 1, 1, 1), dark(.11, .18, .34, .95);
        if (p.power == Power::Bomb) {
            mark->drawSolidCircle({0, 0}, cell * .18f, 0, 20, dark);
            for (int j = 0; j < 4; ++j) {
                float a = j * .785398f;
                Vec2 v(std::cos(a) * cell * .12f, std::sin(a) * cell * .12f);
                mark->drawSegment(-v, v, 2, white);
            }
        } else
            for (int s : {-1, 1}) {
                float z = s * cell * .07f;
                Vec2 a(-cell * .23f, z), b(cell * .23f, z);
                if (p.power == Power::Column) {
                    std::swap(a.x, a.y);
                    std::swap(b.x, b.y);
                }
                mark->drawSegment(a, b, 3.7f, dark);
                mark->drawSegment(a, b, 1.8f, white);
            }
    }
    if (game->locked(i)) {
        std::string cover=game->cage[i] ? StringUtils::format("prison_%d.png",std::min(2,game->cage[i]))
                                        : StringUtils::format("ice_%d.png",game->frost[i]);
        gameUI::picture(n,cover,0,0,cell*.9f);
    }
    return n;
}
void GameScene::drawPieces() {
    floor->removeAllChildren();
    gems->removeAllChildren();
    sprites.assign(game->pieces.size(), nullptr);
    const auto &level = gameUI::levelAt(stage);
    for (int i = 0; i < int(sprites.size()); ++i) {
        if (!game->playable(i))
            continue;
        auto pos = center(i);
        bool alternate = (i / game->width + i % game->width) % 2 != 0;
        if (level.original()) {
            gameUI::picture(floor, alternate ? "images/plist/tex_map/map_cell_1.png"
                                             : "images/plist/tex_map/map_cell_2.png",
                            pos.x,pos.y,cell,cell);
        } else {
            gameUI::panel(floor, pos.x-cell*.48f, pos.y-cell*.48f, cell*.96f, cell*.96f,
                          alternate ? tileA : tileB, cell*.09f);
        }
        if (game->ice[i])
            gameUI::picture(floor, StringUtils::format("ice_%d.png", game->ice[i]), pos.x, pos.y,
                            cell * .96f, cell * .96f);
        if (!game->bottom.empty()) {
            int type=game->bottom[i];
            if (game->hidden[i])
                gameUI::picture(floor,"ground_1.png",pos.x,pos.y,cell*.9f,cell*.9f);
            if (type) {
                std::string file=type==1 ? "mbox_3.png" : type<=4 ? StringUtils::format("jelly_%d.png",type-1)
                                                               : StringUtils::format("ice_cookie_%d.png",type-4);
                gameUI::picture(floor,file,pos.x,pos.y,cell*.95f,cell*.95f);
            }
            const auto &cellInfo=level.originalCells[i];
            if (cellInfo.portalIn || cellInfo.portalOut)
                gameUI::picture(floor,cellInfo.portalIn ? "tile_warp_1.png" : "tile_warp_2.png",
                                pos.x,pos.y,cell*.93f,cell*.93f);
            if (cellInfo.exit)
                gameUI::picture(floor,"map_exit.png",pos.x,pos.y-cell*.37f,cell*.80f,cell*.32f);
        }
        sprites[i] = drawPiece(i);
    }
}
void GameScene::refresh() {
    hud->removeAllChildren();
    bool original = gameUI::originalCampaign();
    if (original) {
        gameUI::picture(hud,"game_bg1.png",360,1112,690,185);
        auto score = gameUI::label(hud,gameUI::tr("score")+"   "+std::to_string(game->score),34,360,1232);
        score->enableOutline(Color4B(20,57,91,255),3);
        gameUI::label(hud,gameUI::tr("level"),24,360,1140,Color3B(23,70,124));
        gameUI::label(hud,gameUI::stageLabel(stage),42,360,1097,Color3B(23,70,124));
    } else {
        gameUI::panel(hud, 26, 1018, 668, 236, navy, 33);
        gameUI::label(hud, gameUI::tr("level")+" "+gameUI::stageLabel(stage), 43, 181, 1204);
        gameUI::label(hud, gameUI::tr("score")+"   "+std::to_string(game->score), 29, 410, 1203, pale);
        auto d = DrawNode::create();
        hud->addChild(d);
        d->drawSegment({56,1160},{664,1160},1,Color4F(.33,.61,.72,.7));
    }
    float goalX = original ? 556.f : 441.f;
    gameUI::label(hud, gameUI::tr(game->timed() ? "time" : "moves"), 25, 130, original ? 1140 : 1125, pale);
    gameUI::label(hud, std::to_string(game->moves), 46, 130, original ? 1090 : 1068,
                  game->moves <= 4 ? gold : Color3B::WHITE);
    gameUI::label(hud, gameUI::tr("goal"), 25, goalX, original ? 1140 : 1125, pale);
    std::vector<std::pair<std::string, int>> goals;
    auto &level = gameUI::levelAt(stage);
    for (int c = 0; c < 6; ++c)
        if (level.collect[c])
            goals.emplace_back(StringUtils::format("block_%d.png", c), game->remaining[c]);
    bool hasIce = std::any_of(level.board.begin(), level.board.end(), [](const std::string &r) {
        return r.find_first_of("12") != std::string::npos;
    });
    if (hasIce)
        goals.emplace_back("ice_1.png", game->iceLeft);
    for (const auto &goal:game->objectives) {
        static const std::map<int,std::string> icons = {
            {7,"Mission_Line.png"},{8,"Mission_Bomb.png"},{9,"rainbow_big.png"},
            {10,"pot-normal.png"},{11,"drop_1.png"},{12,"drop_2.png"},{13,"box_1.png"},
            {14,"box_1-color_0.png"},{17,"nachimpan.png"},{33,"prison_1.png"},{34,"ice_1.png"},{23,"bear_1.png"},{30,"ground_1.png"},{31,"mbox_3.png"},{32,"jelly_1.png"}
        };
        auto img = goal.id<=6 ? StringUtils::format("mission-block_%d.png",goal.id-1) : icons.at(goal.id);
        goals.emplace_back(img,goal.remaining);
    }
    float spacing = std::min(139.f, (original ? 240.f : 424.f) / goals.size());
    for (size_t j = 0; j < goals.size(); ++j) {
        float x = goalX + (int(j) - float(goals.size() - 1) * .5f) * spacing;
        gameUI::picture(hud, goals[j].first, x - (original ? 16.f : 27.f), original ? 1079 : 1069, original ? 33.f : 42.f);
        gameUI::label(hud, std::to_string(goals[j].second), original ? 27.f : 36.f, x + (original ? 17.f : 27.f), original ? 1078 : 1068);
    }
}
void GameScene::select(int i) {
    if (selected >= 0 && sprites[selected])
        sprites[selected]->setScale(1);
    selected = i;
    if (i >= 0) {
        sprites[i]->stopAllActions();
        sprites[i]->setScale(1.16f);
    }
}
void GameScene::swapTiles(int a, int b) {
    select(-1);
    if (game->locked(b) || game->pieces[b].fixed()) return;
    busy = true;
    gameUI::effect("Swap-Block");
    bool valid = game->swap(a, b);
    for (auto pair : {std::make_pair(a, b), std::make_pair(b, a)}) {
        auto n = sprites[pair.first];
        n->stopAllActions();
        n->setScale(1);
        auto move = MoveTo::create(.13f, center(pair.second));
        if (valid)
            n->runAction(move);
        else
            n->runAction(Sequence::create(move, MoveTo::create(.13f, center(pair.first)), nullptr));
    }
    runAction(Sequence::create(DelayTime::create(valid ? .14f : .28f), CallFunc::create([this, valid] {
                                   if (valid) {
                                       drawPieces();
                                       refresh();
                                       cascade(0);
                                   } else {
                                       busy = false;
                                       message->setString(gameUI::tr("noMatch"));
                                       if (game->moves == 0) done();
                                   }
                               }),
                               nullptr));
}
void GameScene::playCrushEffect(const ice::Crush &crush) {
    const auto file = crushFile(crush.piece);
    auto it = effectFiles.find(file);
    if (it == effectFiles.end())
        it = effectFiles.emplace(file, FileUtils::getInstance()->getDataFromFile(file)).first;
    const auto &data = it->second;
    if (data.isNull()) return;
    auto clip = CSLoader::createNode(data);
    auto timeline = CSLoader::createTimeline(data, file);
    if (!clip || !timeline) return;

    // The CSB animates its own child coordinates. Keep board placement independent.
    auto anchor = Node::create();
    anchor->setPosition(center(crush.cell));
    anchor->setScale(cell * .78f / 70.f); // Original gem sprites are 70 design pixels.
    effects->addChild(anchor, crush.piece.special() ? 1 : 0);
    anchor->addChild(clip);
    clip->runAction(timeline);
    timeline->gotoFrameAndPlay(0, false);
    // Defer removal until the timeline has returned from its last-frame callback.
    timeline->setLastFrameCallFunc([anchor] { anchor->runAction(RemoveSelf::create()); });
}

void GameScene::cascade(int depth) {
    auto wave = game->resolve(depth);
    if (wave.hit.empty()) {
        done();
        return;
    }
    gameUI::effect(wave.fired ? "Break-Bomb" : wave.cracked ? "Break-Ice" : "Break-Block");
    message->setString(depth ? gameUI::tr("cascade") + "  x" + std::to_string(depth + 1)
                             : gameUI::tr("matched"));
    for (const auto &crush : wave.crushed)
        playCrushEffect(crush);
    for (int i : wave.hit)
        sprites[i]->runAction(
            Sequence::create(ScaleTo::create(.065f, 1.15f), ScaleTo::create(.11f, .05f), nullptr));
    runAction(Sequence::create(DelayTime::create(.18f), CallFunc::create([this, wave, depth] {
                                   drawPieces();
                                   refresh();
                                   for (auto fall : wave.falls) {
                                       auto n = sprites[fall.second];
                                       auto dest = center(fall.second);
                                       if (fall.first >= 0)
                                           n->setPosition(center(fall.first));
                                       else {
                                           n->setPosition(dest + Vec2(0, cell * .5f));
                                           n->setScale(.2f);
                                           n->runAction(ScaleTo::create(.16f, 1));
                                       }
                                       n->runAction(EaseSineOut::create(MoveTo::create(.20f, dest)));
                                   }
                                   runAction(Sequence::create(
                                       DelayTime::create(.22f),
                                       CallFunc::create([this, depth] { cascade(depth + 1); }),
                                       nullptr));
                               }),
                               nullptr));
}
void GameScene::done() {
    if (game->outcome() != icequest::Outcome::Playing) {
        busy = false;
        dialog(true);
        return;
    }
    if (game->ensureMove()) {
        drawPieces();
        if (game->choices().empty()) {
            busy=false;
            dialog(true);
            return;
        }
        if (gameUI::originalCampaign()) gameUI::effect("Pop-Shuffling");
        message->setString(gameUI::tr("shuffle"));
    }
    busy = false;
}
void GameScene::hint() {
    auto p = game->hint();
    if (p.first < 0)
        return;
    select(-1);
    for (int i : {p.first, p.second}) {
        auto n = sprites[i];
        n->stopAllActions();
        n->setScale(1);
        n->runAction(Repeat::create(
            Sequence::create(ScaleTo::create(.17f, .72f), ScaleTo::create(.17f, 1), nullptr), 3));
    }
    message->setString(gameUI::tr("hintTip"));
}
void GameScene::dialog(bool finished) {
    finishedDialog = finished;
    controls->setVisible(false);
    modal = LayerColor::create(Color4B(7, 22, 46, 180), 720, 1280);
    addChild(modal, 10);
    gameUI::panel(modal, 72, 366, 576, 555, navy, 38);
    bool win = finished && game->won();
    gameUI::label(modal, gameUI::tr(win ? "won" : !finished ? "paused" : game->moves>0 ? "noMoves" : game->timed() ? "timeUp" : "lost"), 48, 360, 833);
    if (finished) {
        if (gameUI::originalCampaign()) gameUI::effect(win ? "Pop-Complete" : "Pop-Failed");
        if (win) {
            gameUI::saveWin(stage, game->stars(), game->finishScore());
            for (int i = 0; i < 3; ++i) {
                auto s = gameUI::picture(modal, "Icon_star_gold.png", 284 + i * 76,
                                         735 + (i == 1 ? 10 : 0), 65);
                if (i >= game->stars() && s)
                    s->setColor(Color3B(65, 73, 101));
            }
        } else
            gameUI::label(modal, gameUI::tr("tryAgain"), 28, 360, 734, pale);
        gameUI::effect(win ? "Pop-Complete" : "Pop-Failed");
        gameUI::label(modal, gameUI::tr("score") + "   " + std::to_string(game->finishScore()), 36, 360,
                      648);
    } else {
        gameUI::label(modal, gameUI::tr("level") + " " + gameUI::stageLabel(stage), 35, 360, 722, pale);
        gameUI::label(modal, gameUI::tr("best") + "   " + std::to_string(gameUI::best(stage, "score")),
                      29, 360, 649);
    }
    bool finalStage = stage == gameUI::levelCount();
    gameUI::button(modal,
                   gameUI::tr(!finished ? "resume" : !win ? "retry" : finalStage ? "finish" : "next"),
                   360, 544, 381, 83,
                   [this, finished, win, finalStage] {
                       if (!finished) {
                           modal->removeFromParent();
                           modal = nullptr;
                           controls->setVisible(true);
                       } else if (win && finalStage)
                           gameUI::go(MainMenuScene::world((stage - 1) / 6));
                       else
                           gameUI::go(createStage(win ? stage + 1 : stage));
                   },
                   true);
    gameUI::button(modal, gameUI::tr("map"), 360, 436, 260, 66,
                   [this] { gameUI::go(MainMenuScene::world((stage - 1) / 6)); });
}
