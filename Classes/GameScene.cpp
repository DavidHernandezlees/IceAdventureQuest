#include "GameScene.h"
#include "Interface.h"
#include "MainMenuScene.h"
#include "editor-support/cocostudio/ActionTimeline/CSLoader.h"
#include "editor-support/cocostudio/ActionTimeline/CCActionTimeline.h"
#include "ui/CocosGUI.h"
#include <algorithm>
#include <chrono>
#include <cmath>
USING_NS_CC;
using ice::Power;
namespace {
std::string crushFile(const ice::Piece &piece) {
    if (piece.power == Power::Bomb) return "effect/explode_bomb_new.csb";
    if (piece.power == Power::Rainbow) return "effect/explode_rainbow.csb";
    if (piece.blocker() || piece.color < 1 || piece.color > 6) return "effect/explode_simple.csb";
    const char *pattern = "effect/explode_simple_%d.csb";
    if (piece.power == Power::Plane) pattern = "effect/explode_plane_%d.csb";
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
    auto layout = CSLoader::createNode("dialog/GameScene.csb");
    addChild(layout);
    auto background = static_cast<Sprite *>(layout->getChildByName("bg"));
    background->setTexture(gameUI::boardBackground(n));
    layout->getChildByName("btn_skip")->setVisible(false);
    layout->getChildByName("lbl_coin")->setVisible(false);
    hud = layout->getChildByName("top_bar");
    controls = layout->getChildByName("bottom_bar");
    controls->setPositionY(22);
    for (int i = 1; i <= 4; ++i)
        controls->getChildByName(StringUtils::format("booster%d", i))->setVisible(false);
    auto pause = static_cast<ui::Button *>(controls->getChildByName("btn_setting"));
    pause->addClickEventListener([this](Ref *) { if (!busy && !modal) dialog(false); });

    firstColumn = game->width;
    int right = 0, top = game->height;
    for (int i = 0; i < int(game->pieces.size()); ++i) {
        if (!game->playable(i)) continue;
        firstColumn = std::min(firstColumn, i % game->width);
        right = std::max(right, i % game->width);
        top = std::min(top, i / game->width);
        lastRow = std::max(lastRow, i / game->width);
    }
    visibleColumns = right - firstColumn + 1;
    visibleRows = lastRow - top + 1;
    // MatchGameLayer scales its 75-pixel cells by the larger active dimension.
    const float scale[] = {0, 1.5f, 1.5f, 1.5f, 1.4f, 1.4f, 1.25f, 1.1f, 1.05f, 1.03f, 1.01f};
    cell = 75 * scale[std::max(visibleColumns, visibleRows)];
    board = Node::create();
    board->setPosition(360 - cell * visibleColumns / 2, 646 - cell * visibleRows / 2);
    addChild(board);
    floor = Node::create();
    board->addChild(floor);
    gems = Node::create();
    board->addChild(gems);
    effects = Node::create();
    board->addChild(effects, 1);
    drawPieces();
    refresh();
    auto touch = EventListenerTouchOneByOne::create();
    touch->onTouchBegan = [this](Touch *t, Event *) {
        if (busy || modal)
            return false;
        idle = 0;
        touchStart = t->getLocation();
        if (guide && game->tutorial() && !game->tutorial()->guided) {
            game->dismissTutorial();
            showTutorial();
            return false;
        }
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
            if (guide) guide->setVisible(true);
        }
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(key, this);
    scheduleUpdate();
    busy = true;
    cascade(0);
    return true;
}
void GameScene::update(float dt) {
    if (modal || game->moves == 0) return;
    if (!busy && !guide && selected < 0) {
        idle += dt;
        if (idle >= 5.f) { idle = 0; hint(); }
    }
    if (!game->timed() || guide) return;
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
    case Power::Bomb: file=StringUtils::format("block_bomb_%d.png",p.color-1); break;
    case Power::Plane: file=StringUtils::format("plane%d.png",p.color); break;
    default: file=StringUtils::format("block_%d.png",p.color-1); break;
    }
    gameUI::picture(n, file, 0, 0, cell * .97f);
    if (p.power == Power::Column || p.power == Power::Row) {
        auto stripe = gameUI::picture(n, "Line.png", 0, 0, cell*.57f);
        if (stripe && p.power == Power::Column) stripe->setRotation(90);
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
        gameUI::picture(floor, alternate ? "images/plist/tex_map/map_cell_1.png"
                                         : "images/plist/tex_map/map_cell_2.png",
                        pos.x, pos.y, cell, cell);
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
namespace {
void caption(Node *parent, const std::string &name, const std::string &value) {
    auto widget = parent->getChildByName(name);
    if (auto text = dynamic_cast<ui::Text *>(widget)) {
        if (gameUI::lang() != "en" && text->getFontName() != gameUI::font()) text->setFontName(gameUI::font());
        text->setString(value);
    }
    else if (auto number = dynamic_cast<ui::TextBMFont *>(widget)) number->setString(value);
}

void drawTargets(Node *box, const std::vector<icequest::Target> &goals) {
    static const std::map<int, std::string> icons = {
        {7,"Mission_Line.png"},{8,"Mission_Bomb.png"},{9,"rainbow_big.png"},
        {10,"pot-normal.png"},{11,"drop_1.png"},{12,"drop_2.png"},{13,"box_1.png"},
        {14,"box_1-color_0.png"},{17,"nachimpan.png"},{23,"bear_1.png"},
        {30,"ground_1.png"},{31,"mbox_3.png"},{32,"jelly_1.png"},{33,"prison_1.png"},{34,"ice_1.png"}
    };
    for (int i = 0; i < 4; ++i) {
        auto slot = box->getChildByName(StringUtils::format("Target%d", i + 1));
        slot->setVisible(i < static_cast<int>(goals.size()));
        if (!slot->isVisible()) continue;
        const auto &g = goals[i];
        auto placeholder = slot->getChildByName("centerCandy");
        placeholder->setVisible(false);
        if (!slot->getChildByName("goalIcon")) {
            auto file = g.id <= 6 ? StringUtils::format("mission-block_%d.png",g.id-1) : icons.at(g.id);
            auto pos = placeholder->getPosition();
            if (auto image = gameUI::picture(slot, file, pos.x, pos.y, 58)) image->setName("goalIcon");
        }
        caption(slot, "lbl_count", std::to_string(g.remaining));
        slot->getChildByName("lbl_count")->setVisible(g.remaining > 0);
        slot->getChildByName("mark_done")->setVisible(g.remaining == 0);
        slot->getChildByName("mark_fail")->setVisible(false);
    }
}
}
void GameScene::refresh() {
    caption(hud, "lbl_move", std::to_string(game->moves));
    caption(hud, "lbl_score", std::to_string(game->score));
    caption(hud, "lbl_level", gameUI::tr("level") + " " + gameUI::stageLabel(stage));
    caption(hud, "Text_3", gameUI::tr("score"));
    const auto &level = gameUI::levelAt(stage);
    for (int i = 1; i <= 3; ++i) {
        auto gray = hud->getChildByName(StringUtils::format("star_undone%d", i));
        auto goldStar = hud->getChildByName(StringUtils::format("star_done%d", i));
        goldStar->setPosition(gray->getPosition());
        goldStar->setVisible(game->score >= level.originalStars[i - 1]);
    }
    drawTargets(hud->getChildByName("TargetNodeBox"), game->objectives);
}
void GameScene::showTutorial() {
    if (guide) { guide->removeFromParent(); guide = nullptr; }
    const auto *step = game->tutorial();
    if (!step) return;
    guide = CSLoader::createNode("dialog/TutorialDlg.csb");
    addChild(guide, 3);
    auto panel = guide->getChildByName("label_bg");
    auto skip = static_cast<ui::Button *>(panel->getChildByName("btn_skip"));
    skip->addClickEventListener([this](Ref *) { game->dismissTutorial(); showTutorial(); });
    panel->getChildByName("tutor_icon_1")->setVisible(true);
    caption(panel, "label", gameUI::tr(step->text));
    caption(skip, "Text_3", gameUI::tr("skip"));
    auto hand = step->hand;
    if (!game->legal(hand.first, hand.second)) return;
    Vec2 point = board->convertToWorldSpace(center(hand.first));
    if (point.y > 640) panel->setPosition(guide->getChildByName("anchor_bottom")->getPosition());
    auto pointer = CSLoader::createNode("dialog/TutorialHand.csb");
    guide->addChild(pointer);
    pointer->setPosition(guide->convertToNodeSpace(point));
    auto motion = CSLoader::createTimeline("dialog/TutorialHand.csb");
    pointer->runAction(motion);
    int delta = hand.second - hand.first;
    motion->play(delta == -1 ? "left" : delta == 1 ? "right" : delta > 0 ? "down" : "up", true);
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
    bool valid = game->playerSwap(a, b);
    if (valid && guide) { guide->removeFromParent(); guide = nullptr; }
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
    anchor->setScale(cell * .97f / 70.f); // Original gem sprites are 70 design pixels.
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
    for (const auto &crush : wave.crushed)
        playCrushEffect(crush);
    for (const auto &flight : wave.flights) {
        auto src = center(flight.first);
        auto crushed = std::find_if(wave.crushed.begin(), wave.crushed.end(),
            [&](const ice::Crush &c) { return c.cell == flight.first; });
        if (crushed == wave.crushed.end()) continue;
        auto plane = gameUI::picture(effects, StringUtils::format("plane%d.png", crushed->piece.color),
                                     src.x, src.y, cell * .9f);
        if (plane) plane->runAction(Sequence::create(MoveTo::create(.28f, center(flight.second)),
                                                    RemoveSelf::create(), nullptr));
    }
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
        gameUI::effect("Pop-Shuffling");
    }
    busy = false;
    idle = 0;
    showTutorial();
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
}
void GameScene::dialog(bool finished) {
    finishedDialog = finished;
    controls->setVisible(false);
    if (guide) guide->setVisible(false);
    modal = LayerColor::create(Color4B(7, 22, 46, 180), 720, 1280);
    addChild(modal, 10);
    bool win = finished && game->won();
    const std::string file = !finished ? "PauseDlg" : win ? "GameWinDlg" : "GameLoseDlg";
    auto layout = CSLoader::createNode("dialog/" + file + ".csb");
    modal->addChild(layout);
    auto bind = [layout](const char *name, const std::function<void()> &action) {
        auto button = static_cast<ui::Button *>(layout->getChildByName(name));
        button->addClickEventListener([action, button](Ref *) {
            button->setTouchEnabled(false);
            gameUI::effect("Click-Button");
            action();
        });
    };
    auto map = [this] { gameUI::go(MainMenuScene::world(stage)); };
    if (!finished) {
        auto resume = [this] {
            modal->removeFromParent();
            modal = nullptr;
            controls->setVisible(true);
            if (guide) guide->setVisible(true);
        };
        bind("btnContinue", resume);
        bind("btn_close", resume);
        bind("btnReturn", map);
        caption(layout, "Text_21", gameUI::tr("paused"));
        caption(layout->getChildByName("btnReturn"), "Text_1", gameUI::tr("map"));
        caption(layout->getChildByName("btnContinue"), "Text_1_0", gameUI::tr("resume"));
        auto music = static_cast<ui::CheckBox *>(layout->getChildByName("chk_music"));
        auto effects = static_cast<ui::CheckBox *>(layout->getChildByName("chk_effect"));
        music->setSelected(gameUI::musicEnabled());
        effects->setSelected(gameUI::effectsEnabled());
        auto updateAudio = [music, effects](Ref *, ui::CheckBox::EventType) {
            gameUI::setAudio(music->isSelected(), effects->isSelected());
        };
        music->addEventListener(updateAudio);
        effects->addEventListener(updateAudio);
        return;
    }
    gameUI::effect(win ? "Pop-Complete" : "Pop-Failed");
    bind("btn_close", map);
    caption(layout, "lbl_level", gameUI::tr("level") + " " + gameUI::stageLabel(stage));
    if (!win) {
        bind("btnReplay", [this] { gameUI::go(createStage(stage)); });
        caption(layout->getChildByName("btnReplay"), "Text_2", gameUI::tr("retry"));
        drawTargets(layout->getChildByName("TargetNodeBox"), game->objectives);
        return;
    }
    gameUI::saveWin(stage, game->stars(), game->finishScore());
    for (auto child : layout->getChildren()) {
        const auto &name = child->getName();
        if (name.compare(0, 4, "coin") == 0 || name == "btnDouble" || name == "puzzleNode")
            child->setVisible(false);
    }
    caption(layout, "lbl_score", std::to_string(game->finishScore()));
    caption(layout, "Text_3", gameUI::tr("score"));
    caption(layout->getChildByName("btn_continue"), "Text_2",
            gameUI::tr(stage == gameUI::levelCount() ? "finish" : "next"));
    for (int i = 1; i <= 3; ++i)
        layout->getChildByName(StringUtils::format("star%d", i))->setVisible(i <= game->stars());
    bind("btn_continue", [this] {
        if (stage == gameUI::levelCount()) gameUI::go(MainMenuScene::world(stage));
        else gameUI::go(createStage(stage + 1));
    });
}
