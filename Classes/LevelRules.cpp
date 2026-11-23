#include "LevelRules.hpp"
#include <algorithm>

namespace icequest {
bool acceptsCandy(int targetId, Candy candy) {
    // ARM64 LevelTarget::isAcceptable, 0x55ddac. Its switch has 42 entries.
    if (targetId < 1 || targetId > 42) return false;
    if (targetId <= 6) return candy.kind == 1 && candy.color == targetId;
    if (targetId == 7) return candy.kind == 2 || candy.kind == 3;
    if (targetId == 11 || targetId == 12)
        return candy.kind == 13 && candy.variant == targetId - 10;

    // Indexed by native target ID. Negative entries belong to cases handled above.
    static const std::array<int,43> kindForTarget = {{
        -1, -1, -1, -1, -1, -1, -1, -1, 4, 11, 6, -1, -1,
        14, 7, 15, 33, 17, 18, 19, 20, 21, 22, 24, 25, 26, 34,
        41, 35, 43, 46, 47, 48, 51, 50, 31, 28, 37, 38, 30, 44, 39, 36
    }};
    return candy.kind == kindForTarget[targetId];
}

bool reserveCandy(Target& target, Candy candy) {
    if (target.unreserved <= 0 || !acceptsCandy(target.id, candy)) return false;
    --target.unreserved;
    return true;
}

int collectCandy(std::vector<Target>& targets, Candy candy) {
    for (std::size_t i = 0; i < targets.size(); ++i) {
        Target& target = targets[i];
        if (target.remaining > 0 && acceptsCandy(target.id, candy)) {
            --target.remaining;
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool goalsComplete(const std::vector<Target>& targets, bool successFlag) {
    return successFlag || std::all_of(targets.begin(), targets.end(),
                                      [](const Target& t) { return t.remaining <= 0; });
}

int starsForScore(int score, const std::array<int,3>& thresholds) {
    for (int i = 2; i >= 0; --i) {
        if (score >= thresholds[i]) return i + 1;
    }
    return 0;
}

int remainingAfterTurn(int remaining, LimitMode mode) {
    return mode == LimitMode::Moves && remaining > 0 ? remaining - 1 : remaining;
}
int remainingAfterTick(int remaining, LimitMode mode) {
    return mode == LimitMode::Time && remaining > 0 ? remaining - 1 : remaining;
}

Outcome evaluateEnd(bool readyToEvaluate, const std::vector<Target>& targets, int remaining,
                    bool failureFlag, bool successFlag) {
    if (!readyToEvaluate) return Outcome::Playing;
    if (goalsComplete(targets, successFlag)) return Outcome::Won;
    if (remaining <= 0 || failureFlag) return Outcome::Lost;
    return Outcome::Playing;
}
}
