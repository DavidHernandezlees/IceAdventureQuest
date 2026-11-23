#include "Rewards.h"
#include "Puzzle.h"
#include "OriginalLevel.h"
#include <cassert>
#include <fstream>
#include <ctime>
#include <sstream>

int main() {
    // Check successive local calendar dates, including the US daylight-saving transition.
    std::tm t{};
    t.tm_year=124; t.tm_mon=2; t.tm_mday=9; t.tm_hour=12; t.tm_isdst=-1;
    int march9=ice::localDay(std::mktime(&t));
    t.tm_mday=10; t.tm_isdst=-1;
    assert(ice::localDay(std::mktime(&t))==march9+1);
    ice::Rewards r;
    for (int i=0; i<7; ++i) {
        assert(r.nextDay(10+i)==i);
        assert(r.claim(10+i));
        assert(!r.claim(10+i) && !r.claim(9+i));
        unsigned revision=0;
        ice::Rewards reload;
        assert(reload.decode(r.encode(12),revision) && revision==12);
        r=reload;
    }
    assert(r.gold==1900 && (r.boosters==std::array<int,4>{{1,1,1,1}}));
    assert(r.claim(17) && r.streak==1);
    assert(r.claim(19) && r.streak==1);
    assert(r.video(100000) && !r.video(99999) && !r.video(103599) && r.video(103600));
    assert(!r.selectPet(0,5) && r.selectPet(0,6));
    assert(!r.selectPet(1,30) && r.selectPet(1,31));
    assert(!r.care(0,5) && r.care(0,31) && r.care(0,31));
    assert(r.level==2 && r.smiles==0 && r.food==1);
    assert(r.care(1,31) && r.soap==2 && r.smiles==2);
    assert(r.care(2,31) && r.hats[1]==1 && r.hats[0]==0);
    int before=r.gold;
    assert(r.buy(0) && r.food==2 && r.gold==before-ice::foodPrice);
    assert(r.buy(2) && r.boosters[0]==2 && r.spendBooster(0));
    r.gold=0;
    assert(!r.buy(5) && r.boosters[3]==1);
    assert(!r.purchase(8,"id",1) && !r.purchase(0,"",1));
    assert(r.purchase(5,"coin-order",2) && r.gold==1000);
    assert(r.purchase(6,"bundle-order",1) && r.gold==1500 && r.boosters[0]==2);
    unsigned revision=0;
    ice::Rewards saved;
    assert(saved.decode(r.encode(8),revision) && saved.hats[1]==1 && saved.level==2);
    assert(saved.purchase(5,"coin-order",2) && saved.gold==1500);
    assert(saved.purchase(6,"bundle-restore",1) && saved.gold==1500);
    assert(saved.purchase(7,"second-bundle",1));
    assert(saved.gold==4300 && saved.boosters[0]==7 && saved.boosters[3]==1);
    assert(!saved.decode("[1,9",revision));
    assert(!saved.decode("{}",revision));
    ice::Rewards chests;
    assert(chests.decode("[1,12,300,-1,0,0,0,0,0,0,3,3,0,1,0,0,0,0,0,0,0,[]]",revision));
    assert(chests.gold==300 && chests.starChests==0 && revision==12);
    assert(!chests.claimStarChest(19,0,0) && chests.claimStarChest(20,0,1));
    assert(!chests.claimStarChest(20,0,1) && chests.claimStarChest(60,1,2));
    assert(chests.claimStarChest(60,2,3) && !chests.claimStarChest(60,2,3));
    assert((chests.boosters==std::array<int,4>{{1,1,1,1}}));
    assert(chests.decode(chests.encode(13),revision) && chests.starChests==3);
    assert(chests.claimStarChest(80,2,3) && chests.boosters[2]==2);
    std::ifstream source("Resources/level/1.txt");
    std::ostringstream json;json<<source.rdbuf();
    for(int item=0;item<4;++item) {
        auto level=ice::loadOriginalLevel(json.str());
        level.tutorials.clear();
        ice::Puzzle board(level,42);
        int target=-1;
        for(int i=0;i<int(board.pieces.size());++i)
            if(board.playable(i) && board.pieces[i].color>0 && !board.locked(i)) {target=i;break;}
        int moves=board.moves;
        assert(!board.targetBooster(item,-1) && board.targetBooster(item,target));
        auto wave=board.resolve(0);
        assert(!wave.hit.empty() && board.moves==moves);
        if(item) assert(wave.fired && wave.hit.size()>1);
    }
}
