#include "Interface.h"
#include "OriginalLevel.h"
#include <sstream>
#include <unordered_map>
#include "audio/include/AudioEngine.h"
#include "json/document.h"
#include <algorithm>
#include <cmath>
USING_NS_CC;
using Audio = cocos2d::experimental::AudioEngine;
namespace gameUI {
namespace {
int musicId = -1;
std::string track;
std::string selected;
std::map<std::string, std::string> words;
const char *font() {
    const auto locale = lang();
    if (locale == "en") return "Font/FREDOKAONE-REGULAR.TTF";
    if (originalCampaign()) {
        if (locale == "ja") return "fonts/NotoSansCJK-JP-Regular.otf";
        if (locale == "zh-Hans") return "fonts/NotoSansCJK-SC-Regular.otf";
        if (locale == "zh-Hant") return "fonts/NotoSansCJK-TC-Regular.otf";
    }
    return "Font/interface.otf";
}
void loadWords() {
    if (selected.empty())
        selected = UserDefault::getInstance()->getStringForKey("language", "en");
    std::string raw = FileUtils::getInstance()->getStringFromFile("text/" + selected + ".json");
    rapidjson::Document d;
    d.Parse(raw.c_str());
    words.clear();
    if (d.IsObject())
        for (auto it = d.MemberBegin(); it != d.MemberEnd(); ++it) {
            if (it->value.IsString())
                words[it->name.GetString()] = it->value.GetString();
        }
}
// Sprite names in the supplied TexturePacker sheets include their original directory.
struct AtlasFrame {
    std::string sheet;
    std::string frame;
};
Sprite *originalSprite(const std::string &file) {
    if (file.compare(0,7,"images/") == 0) return Sprite::create(file);
    if (file.size() > 4 && file.substr(file.size()-4) == ".jpg")
        return Sprite::create("images/background/" + file);
    static const auto frames = [] {
        std::unordered_map<std::string,AtlasFrame> index;
        for (const char *name : {"img_candy", "img_UI", "img_target", "img_atlas2", "map", "img_tex_map"}) {
            std::string sheet = std::string("images/plist/") + name + ".plist";
            const auto metadata = FileUtils::getInstance()->getValueMapFromFile(sheet);
            auto group = metadata.find("frames");
            if (group == metadata.end()) continue;
            for (const auto &entry : group->second.asValueMap()) {
                const auto &path = entry.first;
                index.emplace(path.substr(path.find_last_of('/')+1), AtlasFrame{sheet,path});
            }
        }
        return index;
    }();
    auto found = frames.find(file);
    if (found == frames.end()) return nullptr;
    auto cache = SpriteFrameCache::getInstance();
    cache->addSpriteFramesWithFile(found->second.sheet);
    auto frame = cache->getSpriteFrameByName(found->second.frame);
    return frame ? Sprite::createWithSpriteFrame(frame) : nullptr;
}
void roundRect(DrawNode *d, float x, float y, float w, float h, float radius, Color4F fill) {
    Vec2 points[36];
    const float pi = 3.14159265f;
    radius = std::min(radius, std::min(w, h) * .5f);
    const Vec2 centers[] = {{x + w - radius, y + h - radius},
                            {x + radius, y + h - radius},
                            {x + radius, y + radius},
                            {x + w - radius, y + radius}};
    for (int k = 0; k < 4; ++k)
        for (int j = 0; j < 9; ++j) {
            float a = (k + j / 8.f) * pi * .5f;
            points[k * 9 + j] = centers[k] + Vec2(std::cos(a), std::sin(a)) * radius;
        }
    d->drawSolidPoly(points, 36, fill);
}
} // namespace
bool originalCampaign() {
    return UserDefault::getInstance()->getBoolForKey("original.selected", true);
}
void selectCampaign(bool original) {
    auto u=UserDefault::getInstance();
    u->setBoolForKey("original.selected",original);
    u->flush();
}
static const std::vector<std::string> &catalog() {
    static const auto paths=[] {
        std::istringstream file(FileUtils::getInstance()->getStringFromFile("original-index.txt"));
        std::vector<std::string> out;
        std::string path;
        while (file >> path) out.push_back(path);
        return out;
    }();
    return paths;
}
static const std::vector<ice::Level> &customLevels() {
    static const auto data =
        ice::loadCampaign(FileUtils::getInstance()->getStringFromFile("campaign.json"));
    return data;
}
int levelCount() {
    return originalCampaign() ? int(catalog().size()) : int(customLevels().size());
}
const ice::Level &levelAt(int stage) {
    if (!originalCampaign()) return customLevels().at(stage-1);
    static int cached = -1;
    static ice::Level level;
    if (cached != stage) {
        level = ice::loadOriginalLevel(FileUtils::getInstance()->getStringFromFile(catalog().at(stage-1)));
        cached = stage;
    }
    return level;
}
std::string stageLabel(int stage) {
    if (!originalCampaign()) return std::to_string(stage);
    const auto &path=catalog().at(stage-1);
    auto slash=path.find('/');
    auto id=path.substr(slash+1,path.rfind('.')-slash-1);
    return path[5]=='/' ? id : path.substr(5,1)+"-"+id;
}
std::string lang() {
    if (selected.empty())
        loadWords();
    return selected;
}
void language(const std::string &id) {
    selected = id;
    UserDefault::getInstance()->setStringForKey("language", id);
    UserDefault::getInstance()->flush();
    loadWords();
}
std::string tr(const std::string &key) {
    if (words.empty())
        loadWords();
    auto it = words.find(key);
    return it == words.end() ? key : it->second;
}
Label *label(Node *parent, const std::string &value, float size, float x, float y, Color3B color) {
    auto l = Label::createWithTTF(value, font(), size);
    if (!l)
        l = Label::createWithSystemFont(value, "sans-serif", size);
    l->setPosition(x, y);
    l->setTextColor(Color4B(color));
    parent->addChild(l);
    return l;
}
Sprite *picture(Node *parent, const std::string &file, float x, float y, float w, float h) {
    auto p = originalCampaign() ? originalSprite(file) : nullptr;
    if (!p) p = Sprite::create("art/" + file);
    if (!p)
        return nullptr;
    auto size = p->getContentSize();
    p->setScale(w / size.width, h ? h / size.height : w / size.width);
    p->setPosition(x, y);
    parent->addChild(p);
    return p;
}
void back(Node *parent, const std::string &file) {
    auto p = picture(parent, file, 360, 640, 720, 1280);
    if (p)
        p->setLocalZOrder(-10);
}
DrawNode *panel(Node *parent, float x, float y, float w, float h, Color4F fill, float radius) {
    auto d = DrawNode::create();
    d->setPosition(x, y);
    parent->addChild(d);
    roundRect(d, 0, 0, w, h, radius, fill);
    return d;
}
MenuItem *button(Node *parent, const std::string &text, float x, float y, float w, float h,
                 const std::function<void()> &action, bool primary) {
    auto makeFace = [&](bool pressed) {
        auto face = Node::create();
        face->setContentSize(Size(w, h));
        auto skin = originalCampaign()
            ? picture(face, primary ? "btn_8.png" : "btn_6.png", w/2, h/2, w, h)
            : nullptr;
        if (skin) {
            if (pressed) skin->setColor(Color3B(195,195,195));
        } else {
            panel(face, 0, 0, w, h, primary ? Color4F(.05,.38,.29,1) : Color4F(.045,.16,.31,.95), h/2);
            panel(face, 2, pressed ? 1 : 5, w-4, h-6,
                  primary ? Color4F(.22,.76,.36,1) : Color4F(.12,.34,.52,.98), h/2);
        }
        auto l = label(face, text, std::min(35.f, h * .43f), w / 2, h * .53f);
        if (skin) l->enableOutline(Color4B(25,69,88,255), 2);
        if (l->getContentSize().width > w * .86f)
            l->setScale(w * .86f / l->getContentSize().width);
        return face;
    };
    auto item = MenuItemSprite::create(makeFace(false), makeFace(true), [action](Ref *) {
        effect("Click-Button");
        action();
    });
    auto m = Menu::create(item, nullptr);
    m->setPosition(x, y);
    parent->addChild(m);
    return item;
}
bool sound() {
    return UserDefault::getInstance()->getBoolForKey("sound", true);
}
void music(bool map) {
    std::string path = map ? "music/music_map.mp3" : "music/music_game.mp3";
    if (path == track && musicId != -1 && sound())
        return;
    if (musicId != -1)
        Audio::stop(musicId);
    musicId = -1;
    track = path;
    if (sound())
        musicId = Audio::play2d(track, true, .28f);
}
void toggleSound() {
    auto *u = UserDefault::getInstance();
    u->setBoolForKey("sound", !sound());
    u->flush();
    music(track == "music/music_map.mp3");
}
void effect(const std::string &file) {
    if (sound())
        Audio::play2d("music/" + file + ".mp3", false, .65f);
}
int unlocked() {
    if (originalCampaign()) {
        const auto &list=catalog();
        auto last=UserDefault::getInstance()->getStringForKey("original.unlocked",list.front());
        auto it=std::find(list.begin(),list.end(),last);
        return it==list.end() ? 1 : int(it-list.begin())+1;
    }
    return std::min(levelCount(),
                    std::max(1, UserDefault::getInstance()->getIntegerForKey("unlocked", 1)));
}
static std::string scoreKey(int stage,const char *field) {
    if (originalCampaign()) return "original."+std::string(field)+"."+stageLabel(stage);
    return StringUtils::format("%s.%d",field,stage);
}
int best(int stage, const char *field) {
    return UserDefault::getInstance()->getIntegerForKey(
        scoreKey(stage,field).c_str(), 0);
}
void saveWin(int stage, int count, int score) {
    auto u = UserDefault::getInstance();
    u->setIntegerForKey(scoreKey(stage,"stars").c_str(),
                        std::max(count, best(stage, "stars")));
    u->setIntegerForKey(scoreKey(stage,"score").c_str(),
                        std::max(score, best(stage, "score")));
    int next=std::max(unlocked(),std::min(stage+1,levelCount()));
    if (originalCampaign()) u->setStringForKey("original.unlocked",catalog().at(next-1));
    else u->setIntegerForKey("unlocked",next);
    u->flush();
}
void go(Scene *scene) {
    Director::getInstance()->replaceScene(TransitionFade::create(.18f, scene, Color3B(21, 60, 100)));
}
} // namespace gameUI
