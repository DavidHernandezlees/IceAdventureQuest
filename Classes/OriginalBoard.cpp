#include "Puzzle.h"
#include <algorithm>
#include <stdexcept>

namespace ice {
namespace {
Power openingPower(int type) {
    switch (type) {
    case 2: return Power::Row;
    case 3: return Power::Column;
    case 4: return Power::Bomb;
    case 5: return Power::Jar;
    case 6: return Power::ColorBox;
    case 9: return Power::Rainbow;
    case 10: return Power::Stone;
    case 15: return Power::Compass;
    case 11: return Power::Drop;
    case 12: return Power::Box;
    case 22: return Power::Bear;
    default: return Power::Plain;
    }
}
int nativeKind(Power p) {
    switch(p) {
    case Power::Plain: return 1;
    case Power::Row: return 2;
    case Power::Column: return 3;
    case Power::Bomb: return 4;
    case Power::Jar: return 6;
    case Power::ColorBox: return 7;
    case Power::Rainbow: return 11;
    case Power::Drop: return 13;
    case Power::Box: return 14;
    case Power::Bear: return 24;
    case Power::Compass: return 17;
    case Power::Stone: return 12;
    }
    throw std::logic_error("Unknown game piece");
}
}

int Puzzle::stableColor(int i) {
    int r = i/width, c = i%width;
    auto weights = layout.weights;
    // Reserve specified neighbors, including ones to the right and below.
    for (int color = 1; color <= 6; ++color)
        for (int k = -2; k <= 0; ++k) {
            bool row = true, column = true;
            for (int d = k; d <= k+2; ++d) {
                if (!d) continue;
                row &= colorAt(r,c+d) == color;
                column &= colorAt(r+d,c) == color;
            }
            if (row || column) weights[color-1] = 0;
        }
    if (std::none_of(weights.begin(),weights.end(),[](int x){return x>0;})) return randomColor();
    return std::discrete_distribution<int>(weights.begin(),weights.end())(rng)+1;
}
void Puzzle::setOriginalOpening() {
    const auto &cells = layout.originalCells;
    pieces.resize(cells.size());
    for (int i=0; i<int(cells.size()); ++i) {
        const auto &c = cells[i];
        ice.push_back(layout.board[i/width][i%width] == '#' ? -1 : 0);
        bottom.push_back(c.bottom);
        hidden.push_back(c.hidden);
        frost.push_back(c.frost);
        cage.push_back(c.cage);
        if (playable(i) && c.initial != 255)
            pieces[i] = Piece(c.color,openingPower(c.initial),std::max(1,c.variant));
    }
    for (int variant : {1,2}) {
        int needed=0;
        for (const auto &target: objectives)
            if (target.id == 10+variant) needed+=target.remaining;
        for (const auto &piece: pieces)
            if (piece.power == Power::Drop && piece.strength == variant) --needed;
        if (needed>0) pendingDrops.insert(pendingDrops.end(),needed,variant);
    }
    for (int i=0; i<int(cells.size()); ++i)
        if (pieces[i].power==Power::ColorBox && !pieces[i].color) pieces[i].color=randomColor();
    for (int i=0; i<int(cells.size()); ++i)
        if (playable(i) && cells[i].initial != 255 && !pieces[i].color && !pieces[i].blocker() &&
            pieces[i].power != Power::Drop && pieces[i].power != Power::Rainbow)
            pieces[i].color = stableColor(i);
    Wave opening;
    originalGravity(opening);
    // Fixed opening patterns resolve through the same animation path as cascades.
    if (groups().empty()) ensureMove();
}
void Puzzle::tick(int seconds) {
    if (timed() && seconds > 0 && moves > 0) moves -= std::min(moves,seconds);
}
icequest::Outcome Puzzle::outcome() const {
    if (layout.original()) return icequest::evaluateEnd(!resolving,objectives,moves);
    if (resolving) return icequest::Outcome::Playing;
    if (won()) return icequest::Outcome::Won;
    return moves > 0 ? icequest::Outcome::Playing : icequest::Outcome::Lost;
}
Swap Puzzle::hint() const {
    if (tutorialStep < layout.tutorialSwaps.size()) {
        auto h = layout.tutorialSwaps[tutorialStep];
        if (legal(h.first,h.second)) return h;
    }
    auto options = choices();
    return options.empty() ? Swap(-1,-1) : options.front();
}
void Puzzle::damageCell(int i, Wave &wave) {
    auto &piece = pieces[i];
    if (locked(i)) {
        int kind=cage[i] ? 51 : 50;
        int &hp=cage[i] ? cage[i] : frost[i];
        if (--hp == 0) icequest::collectCandy(objectives,{kind,0,0});
        wave.cracked=true;
        if (piece.power != Power::Compass || locked(i)) return;
    }
    if (piece.blocker() && piece.strength > 1) {
        --piece.strength;
        wave.cracked = true;
        return;
    }
    icequest::collectCandy(objectives,{nativeKind(piece.power),piece.color,piece.strength});
    wave.crushed.push_back({i,piece});
    piece = Piece();
    // Bottom layer codes 2..4 are jelly and 5..7 cover a buried item.
    if (bottom[i] >= 2) {
        if (bottom[i] == 2 || bottom[i] == 5) {
            icequest::collectCandy(objectives,{bottom[i] == 2 ? 48 : 49,0,0});
            bottom[i] = 0;
        } else --bottom[i];
        wave.cracked = true;
    }
}
void Puzzle::revealGround() {
    static const int rows[] = {0,2,1,2,3,2,3};
    static const int cols[] = {0,1,2,2,2,3,3};
    for (int i=0; i<int(hidden.size()); ++i) {
        int shape = hidden[i];
        if (!shape) continue;
        bool covered = false;
        for (int r=i/width; r<std::min(height,i/width+rows[shape]); ++r)
            for (int c=i%width; c<std::min(width,i%width+cols[shape]); ++c)
                covered |= bottom[r*width+c]>=5;
        if (!covered) {
            hidden[i]=0;
            icequest::collectCandy(objectives,{46,0,shape});
        }
    }
}
Piece Puzzle::refill(int index) {
    if (dropSlots > 0 && !pendingDrops.empty()) {
        bool exitBelow=false;
        for (int i=index; i<int(pieces.size()); i+=width)
            if (layout.originalCells[i].exit || layout.originalCells[i].portalIn) exitBelow=true;
        if (exitBelow && std::uniform_int_distribution<int>(0,2)(rng)==1) {
            int pos=std::uniform_int_distribution<int>(0,int(pendingDrops.size())-1)(rng);
            int variant=pendingDrops[pos];
            pendingDrops.erase(pendingDrops.begin()+pos);
            --dropSlots;
            return Piece(0,Power::Drop,variant);
        }
    }
    return Piece(randomColor());
}
void Puzzle::originalGravity(Wave &wave) {
    // Resolve the column first, including pass-through gaps and paired portals.
    // At a solid obstruction, a movable diagonal neighbor can supply the space.
    std::vector<int> origin(pieces.size());
    for (int i=0; i<int(origin.size()); ++i) origin[i]=i;
    for (int i=int(pieces.size())-1; i>=0; --i) {
        if (!playable(i) || pieces[i].occupied()) continue;
        int dest=i;
        std::vector<bool> visited(pieces.size());
        for (;;) {
            int here=dest, src=-1;
            while (here>=0 && !visited[here]) {
                visited[here]=true;
                const auto &cell=layout.originalCells[here];
                int above=cell.upstreamPortal>=0 ? cell.upstreamPortal : here-width;
                while (above>=0 && !playable(above) && layout.originalCells[above].passThrough) above-=width;
                if (above<0 || visited[above] || !playable(above)) break;
                if (pieces[above].fixed() || locked(above)) {
                    for (int side : {-1,1}) {
                        int diagonal=above+side;
                        if (diagonal<0 || diagonal/width!=above/width || !playable(diagonal)) continue;
                        if (!visited[diagonal] && !locked(diagonal) && !pieces[diagonal].fixed() &&
                            pieces[diagonal].occupied()) { src=diagonal; break; }
                    }
                    break;
                }
                if (pieces[above].occupied()) { src=above; break; }
                here=above;
            }
            if (src<0) {
                pieces[dest]=refill(dest);
                origin[dest]=-1;
                break;
            }
            pieces[dest]=pieces[src];
            origin[dest]=origin[src];
            pieces[src]=Piece();
            dest=src;
        }
    }
    for (int i=0; i<int(origin.size()); ++i)
        if (origin[i]!=i) wave.falls.emplace_back(origin[i],i);
}
void Puzzle::collectDrops(Wave &wave) {
    for (;;) {
        bool any=false;
        for (int i=0; i<int(pieces.size()); ++i)
            if (pieces[i].power==Power::Drop && layout.originalCells[i].exit && !locked(i)) {
                if (icequest::collectCandy(objectives,{13,0,pieces[i].strength})>=0) ++dropSlots;
                pieces[i]=Piece();
                if (std::find(wave.hit.begin(),wave.hit.end(),i)==wave.hit.end()) wave.hit.push_back(i);
                any=true;
            }
        if (!any) break;
        // The last snapshot is rendered after all exits in this wave.
        wave.falls.clear();
        originalGravity(wave);
    }
}
} // namespace ice
