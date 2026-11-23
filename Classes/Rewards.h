#pragma once
#include <array>
#include <cstdint>
#include <set>
#include <string>

namespace ice {
struct Prize { int item, count; }; // IDs 4–7 are board boosters. ID 8 is gold.
extern const std::array<Prize, 7> dailyPrizes;
extern const std::array<int, 5> petUnlocks;
extern const std::array<const char *, 8> products;
extern const std::array<int, 8> productGold;
extern const std::array<int, 2> bundleBoosters;
constexpr int videoGold = 100;
constexpr int foodPrice = 25;
constexpr int washPrice = 15;
constexpr int boosterPrice = 100;
int localDay(std::int64_t seconds);
int smileGoal(int level);

struct Rewards {
    int gold = 1000, day = -1, streak = 0;
    std::int64_t videoTime = 0;
    std::array<int, 4> boosters{};
    int food = 3, soap = 3, pet = 0, level = 1, smiles = 0;
    std::array<int, 5> hats{};
    int bundles = 0;
    std::set<std::string> receipts;
    int nextDay(int today) const;
    bool claim(int today);
    bool video(std::int64_t now);
    bool buy(int item); // 0 food, 1 soap, 2–5 board boosters
    bool care(int action, int stage);
    bool selectPet(int index, int stage);
    bool purchase(int product, const std::string &receipt, int quantity);
    bool spendBooster(int index);
    std::string encode(unsigned revision) const;
    bool decode(const std::string &json, unsigned &revision);
};
}
