#include "Puzzle.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace ice {
Puzzle::Puzzle(const Level &level, unsigned seed)
    : width(static_cast<int>(level.board.at(0).size())), height(static_cast<int>(level.board.size())),
      moves(level.moves), remaining(level.collect), objectives(level.objectives), layout(level), rng(seed) {
    if (level.original()) {
        setOriginalOpening();
        return;
    }
    for (int r = 0; r < height; ++r)
        for (int c = 0; c < width; ++c) {
            char floor = level.board[r][c], gem = level.gems[r][c];
            int layers = floor == '#' ? -1 : floor == '.' ? 0 : floor - '0';
            ice.push_back(layers);
            iceLeft += std::max(layers, 0);
            pieces.emplace_back(layers < 0 ? 0 : gem == '.' ? randomColor() : gem - '0');
        }
    if (!groups().empty())
        seedStable();
    ensureMove();
}
bool Puzzle::playable(int i) const {
    return i >= 0 && i < int(ice.size()) && ice[i] >= 0;
}
bool Puzzle::adjacent(int a, int b) const {
    return playable(a) && playable(b) &&
           std::abs(a / width - b / width) + std::abs(a % width - b % width) == 1;
}
int Puzzle::randomColor() {
    std::discrete_distribution<int> draw(layout.weights.begin(), layout.weights.end());
    return draw(rng) + 1;
}
int Puzzle::colorAt(int r, int c) const {
    if (r < 0 || r >= height || c < 0 || c >= width)
        return 0;
    const auto &p = pieces[r * width + c];
    return (!frost.empty() && frost[r*width+c]) || p.power == Power::Rainbow || p.blocker() || p.power == Power::Drop ? 0 : p.color;
}
bool Puzzle::ordinaryMove(int a, int b) const {
    auto after = [&](int r, int c) {
        if (r < 0 || c < 0 || r >= height || c >= width)
            return 0;
        int i = r * width + c, src = i == a ? b : i == b ? a : i;
        return colorAt(src / width, src % width);
    };
    for (int i : {a, b}) {
        int r = i / width, c = i % width, v = after(r, c);
        if (!v)
            continue;
        for (int k = -2; k <= 0; ++k) {
            if (after(r, c + k) == v && after(r, c + k + 1) == v && after(r, c + k + 2) == v)
                return true;
            if (after(r + k, c) == v && after(r + k + 1, c) == v && after(r + k + 2, c) == v)
                return true;
        }
    }
    return false;
}
bool Puzzle::legal(int a, int b) const {
    if (!adjacent(a, b))
        return false;
    if (locked(a) || locked(b) || pieces[a].fixed() || pieces[b].fixed() || !pieces[a].occupied() || !pieces[b].occupied())
        return false;
    auto p = pieces[a].power, q = pieces[b].power;
    if (p == Power::Rainbow || q == Power::Rainbow) {
        const auto &partner = p == Power::Rainbow ? pieces[b] : pieces[a];
        return partner.special() || (partner.power == Power::Plain && partner.color != 0);
    }
    return (pieces[a].special() && pieces[b].special()) ||
           ordinaryMove(a, b);
}
std::vector<Swap> Puzzle::choices() const {
    std::vector<Swap> out;
    for (int i = 0; i < int(pieces.size()); ++i)
        for (int j : {i + 1, i + width})
            if (legal(i, j))
                out.emplace_back(i, j);
    return out;
}
bool Puzzle::swap(int a, int b) {
    if (moves <= 0 || won() || resolving || !legal(a, b))
        return false;
    std::swap(pieces[a], pieces[b]);
    last = {a, b};
    resolving = true;
    matchList = 0;
    ++tutorialStep;
    moves = icequest::remainingAfterTurn(moves, layout.mode);
    return true;
}
std::vector<Puzzle::Group> Puzzle::groups() const {
    std::vector<Group> out;
    auto add = [&](int start, int stride, int length) {
        Group g;
        g.across = stride == 1;
        g.down = !g.across;
        g.longest = length;
        for (int k = 0; k < length; ++k)
            g.cells.push_back(start + k * stride);
        for (auto it = out.begin(); it != out.end();) {
            bool overlaps = std::any_of(it->cells.begin(), it->cells.end(), [&](int i) {
                return std::find(g.cells.begin(), g.cells.end(), i) != g.cells.end();
            });
            if (!overlaps) {
                ++it;
                continue;
            }
            g.cells.insert(g.cells.end(), it->cells.begin(), it->cells.end());
            g.across |= it->across;
            g.down |= it->down;
            g.longest = std::max(g.longest, it->longest);
            it = out.erase(it);
        }
        std::sort(g.cells.begin(), g.cells.end());
        g.cells.erase(std::unique(g.cells.begin(), g.cells.end()), g.cells.end());
        out.push_back(g);
    };
    for (int i = 0; i < int(pieces.size()); ++i) {
        int r = i / width, c = i % width, v = colorAt(r, c);
        if (!v)
            continue;
        if (v != colorAt(r, c - 1)) {
            int n = 1;
            while (colorAt(r, c + n) == v)
                ++n;
            if (n >= 3)
                add(i, 1, n);
        }
        if (v != colorAt(r - 1, c)) {
            int n = 1;
            while (colorAt(r + n, c) == v)
                ++n;
            if (n >= 3)
                add(i, width, n);
        }
    }
    return out;
}
bool Puzzle::canTargetBooster(int item, int index) const {
    return item>=0 && item<4 && playable(index) && !resolving && !tutorial() && boosterTarget<0 &&
        pieces[index].power==Power::Plain && pieces[index].color>0 && !locked(index);
}
bool Puzzle::targetBooster(int item, int index) {
    if (!canTargetBooster(item,index)) return false;
    const Power powers[]={Power::Plain,Power::Row,Power::Bomb,Power::Rainbow};
    pieces[index].power=powers[item];
    boosterTarget=index;
    return true;
}
Wave Puzzle::resolve(int depth) {
    Wave wave;
    std::vector<bool> marked(pieces.size()), painted(pieces.size());
    std::vector<int> points(pieces.size());
    auto hit = [&](int i) {
        if (playable(i) && !marked[i] &&
            (locked(i) || (pieces[i].power != Power::Stone && pieces[i].power != Power::Drop))) {
            marked[i] = true;
            wave.hit.push_back(i);
        }
    };
    if (boosterTarget>=0) {
        hit(boosterTarget);
        boosterTarget=-1;
    }
    std::vector<std::pair<int, Piece>> created;
    for (const auto &group : groups()) {
        bool spreads = layout.original() && std::any_of(group.cells.begin(), group.cells.end(),
            [&](int i) { return bottom[i] == 1; });
        int base = 600 + 10 * ++matchList;
        for (int i : group.cells) {
            hit(i);
            points[i] = base;
            painted[i] = spreads;
        }
        Power p = Power::Plain;
        if (group.longest >= 5)
            p = Power::Rainbow;
        else if (group.across && group.down)
            p = Power::Bomb;
        else if (group.longest == 4)
            p = group.across ? Power::Row : Power::Column;
        if (p == Power::Plain)
            continue;
        int anchor = group.cells[group.cells.size() / 2];
        for (int i : {last.first, last.second})
            if (std::find(group.cells.begin(), group.cells.end(), i) != group.cells.end())
                anchor = i;
        created.emplace_back(anchor, Piece(pieces[anchor].color, p));
    }
    Power flyingPower = Power::Plain;
    int carrier = -1;
    int rainbowColor = 0;
    bool clearAll = false;
    if (last.first >= 0) {
        const auto &a = pieces[last.first];
        const auto &b = pieces[last.second];
        clearAll = a.power == Power::Rainbow && b.power == Power::Rainbow;
        if (a.power == Power::Rainbow || b.power == Power::Rainbow)
            rainbowColor = a.power == Power::Rainbow ? b.color : a.color;
        if (clearAll || rainbowColor || (a.special() && b.special())) {
            hit(last.first);
            hit(last.second);
        }
    }
    if (layout.original() && last.first >= 0 && pieces[last.first].special() && pieces[last.second].special()) {
        int origin = last.second, r = origin / width, c = origin % width;
        auto p = pieces[last.first].power, q = pieces[last.second].power;
        if (p == Power::Rainbow || q == Power::Rainbow) {
            auto convert = p == Power::Rainbow ? q : p;
            if (convert != Power::Rainbow) {
                for (int i = 0; i < int(pieces.size()); ++i)
                    if (pieces[i].power == Power::Plain && pieces[i].color == rainbowColor) {
                        pieces[i].power = (convert == Power::Bomb || convert == Power::Plane) ? convert : (rng()%2 ? Power::Row : Power::Column);
                        hit(i);
                    }
            }
        } else if (p == Power::Plane || q == Power::Plane) {
            carrier = p == Power::Plane ? last.first : last.second;
            flyingPower = p == Power::Plane ? q : p;
        } else {
            int bombs = int(p == Power::Bomb) + int(q == Power::Bomb);
            for (int i = 0; i < int(pieces.size()); ++i) {
                int dy = std::abs(i / width-r), dx = std::abs(i % width-c);
                if ((bombs == 2 && std::max(dx,dy) <= 2) ||
                    (bombs == 1 && std::min(dx,dy) <= 1) ||
                    (!bombs && std::min(dx,dy) == 0)) hit(i);
            }
        }
    }
    last = {-1, -1};
    for (size_t k = 0; k < wave.hit.size(); ++k) {
        int i = wave.hit[k], r = i / width, c = i % width;
        Power p = pieces[i].power;
        if (locked(i) || !pieces[i].special())
            continue;
        wave.fired = true;
        if (p == Power::Plane) {
            for (int n : {i - width, i + width, i - 1, i + 1})
                if (adjacent(i, n)) hit(n);
            int dest = planeTarget(marked);
            if (dest >= 0) {
                wave.flights.emplace_back(i, dest);
                hit(dest);
                bool carriesPaint = bottom[i] == 1 || painted[i];
                painted[dest] = painted[dest] || carriesPaint;
                if (i == carrier && flyingPower != Power::Plane) {
                    for (int j = 0; j < int(pieces.size()); ++j) {
                        int dx = std::abs(j % width - dest % width), dy = std::abs(j / width - dest / width);
                        if ((flyingPower == Power::Row && dy == 0) || (flyingPower == Power::Column && dx == 0) ||
                            (flyingPower == Power::Bomb && std::max(dx, dy) <= 1)) hit(j);
                    }
                }
            }
            continue;
        }
        int burstColor = rainbowColor ? rainbowColor : pieces[i].color;
        if (p == Power::Rainbow && !clearAll && !burstColor) {
            std::vector<int> candidates;
            for (const Piece &piece : pieces)
                if (piece.power == Power::Plain && piece.color) candidates.push_back(piece.color);
            if (!candidates.empty())
                burstColor = candidates[std::uniform_int_distribution<size_t>(0,candidates.size()-1)(rng)];
        }
        for (int j = 0; j < int(pieces.size()); ++j) {
            int y = j / width, x = j % width;
            if ((p == Power::Row && r == y) || (p == Power::Column && c == x) ||
                (p == Power::Bomb && std::abs(r - y) <= 1 && std::abs(c - x) <= 1) ||
                (p == Power::Rainbow &&
                 (clearAll || (burstColor && pieces[j].color == burstColor && !pieces[j].blocker()))))
                hit(j);
        }
    }
    if (wave.hit.empty()) {
        if (layout.original()) collectDrops(wave);
        resolving = !wave.hit.empty();
        return wave;
    }
    resolving = true;
    if (layout.original()) {
        // A neighboring collection can damage the same blocker only once in a wave.
        size_t directHits = wave.hit.size();
        for (size_t j = 0; j < directHits; ++j) {
            int i = wave.hit[j];
            if (pieces[i].blocker() || pieces[i].power == Power::Drop) continue;
            for (int n : {i-width, i+width, i-1, i+1}) {
                if (!adjacent(i,n)) continue;
                if (frost[n]) { hit(n); continue; }
                if (!pieces[n].blocker()) continue;
                const Piece &neighbor = pieces[n];
                if ((neighbor.power != Power::Jar && neighbor.power != Power::ColorBox) || !neighbor.color || neighbor.color == pieces[i].color)
                    hit(n);
            }
        }
        for (int i : wave.hit) {
            if (pieces[i].power == Power::Drop && !locked(i)) continue;
            if (painted[i] && bottom[i] == 0) {
                bottom[i] = 1;
                icequest::collectCandy(objectives, {47,0,0});
            }
            damageCell(i, wave);
            score += points[i] ? points[i] : 600 + 10 * std::max(1,matchList);
        }
        for (const auto &entry : created)
            if (!pieces[entry.first].occupied()) pieces[entry.first] = entry.second;
        revealGround();
        originalGravity(wave);
        collectDrops(wave);
        return wave;
    }
    for (int i : wave.hit) {
        if (ice[i] > 0) {
            --ice[i];
            --iceLeft;
            wave.cracked = true;
        }
        int color = pieces[i].color;
        if (color > 0 && remaining[color - 1] > 0)
            --remaining[color - 1];
        wave.crushed.push_back({i,pieces[i]});
        pieces[i] = Piece();
    }
    for (const auto &entry : created)
        pieces[entry.first] = entry.second;
    score += int(wave.hit.size()) * 100 * std::min(5, depth + 1);
    collapse(wave);
    return wave;
}
void Puzzle::collapse(Wave &wave) {
    for (int c = 0; c < width; ++c) {
        int write = height - 1;
        for (int r = height - 1; r >= -1; --r) {
            int src = r * width + c;
            if (r < 0 || !playable(src)) {
                while (write > r) {
                    int dst = write-- * width + c;
                    pieces[dst] = Piece(randomColor());
                    wave.falls.emplace_back(-1, dst);
                }
                write = r - 1;
            } else if (pieces[src].color) {
                int dst = write-- * width + c;
                if (dst != src) {
                    pieces[dst] = pieces[src];
                    pieces[src] = Piece();
                    wave.falls.emplace_back(src, dst);
                }
            }
        }
    }
}
void Puzzle::seedStable() {
    for (int i = 0; i < int(pieces.size()); ++i) {
        if (!playable(i))
            continue;
        int r = i / width, c = i % width, v;
        do {
            v = randomColor();
        } while ((colorAt(r, c - 1) == v && colorAt(r, c - 2) == v) ||
                 (colorAt(r - 1, c) == v && colorAt(r - 2, c) == v));
        pieces[i] = Piece(v);
    }
}
bool Puzzle::ensureMove() {
    if (!choices().empty())
        return false;
    std::vector<Piece> pool;
    for (int i = 0; i < int(pieces.size()); ++i)
        if (playable(i) && (!layout.original() || (pieces[i].power == Power::Plain && !locked(i))))
            pool.push_back(pieces[i]);
    for (int tryNo = 0; tryNo < 120; ++tryNo) {
        if (tryNo < 50 || layout.original()) {
            std::shuffle(pool.begin(), pool.end(), rng);
            auto it = pool.begin();
            for (int i = 0; i < int(pieces.size()); ++i)
                if (playable(i) && (!layout.original() || (pieces[i].power == Power::Plain && !locked(i))))
                    pieces[i] = *it++;
        } else
            seedStable();
        if (groups().empty() && !choices().empty())
            return true;
    }
    // Even a narrow custom board can continue with a rainbow next to another gem.
    if (layout.original()) {
        for (int i = 0; i < int(pieces.size()); ++i)
            if (pieces[i].power == Power::Plain && playable(i) && !locked(i)) pieces[i].color = stableColor(i);
        if (!choices().empty()) return true;
        for (int i = 0; i < int(pieces.size()); ++i) {
            if (!playable(i) || pieces[i].power != Power::Plain || locked(i)) continue;
            Piece keep = pieces[i];
            pieces[i].power = Power::Rainbow;
            if (!choices().empty()) return true;
            pieces[i] = keep;
        }
        return true; // The scene reports a dead board if the fixed geometry admits no move.
    }
    seedStable();
    for (int i = 0; i < int(pieces.size()); ++i)
        if (adjacent(i, i + 1) || adjacent(i, i + width)) {
            pieces[i].power = Power::Rainbow;
            return true;
        }
    throw std::runtime_error("Board needs at least two adjacent cells");
}
bool Puzzle::won() const {
    if (layout.original()) return icequest::goalsComplete(objectives);
    return iceLeft == 0 && std::all_of(remaining.begin(), remaining.end(), [](int v) { return v == 0; });
}
int Puzzle::finishScore() const {
    if (layout.original()) {
        if (!won() || resolving) return score;
        // MatchGameLayer::endBonusTime(true) fast-forwards the original victory bonus.
        int specials = int(std::count_if(pieces.begin(),pieces.end(),[](const Piece &p){return p.special();}));
        return score + 10000*(moves+specials);
    }
    return score + (won() ? moves * 300 : 0);
}
int Puzzle::stars() const {
    if (layout.original()) return icequest::starsForScore(finishScore(),layout.originalStars);
    return won() ? 1 + (finishScore() >= layout.stars[0]) + (finishScore() >= layout.stars[1]) : 0;
}
} // namespace ice
