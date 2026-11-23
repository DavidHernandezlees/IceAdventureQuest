#pragma once
#include <array>
#include <vector>

namespace icequest {
// Runtime fields in CandyKind. The packed tile integers in a level file must be decoded first.
struct Candy {
    int kind;
    int color;
    int variant;
};

struct Target {
    int id;
    int remaining;
    int unreserved;
};

enum class LimitMode { Moves, Time };
enum class Outcome { Playing, Won, Lost };

bool acceptsCandy(int targetId, Candy candy);
bool reserveCandy(Target& target, Candy candy);
int collectCandy(std::vector<Target>& targets, Candy candy); // index credited, or -1
bool goalsComplete(const std::vector<Target>& targets, bool successFlag = false);
int starsForScore(int score, const std::array<int, 3>& thresholds);
int remainingAfterTurn(int remaining, LimitMode mode);
int remainingAfterTick(int remaining, LimitMode mode);

// Gameplay decision only. Callbacks, effects and bonus presentation are handled by the scene.
Outcome evaluateEnd(bool readyToEvaluate, const std::vector<Target>& targets, int remaining,
                    bool failureFlag = false, bool successFlag = false);
}
