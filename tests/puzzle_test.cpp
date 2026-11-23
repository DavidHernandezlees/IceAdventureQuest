#include "Campaign.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
using namespace ice;

static int settle(Puzzle &game) {
    int depth = 0;
    while (!game.resolve(depth).hit.empty()) {
        ++depth;
        assert(depth < 100);
    }
    return depth;
}
static int goalLeft(const Puzzle &game) {
    int count = game.iceLeft;
    for (int n : game.remaining)
        count += n;
    return count;
}
static void invariants(const Puzzle &game) {
    int layers = 0;
    for (size_t i = 0; i < game.ice.size(); ++i) {
        if (game.ice[i] < 0)
            assert(game.pieces[i].color == 0);
        else {
            assert(game.pieces[i].color >= 1 && game.pieces[i].color <= 6);
            layers += game.ice[i];
        }
    }
    assert(layers == game.iceLeft);
}
static void specials() {
    Level level;
    level.board = {".....", "..2..", ".....", ".....", "....."};
    level.gems = {".....", ".....", ".....", ".....", "....."};
    level.moves = 15;
    level.weights = {{1, 1, 1, 1, 1, 1}};
    level.collect[5] = 100;
    auto clean = [&]() {
        Puzzle p(level, 1);
        for (int i = 0; i < 25; ++i)
            p.pieces[i] = Piece(1 + (i % 5 + 2 * (i / 5)) % 6);
        return p;
    };
    auto p = clean();
    p.pieces[1] = Piece(6, Power::Rainbow);
    p.pieces[2] = Piece(5, Power::Rainbow);
    assert(p.swap(1, 2));
    auto all = p.resolve(0);
    assert(all.hit.size() == 25 && all.cracked && p.iceLeft == 1);
    for (int len : {4, 5}) {
        auto g = clean();
        for (int x = 0; x < len; ++x)
            g.pieces[x] = Piece(6);
        g.resolve(0);
        Power expected = len == 4 ? Power::Row : Power::Rainbow;
        assert(
            std::any_of(g.pieces.begin(), g.pieces.end(), [&](Piece q) { return q.power == expected; }));
    }
    auto col = clean();
    for (int y = 0; y < 4; ++y)
        col.pieces[y * 5] = Piece(5);
    col.resolve(0);
    assert(std::any_of(col.pieces.begin(), col.pieces.end(),
                       [](Piece q) { return q.power == Power::Column; }));
    auto cross = clean();
    for (int i : {6, 7, 8, 12, 17})
        cross.pieces[i] = Piece(6);
    cross.resolve(0);
    assert(std::any_of(cross.pieces.begin(), cross.pieces.end(),
                       [](Piece q) { return q.power == Power::Bomb; }));
    auto g = clean();
    g.pieces[0] = Piece(1, Power::Row);
    g.pieces[1] = Piece(2, Power::Column);
    assert(g.swap(0, 1));
    auto blast = g.resolve(0);
    assert(blast.fired && blast.hit.size() >= 9);
    assert(blast.crushed.size() == blast.hit.size());
    assert(std::any_of(blast.crushed.begin(), blast.crushed.end(), [](const Crush &crush) {
        return crush.cell == 1 && crush.piece.power == Power::Row && crush.piece.color == 1;
    }));
    auto dead = clean();
    assert(dead.choices().empty());
    assert(dead.ensureMove() && !dead.choices().empty());
}
int main(int argc, char **argv) {
    assert(argc == 2);
    std::ifstream f(argv[1]);
    assert(f);
    std::string raw{std::istreambuf_iterator<char>(f), {}};
    auto levels = loadCampaign(raw);
    specials();
    int winsTotal = 0, turns = 0, largest = 0, losses = 0;
    for (size_t n = 0; n < levels.size(); ++n) {
        int wins = 0;
        for (unsigned seed = 1; seed <= 6; ++seed) {
            Puzzle game(levels[n], seed);
            assert(game.resolve(0).hit.empty());
            assert(!game.swap(-1, 0) && game.moves == levels[n].moves);
            while (!game.won() && game.moves > 0) {
                auto choices = game.choices();
                assert(!choices.empty());
                int best = -1;
                Swap pick = choices.front();
                for (auto choice : choices) {
                    Puzzle trial(game);
                    assert(trial.swap(choice.first, choice.second));
                    settle(trial);
                    int value = (goalLeft(game) - goalLeft(trial)) * 2000 + trial.score - game.score;
                    if (value > best) {
                        best = value;
                        pick = choice;
                    }
                }
                int before = game.moves;
                assert(game.swap(pick.first, pick.second));
                assert(game.moves == before - 1);
                largest = std::max(largest, settle(game));
                ++turns;
                invariants(game);
                if (!game.won() && game.moves)
                    game.ensureMove();
            }
            if (game.won()) {
                ++wins;
                assert(game.stars() >= 1 && game.stars() <= 3);
            } else {
                assert(game.moves == 0 && game.stars() == 0);
                ++losses;
            }
            assert(!game.swap(0, 1));
        }
        std::cout << "Level " << n + 1 << ": " << wins << "/6 clears\n";
        assert(wins > 0);
        winsTotal += wins;
    }
    std::cout << turns << " moves, " << winsTotal << " clears, " << losses << " defeats, max cascade "
              << largest << "\n";
}
