#include "Rewards.h"
#include "json/document.h"
#include "json/writer.h"
#include "json/stringbuffer.h"
#include <algorithm>
#include <ctime>
#include <limits>

namespace ice {
const std::array<Prize, 7> dailyPrizes{{{8,100},{4,1},{5,1},{8,300},{6,1},{7,1},{8,500}}};
const std::array<int, 5> petUnlocks{{6,31,61,91,121}};
const std::array<const char *, 8> products{{"pay_gems100000","pay_gems40000","pay_gems20000",
    "pay_gems6000","pay_gems2800","pay_gems500","pay_onetime_1","pay_onetime_2"}};
const std::array<int, 8> productGold{{100000,40000,20000,6000,2800,500,500,2800}};

const std::array<int, 2> bundleBoosters{{1,5}};

int localDay(std::int64_t seconds) {
    // Civil day number stays consecutive across daylight-saving transitions.
    std::time_t time = static_cast<std::time_t>(seconds);
    auto date = std::localtime(&time);
    if (!date) return -1;
    int month = date->tm_mon + 1;
    int year = date->tm_year + 1900 - (month <= 2);
    month += month > 2 ? -3 : 9;
    return year * 365 + year / 4 - year / 100 + year / 400 + (153 * month + 2) / 5 + date->tm_mday;
}
int smileGoal(int level) {
    static const int goals[] = {6,20,30,30,30,40,40,40,50,50,50,50,60,60,60,60,70,70,70,70,70,
        80,80,80,80,80,90,90,90,90,90,100,100,100,110,110,120,130,140,150,160,170,180,190,200,210,
        220,230,240,250,260,270,280,290,300,320,340,360,380,400,420,440,460,480,500,520,540,560,580,
        600,635,670,705,740,775,810,845,880,915,950,985,1020,1055,1090,1125,1160,1195,1230,1265,
        1300,1335,1370,1405,1440,1475,1510,1545,1580,1615,1650,1685,1720,1755,1790,1825,1860,
        1895,1930,1965,2000,2040,2080,2120,2160,2200};
    return goals[std::min(114,std::max(0,level-1))];
}
static int add(int a, std::int64_t b) {
    return static_cast<int>(std::min<std::int64_t>(std::numeric_limits<int>::max(), a+b));
}
int Rewards::nextDay(int today) const {
    if (today < 0 || today <= day) return -1;
    return static_cast<std::int64_t>(today) - day == 1 ? streak % 7 : 0;
}
bool Rewards::claim(int today) {
    int index = nextDay(today);
    if (index < 0) return false;
    auto p = dailyPrizes[index];
    if (p.item == 8) gold = add(gold,p.count);
    else boosters[p.item-4] = add(boosters[p.item-4],p.count);
    day = today;
    streak = index+1;
    return true;
}
bool Rewards::video(std::int64_t now) {
    if (now <= 0 || (videoTime && (now < videoTime || now-videoTime < 3600))) return false;
    gold = add(gold,videoGold);
    videoTime = now;
    return true;
}
bool Rewards::buy(int item) {
    if (item < 0 || item > 5) return false;
    int price = item == 0 ? foodPrice : item == 1 ? washPrice : boosterPrice;
    if (gold < price) return false;
    int &count = item == 0 ? food : item == 1 ? soap : boosters[item-2];
    if (count == std::numeric_limits<int>::max()) return false;
    gold -= price;
    ++count;
    return true;
}
bool Rewards::selectPet(int index, int stage) {
    if (index < 0 || index >= 5 || stage < petUnlocks[index]) return false;
    pet = index;
    return true;
}
bool Rewards::care(int action, int stage) {
    if (stage < petUnlocks[pet] || action < 0 || action > 2) return false;
    if (action == 2) {
        hats[pet] = (hats[pet]+1) % (1 + std::min(3,level));
        return true;
    }
    int &stock = action == 0 ? food : soap;
    if (!stock) return false;
    --stock;
    smiles += action == 0 ? 3 : 2;
    if (smiles >= smileGoal(level)) {
        smiles -= smileGoal(level);
        level = std::min(115,level+1);
    }
    return true;
}
bool Rewards::spendBooster(int index) {
    if (index < 0 || index >= 4 || !boosters[index]) return false;
    --boosters[index];
    return true;
}
bool Rewards::purchase(int product, const std::string &receipt, int quantity) {
    if (product < 0 || product >= 8 || receipt.empty() || quantity < 1 || quantity > 999) return false;
    if (!receipts.insert(receipt).second) return true;
    if (product >= 6) {
        int mask = 1 << (product-6);
        if (bundles & mask) return true;
        bundles |= mask;
        for (int i=0; i<3; ++i) boosters[i] = add(boosters[i],bundleBoosters[product-6]);
        quantity = 1;
    }
    gold = add(gold,static_cast<std::int64_t>(productGold[product])*quantity);
    return true;
}
std::string Rewards::encode(unsigned revision) const {
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> w(buf);
    w.StartArray(); w.Int(1); w.Uint(revision);
    for (int n : {gold,day,streak}) w.Int(n);
    w.Int64(videoTime);
    for (int n : boosters) w.Int(n);
    for (int n : {food,soap,pet,level,smiles,bundles}) w.Int(n);
    for (int n : hats) w.Int(n);
    w.StartArray();
    for (const auto &id : receipts) w.String(id.c_str());
    w.EndArray(); w.EndArray();
    return buf.GetString();
}
bool Rewards::decode(const std::string &json, unsigned &revision) {
    rapidjson::Document d;
    d.Parse(json.c_str());
    if (!d.IsArray() || d.Size()!=22 || !d[0u].IsInt() || d[0u].GetInt()!=1 ||
        !d[1].IsUint() || !d[5].IsInt64() || d[5].GetInt64()<0 || !d[21].IsArray()) return false;
    for (unsigned i = 2; i <= 20; ++i)
        if (i != 5 && (!d[i].IsInt() || d[i].GetInt() < (i == 3 ? -1 : 0))) return false;
    if (d[4].GetInt()>7 || d[12].GetInt()>4 || d[13].GetInt()<1 || d[13].GetInt()>115 ||
        d[14].GetInt()>=smileGoal(d[13].GetInt()) || d[15].GetInt()>3) return false;
    Rewards r;
    r.gold=d[2].GetInt(); r.day=d[3].GetInt(); r.streak=d[4].GetInt(); r.videoTime=d[5].GetInt64();
    for (unsigned i=0;i<4;++i) r.boosters[i]=d[6+i].GetInt();
    r.food=d[10].GetInt(); r.soap=d[11].GetInt(); r.pet=d[12].GetInt(); r.level=d[13].GetInt();
    r.smiles=d[14].GetInt(); r.bundles=d[15].GetInt();
    for (unsigned i=0;i<5;++i) {
        if (d[16+i].GetInt()>3) return false;
        r.hats[i]=d[16+i].GetInt();
    }
    for (auto it=d[21].Begin();it!=d[21].End();++it) {
        if (!it->IsString()) return false;
        r.receipts.insert(it->GetString());
    }
    *this=std::move(r); revision=d[1].GetUint();
    return true;
}
}
