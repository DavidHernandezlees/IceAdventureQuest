#include "OriginalLevel.h"
#include "json/document.h"
#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace ice {
namespace {
using Value = rapidjson::Value;
void require(bool ok, const char *why) {
    if (!ok) throw std::invalid_argument(why);
}
const Value &field(const Value &object, const char *name) {
    require(object.IsObject() && object.HasMember(name), "missing level field");
    return object[name];
}
int integer(const Value &v) {
    require(v.IsInt(), "invalid integer");
    return v.GetInt();
}
uint32_t bits(const Value &v) {
    if (v.IsUint()) return v.GetUint();
    return static_cast<uint32_t>(integer(v));
}
void grid(const Value &rows) {
    require(rows.IsArray() && rows.Size() == 10, "unsupported board height");
    for (const auto &r : rows.GetArray())
        require(r.IsArray() && r.Size() == 9, "unsupported board width");
}
bool supportsTarget(int id) {
    return (id >= 1 && id <= 14) || id == 17 || id == 23 || id == 30 || id == 31 || id == 32 || id == 33 || id == 34;
}
} // namespace

Level loadOriginalLevel(const std::string &json) {
    rapidjson::Document d;
    d.Parse(json.c_str());
    require(!d.HasParseError() && d.IsObject(), "invalid JSON");
    Level l;
    l.moves = integer(field(d, "movecount"));
    int mode = integer(field(d, "moveortime"));
    require(l.moves > 0 && (mode == 0 || mode == 1), "invalid limit");
    l.mode = mode == 0 ? icequest::LimitMode::Moves : icequest::LimitMode::Time;
    const auto &scores = field(d, "scores");
    require(scores.IsArray() && scores.Size() == 3, "invalid star thresholds");
    for (int i = 0; i < 3; ++i) l.originalStars[i] = integer(scores[i]);
    const auto &colors = field(d, "colors");
    require(colors.IsArray() && colors.Size() % 2 == 0, "invalid color weights");
    for (unsigned i = 0; i < colors.Size(); i += 2) {
        int c = integer(colors[i]), weight = integer(colors[i + 1]);
        require(c >= 1 && c <= 6 && weight >= 0, "unsupported color");
        l.weights[c - 1] += weight;
    }
    require(std::count_if(l.weights.begin(), l.weights.end(), [](int w){return w > 0;}) >= 3,
            "fewer than three refill colors");
    const auto &goals = field(d, "targets");
    require(goals.IsArray(), "invalid objectives");
    for (const auto &g : goals.GetArray()) {
        int count = integer(field(g, "count")), id = integer(field(g, "target"));
        if (count <= 0) continue;
        require(supportsTarget(id), "unsupported objective");
        l.objectives.push_back({id, count, count});
    }
    require(!l.objectives.empty(), "no active objective");
    const auto &boards = field(d, "boards");
    require(boards.IsArray() && boards.Size() == 1, "linked boards");
    const auto &b = boards[0];
    require(integer(field(b,"link")) == 0, "linked board");
    const auto &map = field(b,"map"), &layout = field(b,"layout"), &overlay = field(b,"layout2");
    grid(map); grid(layout);
    require(overlay.IsArray(), "invalid secondary layout");
    // layout2 stores sparse (row,column,parameter) triples in this release.
    // Nonzero parameters belong to extended actors such as locks and chains.
    if (!overlay.Empty()) {
        require(overlay.Size()%3 == 0, "invalid secondary layout");
        for (unsigned i=2;i<overlay.Size();i+=3)
            require(bits(overlay[i]) == 0, "secondary candy layer");
    }
    l.board.assign(10, std::string(9, '#'));
    l.gems.assign(10, std::string(9, '.'));
    for (unsigned r = 0; r < 10; ++r) {
        for (unsigned c = 0; c < 9; ++c) {
            uint32_t mask = bits(map[r][c]), v = bits(layout[r][c]);
            CellSetup s;
            l.originalCells.push_back(s);
            auto &cell = l.originalCells.back();
            require((mask & ~0x0f0fff0bU) == 0, "directional, portal or conveyor cell");
            cell.passThrough = (mask & 2) != 0;
            if (!(mask & 1)) continue;
            l.board[r][c] = '.';
            cell.exit = (mask & 8) != 0;
            cell.portalIn = (mask >> 8) & 63;
            cell.portalOut = (mask >> 14) & 63;
            cell.hidden = (mask >> 24) & 15;
            require(cell.hidden <= 6, "unknown buried item");
            require((v & 0x00f00000U) == 0, "chain or special layer");
            cell.frost = (v >> 16) & 3;
            cell.cage = (v >> 18) & 3;
            cell.initial = v & 255; cell.color = (v >> 8) & 15;
            cell.bottom = (v >> 12) & 15; cell.variant = v >> 24;
            require(cell.bottom <= 7 && cell.color <= 6, "unknown floor or color");
            switch (cell.initial) {
            case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 9: case 255:
                break;
            case 11: require(cell.variant == 1 || cell.variant == 2, "unknown drop variant"); break;
            case 10: break; // immovable stone
            case 15: require(cell.frost || cell.cage,"uncovered compass"); break;
            case 12: require(cell.variant >= 1 && cell.variant <= 3,"unknown box strength"); break;
            case 22: require(cell.variant >= 1 && cell.variant <= 5,"unknown bear strength"); break;
            default: require(false, "unsupported piece");
            }
        }
    }
    std::array<int,64> entrances;
    entrances.fill(-1);
    for (size_t i=0; i<l.originalCells.size(); ++i) {
        int id=l.originalCells[i].portalIn;
        if (id) {
            require(entrances[id]==-1,"ambiguous portal");
            entrances[id]=static_cast<int>(i);
        }
    }
    for (auto &cell:l.originalCells)
        if (cell.portalOut) {
            require(entrances[cell.portalOut]>=0,"unpaired portal");
            cell.upstreamPortal=entrances[cell.portalOut];
        }
    // The first two numbers are metadata. The remainder lists row/column pairs.

    if (d.HasMember("tutorials") && d["tutorials"].IsArray())
        for (const auto &t : d["tutorials"].GetArray()) {
            if (!t.IsObject() || !t.HasMember("num") || !t["num"].IsArray() ||
                !t.HasMember("hand_cell") || !t.HasMember("hand_dir")) continue;
            const auto &cells = t["num"];
            int hand = integer(t["hand_cell"]), dir = integer(t["hand_dir"]);
            if (hand < 0 || cells.Size()<4 || unsigned(hand) >= (cells.Size()-2)/2) continue;
            int r = integer(cells[2+2*hand]), c = integer(cells[3+2*hand]);
            if (r<0 || r>=10 || c<0 || c>=9) continue;
            int a = r*9+c;
            // Direction bit values are verified against the authored opening swaps.
            int off = dir == 1 ? 9 : dir == 4 ? -1 : 0;
            if (off) l.tutorialSwaps.emplace_back(a, a+off);
        }
    return l;
}
} // namespace ice
