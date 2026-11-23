#pragma once
#include "LevelRules.hpp"
#include <array>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace ice {
enum class Power { Plain, Row, Column, Bomb, Rainbow, Jar, Box, Bear, Drop, ColorBox, Compass, Stone, Plane };
struct CellSetup {
    int initial = 0, color = 0, variant = 0, bottom = 0, hidden = 0;
    bool exit = false, passThrough = false;
    int frost = 0, cage = 0, portalIn = 0, portalOut = 0, upstreamPortal = -1;
};
struct Piece {
    int color = 0;
    Power power = Power::Plain;
    int strength = 1;
    Piece() = default;
    Piece(int c, Power p = Power::Plain, int hp = 1) : color(c), power(p), strength(hp) {}
    bool blocker() const { return power == Power::Jar || fixed(); }
    bool special() const {
        return power == Power::Row || power == Power::Column || power == Power::Bomb || power == Power::Rainbow || power == Power::Plane;
    }
    bool fixed() const {
        return power == Power::Box || power == Power::Bear || power == Power::ColorBox ||
               power == Power::Compass || power == Power::Stone;
    }
    bool occupied() const { return color != 0 || power != Power::Plain; }
};
using Swap = std::pair<int, int>;
struct Tutorial {
    std::string text;
    Swap hand{-1, -1};
    std::vector<int> cells;
    bool guided = false;
};
struct Level {
    int moves = 0;
    icequest::LimitMode mode = icequest::LimitMode::Moves;
    std::vector<CellSetup> originalCells;
    std::vector<icequest::Target> objectives;
    std::array<int, 3> originalStars{};
    std::vector<Tutorial> tutorials;
    bool original() const { return !originalCells.empty(); }
    std::array<int, 6> weights{}, collect{};
    std::array<int, 2> stars{};
    std::vector<std::string> board, gems;
};
struct Crush {
    int cell;
    Piece piece; // The removed piece, before gravity or special creation replaces it.
};
struct Wave {
    std::vector<Crush> crushed;
    std::vector<Swap> flights; // Plane launch and landing cells before gravity.
    std::vector<int> hit;
    std::vector<std::pair<int, int>> falls; // source, destination; negative source is a new gem
    bool fired = false;
    bool cracked = false;
};
class Puzzle {
  public:
    Puzzle(const Level &level, unsigned seed);
    int width, height, moves, score = 0, iceLeft = 0;
    std::vector<Piece> pieces;
    std::vector<int> ice; // -1 means outside the playable board
    std::array<int, 6> remaining;
    std::vector<icequest::Target> objectives;
    std::vector<int> bottom, hidden, frost, cage;
    bool locked(int i) const { return !frost.empty() && (frost[i] || cage[i]); }
    bool timed() const { return layout.mode == icequest::LimitMode::Time; }
    void tick(int seconds);
    icequest::Outcome outcome() const;
    Swap hint() const;
    const Tutorial *tutorial() const;
    void dismissTutorial();
    bool playerSwap(int from, int to);
    bool playable(int i) const;
    bool adjacent(int a, int b) const;
    bool legal(int a, int b) const;
    std::vector<Swap> choices() const;
    bool swap(int a, int b);
    bool canTargetBooster(int item, int index) const;
    bool targetBooster(int item, int index);
    Wave resolve(int depth); // one match wave; call until hit is empty
    bool ensureMove();       // true if the board had to be shuffled
    bool won() const;
    int stars() const;
    int finishScore() const;

  private:
    const Level layout;
    std::mt19937 rng;
    Swap last{-1, -1};
    int colorAt(int r, int c) const;
    int randomColor();
    int stableColor(int i);
    void setOriginalOpening();
    int planeTarget(const std::vector<bool> &marked);
    void damageCell(int i, Wave &wave);
    void revealGround();
    void collectDrops(Wave &wave);
    bool resolving = false;
    int boosterTarget = -1;
    size_t tutorialStep = 0;
    int matchList = 0, dropSlots = 1;
    std::vector<int> pendingDrops;
    Piece refill(int index);
    void originalGravity(Wave &wave);
    bool ordinaryMove(int a, int b) const;
    struct Group {
        std::vector<int> cells;
        bool across = false, down = false;
        int longest = 0;
    };
    std::vector<Group> groups() const;
    void seedStable();
    void collapse(Wave &wave);
};
} // namespace ice
