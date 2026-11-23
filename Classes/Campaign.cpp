#include "Campaign.h"
#include "json/document.h"
#include <algorithm>
#include <stdexcept>
namespace ice {
namespace {
void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
int number(const rapidjson::Value &v, int low, int high) {
    check(v.IsInt() && v.GetInt() >= low && v.GetInt() <= high, "Invalid campaign number");
    return v.GetInt();
}
} // namespace
std::vector<Level> loadCampaign(const std::string &raw) {
    rapidjson::Document doc;
    doc.Parse(raw.c_str());
    check(!doc.HasParseError() && doc.IsArray() && !doc.Empty(), "Expected a campaign array");
    std::vector<Level> out;
    for (const auto &d : doc.GetArray()) {
        check(d.IsObject(), "Invalid level");
        for (auto k : {"moves", "weights", "board", "gems", "collect", "stars"})
            check(d.HasMember(k), "Missing level field");
        Level l;
        l.moves = number(d["moves"], 1, 999);
        check(d["weights"].IsArray() && d["weights"].Size() == 6, "Expected six gem weights");
        for (int i = 0; i < 6; ++i)
            l.weights[i] = number(d["weights"][i], 0, 100);
        check(std::count_if(l.weights.begin(), l.weights.end(), [](int n) { return n > 0; }) >= 3,
              "Enable three or more colors");
        check(d["stars"].IsArray() && d["stars"].Size() == 2, "Expected two star thresholds");
        l.stars[0] = number(d["stars"][0], 1, 10000000);
        l.stars[1] = number(d["stars"][1], l.stars[0], 10000000);
        check(d["collect"].IsObject(), "Invalid collection goal");
        for (auto it = d["collect"].MemberBegin(); it != d["collect"].MemberEnd(); ++it) {
            std::string k = it->name.GetString();
            check(k.size() == 1 && k[0] >= '1' && k[0] <= '6', "Invalid goal color");
            int color = k[0] - '1';
            check(l.weights[color] > 0, "Target color has zero weight");
            l.collect[color] = number(it->value, 1, 999);
        }
        const auto &rows = d["board"];
        const auto &gems = d["gems"];
        check(rows.IsArray() && rows.Size() >= 3 && rows.Size() <= 10 && gems.IsArray() &&
                  gems.Size() == rows.Size(),
              "Invalid board height");
        bool hasIce = false, hasPair = false;
        for (unsigned i = 0; i < rows.Size(); ++i) {
            check(rows[i].IsString() && gems[i].IsString(), "Expected board rows");
            std::string r = rows[i].GetString(), g = gems[i].GetString();
            check(r.size() >= 3 && r.size() <= 9 && g.size() == r.size() &&
                      (i == 0 || r.size() == l.board[0].size()),
                  "Invalid board width");
            for (size_t j = 0; j < r.size(); ++j) {
                check(r[j] == '#' || r[j] == '.' || r[j] == '1' || r[j] == '2',
                      "Unknown floor character");
                check(g[j] == '.' || (g[j] >= '1' && g[j] <= '6' && r[j] != '#'), "Invalid opening gem");
                hasIce |= r[j] == '1' || r[j] == '2';
                hasPair |= r[j] != '#' && ((j && r[j - 1] != '#') || (i && l.board.back()[j] != '#'));
            }
            l.board.push_back(r);
            l.gems.push_back(g);
        }
        check(hasPair, "Board needs adjacent cells");
        check(hasIce || !d["collect"].ObjectEmpty(), "Level needs a goal");
        out.push_back(l);
    }
    return out;
}
} // namespace ice
