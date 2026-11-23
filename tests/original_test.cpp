#include "OriginalLevel.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>

using namespace ice;

static Level readLevel(const std::string &name) {
    std::ifstream f("Resources/"+name);
    assert(f);
    return loadOriginalLevel({std::istreambuf_iterator<char>(f),{}});
}
static int settle(Puzzle &game) {
    int depth=0;
    while (!game.resolve(depth).hit.empty()) assert(++depth<150);
    return depth;
}
static int progress(const Puzzle &g) {
    int total=0;
    for (const auto &t:g.objectives) total+=t.remaining;
    return total;
}
static void mechanics() {
    auto level=readLevel("level/1.txt");
    assert(level.moves==18 && level.objectives[0].id==2 && level.objectives[0].remaining==20);
    assert((level.originalStars==std::array<int,3>{{20000,30000,40000}}));
    Puzzle first(level,42);
    assert(first.pieces[0].color==1 && first.pieces[1].color==4 && !first.playable(7));
    assert(first.hint()==Swap(12,11)); // The supplied tutorial points at the blue swap.
    assert(!first.swap(6,7) && first.moves==18);
    assert(first.swap(12,11) && first.moves==17);
    auto wave=first.resolve(0);
    assert(wave.hit.size()==3 && first.objectives[0].remaining==17 && first.score==1830);

    assert(wave.crushed.size()==3);
    for (const auto &crush : wave.crushed) {
        assert(crush.piece.power==Power::Plain && crush.piece.color==2);
        assert(std::find(wave.hit.begin(),wave.hit.end(),crush.cell)!=wave.hit.end());
    }

    auto timed=level;
    timed.moves=1;
    timed.mode=icequest::LimitMode::Time;
    timed.objectives={{2,1,1},{2,2,2}};
    Puzzle t(timed,42);
    assert(t.swap(12,11) && t.moves==1);
    t.tick(5);
    assert(t.moves==0 && t.outcome()==icequest::Outcome::Playing);
    settle(t);
    assert(t.outcome()==icequest::Outcome::Won && !t.swap(12,11));
    assert(t.objectives[0].remaining==0 && t.objectives[1].remaining==0);
    Puzzle loss(timed,42);
    loss.tick(2);
    assert(loss.outcome()==icequest::Outcome::Lost && !loss.swap(12,11));
    Puzzle m(level,42);
    m.tick(5);
    assert(m.moves==18);
    for (int k=0;k<3;++k) {
        m.score=level.originalStars[k]-1;
        assert(m.stars()==k);
        ++m.score;
        assert(m.stars()==k+1);
    }

    auto special=level;
    special.objectives={{2,20,20},{7,2,2},{13,1,1}};
    Puzzle s(special,42);
    s.pieces[0]=Piece(2,Power::Row);
    s.pieces[1]=Piece(2,Power::Column);
    s.pieces[2]=Piece(0,Power::Box,2);
    assert(s.swap(0,1));
    auto blast=s.resolve(0);
    assert(std::any_of(blast.crushed.begin(),blast.crushed.end(),[](const Crush &c){
        return c.cell==1 && c.piece.power==Power::Row;
    }));
    assert(std::any_of(blast.crushed.begin(),blast.crushed.end(),[](const Crush &c){
        return c.cell==0 && c.piece.power==Power::Column;
    }));
    assert(std::none_of(blast.crushed.begin(),blast.crushed.end(),[](const Crush &c){return c.cell==2;}));
    assert(s.objectives[1].remaining==0 && s.pieces[2].power==Power::Box && s.pieces[2].strength==1);
    assert(s.objectives[2].remaining==1);

    auto exits=level;
    exits.originalCells[6*9].exit=true;
    exits.originalCells[6*9].initial=11;
    exits.originalCells[6*9].variant=2;
    exits.originalCells[6*9].color=0;
    exits.objectives={{11,1,1},{12,1,1}};
    Puzzle delivery(exits,2);
    assert(delivery.swap(12,11));
    delivery.resolve(0);
    assert(delivery.objectives[0].remaining==1 && delivery.objectives[1].remaining==0);

    Puzzle initialExit(exits,2);
    assert(!initialExit.resolve(0).hit.empty());
    assert(initialExit.moves==18 && initialExit.objectives[1].remaining==0);

    auto terrain=level;
    terrain.originalCells[0].bottom=1;
    terrain.originalCells[1].bottom=2;
    terrain.originalCells[3].hidden=2;
    terrain.originalCells[3].bottom=5;
    terrain.originalCells[4].bottom=5;
    terrain.objectives={{30,1,1},{31,2,2},{32,1,1}};
    Puzzle ground(terrain,42);
    ground.pieces[0]=Piece(2,Power::Row);
    ground.pieces[1]=Piece(3,Power::Column);
    assert(ground.swap(0,1));
    ground.resolve(0);
    assert(!ground.hidden[3] && !ground.bottom[1] && !ground.bottom[3] && !ground.bottom[4]);
    assert(!ground.objectives[0].remaining && !ground.objectives[2].remaining);

    auto paint=level;
    // The tutorial swap completes the vertical line in column 2.
    Puzzle discover(paint,42);
    assert(discover.swap(12,11));
    const auto matches=discover.resolve(0).hit;
    paint.originalCells[matches.front()].bottom=1;
    paint.objectives={{31,2,2}};
    Puzzle spread(paint,42);
    assert(spread.swap(12,11));
    settle(spread);
    assert(spread.won() && !spread.objectives.front().remaining);
    for (int i:matches) assert(spread.bottom[i]==1);
    int specials=std::count_if(spread.pieces.begin(),spread.pieces.end(),[](const Piece &p){return p.special();});
    const int finished=spread.score+10000*(spread.moves+specials);
    assert(spread.finishScore()==finished && spread.finishScore()==finished);
    assert(spread.stars()==icequest::starsForScore(finished,paint.originalStars));

    auto masks=level;
    masks.originalCells[2].frost=1;
    masks.originalCells[4].cage=1;
    Puzzle locked(masks,2);
    assert(!locked.legal(2,3) && !locked.legal(4,5));
    auto portals=readLevel("level/20.txt");
    assert(portals.originalCells[2].upstreamPortal==8*9+8);
    assert(portals.originalCells[6].upstreamPortal==8*9);

    std::vector<icequest::Target> repeated={{7,1,1},{7,2,2},{2,1,1}};
    assert(icequest::collectCandy(repeated,{2,2,0})==0 && repeated[2].remaining==1);
    assert(icequest::collectCandy(repeated,{3,2,0})==1 && repeated[1].remaining==1);
}
int main() {
    mechanics();
    std::ifstream list("Resources/original-index.txt");
    assert(list);
    std::string path;
    int count=0, turns=0, victories=0, defeats=0;
    while (list>>path) {
        auto l=readLevel(path);
        for (int seed=1;seed<=2;++seed) {
            Puzzle p(l,seed);
            settle(p);
            for (int turn=0;p.outcome()==icequest::Outcome::Playing;++turn) {
                p.ensureMove();
                auto hint=p.hint();
                if(hint.first<0) {std::cerr<<"No move "<<path<<" seed "<<seed<<" turn "<<turn<<"\n"; return 2;}
                if (seed==2) {
                    auto options=p.choices();
                    hint=options[(11*turn+count)%options.size()];
                }
                auto before=progress(p);
                assert(p.swap(hint.first,hint.second));
                assert(!p.swap(hint.first,hint.second));
                if (p.timed()) p.tick(1);
                settle(p);
                assert(progress(p)<=before);
                for (int i=0;i<int(p.pieces.size());++i)
                    assert(p.playable(i)==p.pieces[i].occupied());
                ++turns;
            }
            if (p.won()) ++victories;
            else { assert(p.moves==0); ++defeats; }
        }
        ++count;
    }
    assert(count>0);
    std::cout<<count<<" original levels, two seeds each, "<<turns<<" valid turns, "<<victories<<" wins, "<<defeats<<" losses\n";
}
