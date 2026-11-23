#include "MapFeatures.h"
#include "Interface.h"
#include "RewardStore.h"
#include "editor-support/cocostudio/ActionTimeline/CSLoader.h"
#include "editor-support/spine/SkeletonAnimation.h"
#include "editor-support/spine/AttachmentVertices.h"
#include "ui/CocosGUI.h"
#include "audio/include/AudioEngine.h"
#include <ctime>

USING_NS_CC;
namespace {
using gameUI::tr;
ui::Button *button(Node *root,const std::string &name) {
    return static_cast<ui::Button *>(root->getChildByName(name));
}
void bind(Node *root,const std::string &name,std::function<void()> fn) {
    auto b=button(root,name);
    b->setTouchEnabled(true);
    b->addClickEventListener([fn](Ref *) { gameUI::effect("Click-Button"); fn(); });
}
void caption(Node *root,const std::string &name,const std::string &text) {
    auto label=static_cast<ui::Text *>(root->getChildByName(name));
    label->setFontName(gameUI::font());
    label->setString(text);
}
std::string number(int n) { return std::to_string(n); }
std::string unlock(int n) { return tr("unlockAt")+" "+number(n); }
const char *icons[]={"Item_1.png","Item_2.png","Item_line.png","Item_3.png"};
void prize(Node *root, ice::Prize p) {
    root->setVisible(true);
    root->getChildByName("energy")->setVisible(false);
    root->getChildByName("coin")->setVisible(p.item==8);
    auto icon=static_cast<Sprite *>(root->getChildByName("booster"));
    icon->setVisible(p.item!=8);
    if (p.item!=8) icon->setSpriteFrame(std::string("images/plist/img_atlas2/")+icons[p.item-4]);
    caption(root,"count","x"+number(p.count));
}
}
MapFeatures *MapFeatures::open(MapPage page,int stage,std::function<void()> close) {
    auto f=new (std::nothrow) MapFeatures;
    if (!f || !f->initWithColor(Color4B(13,36,68,184),720,1280)) { CC_SAFE_DELETE(f); return nullptr; }
    f->autorelease(); f->stage=stage; f->close=std::move(close);
    f->show(page);
    f->schedule(CC_SCHEDULE_SELECTOR(MapFeatures::refresh),0.5f);
    return f;
}
void MapFeatures::back() {
    if(page==MapPage::Video) market::leaveVideo();
    if (page==MapPage::Supplies) {show(fromPet ? MapPage::Pets : MapPage::Shop); return;}
    if (page==MapPage::Coins && coinsFromShop) {coinsFromShop=false; show(MapPage::Shop);return;}
    auto returnToMap=close;
    returnToMap();
}
void MapFeatures::note(const std::string &s) {
    if (!notice) return;
    notice->setString(s);
    float w=notice->getContentSize().width;
    notice->setScale(w>660 ? 660/w : 1);
}
void MapFeatures::show(MapPage next) {
    removeAllChildren();
    layout=nullptr; notice=nullptr; penguin=nullptr; baseHat=nullptr; wornHat=nullptr; feedback.clear(); page=next;
    int gate=page==MapPage::Daily ? 5 : page==MapPage::Pets ? 6 : 0;
    if (stage<gate) {
        gameUI::panel(this,75,485,570,310,Color4F(.04f,.19f,.35f,.98f));
        gameUI::label(this,unlock(gate),40,360,690);
        gameUI::button(this,tr("back"),360,570,255,78,[this]{back();});
        return;
    }
    if (page==MapPage::Supplies) {supplies(); return;}
    const char *file=page==MapPage::Daily ? "dialog/DailyBonusDlg.csb" :
        page==MapPage::Video ? "dialog/WatchVideoDlg.csb" : page==MapPage::Shop ? "dialog/BuyOnetimeDlg.csb" :
        page==MapPage::Coins ? "dialog/CoinShop.csb" : "pets/PetPlayScene.csb";
    layout=CSLoader::createNode(file);
    addChild(layout);
    notice=gameUI::label(this,"",29,360,page==MapPage::Pets ? 396 : 80);
    notice->enableOutline(Color4B(20,45,72,255),3);
    if (page==MapPage::Pets) {pets(); return;}
    if (page==MapPage::Daily || page==MapPage::Video) {
        std::string spineFile=page==MapPage::Daily ? "spine/DailyBonusDlg" : "spine/WatchVideoDlg";
        auto mascot=spine::SkeletonAnimation::createWithJsonFile(spineFile+".json",spineFile+".atlas");
        layout->getChildByName("Node_1")->addChild(mascot);
        mascot->setAnimation(0,"idle",true);
    }
    bind(layout,"btn_close",[this]{back();});
    if (page==MapPage::Daily) {
        caption(layout,"Text_1",tr("dailyBonus"));
        for (int i=0;i<7;++i) {
            auto day=layout->getChildByName("day"+number(i+1));
            prize(day->getChildByName("item1"),ice::dailyPrizes[i]);
        }
        bind(layout,"btn_claim",[this] {
            int day=ice::localDay(std::time(nullptr));
            feedback=market::change([day](ice::Rewards &r){return r.claim(day);}) ? tr("received") : tr("claimLater");
            refresh(0);
        });
    } else if (page==MapPage::Video) {
        market::refreshStore();
        auto privacy=gameUI::button(layout,tr("adPrivacy"),360,150,280,64,[]{market::privacyOptions();});
        privacy->getParent()->setName("privacy");
        caption(layout,"lbl_title",tr("watchVideo"));
        layout->getChildByName("lbl_text")->setVisible(false);
        caption(layout,"lbl_coin","+"+number(ice::videoGold));
        bind(layout,"btn_watch",[this]{
            feedback=market::watch() ? "" : tr("serviceUnavailable");
            refresh(0);
        });
    } else if (page==MapPage::Shop) {
        market::refreshStore();
        caption(layout,"lbl_title",tr("shop"));
        for (int i=0;i<2;++i) {
            auto pack=layout->getChildByName("pack"+number(i+1));
            caption(pack,"lbl_name",tr(i==0 ? "starterPack" : "adventurePack"));
            prize(pack->getChildByName("item1"),{8,ice::productGold[i+6]});
            for(int j=0;j<3;++j) prize(pack->getChildByName("item"+number(j+2)),{4+j,ice::bundleBoosters[i]});
            bind(pack,"btn_buy",[this,i]{purchase(i+6);});
        }
        gameUI::button(this,tr("supplies"),195,175,280,76,[this]{fromPet=false;show(MapPage::Supplies);});
        gameUI::button(this,tr("coinPacks"),525,175,280,76,[this]{coinsFromShop=true;show(MapPage::Coins);});
    } else {
        market::refreshStore();
        caption(layout,"Text_3",tr("coinPacks"));
        layout->getChildByName("Image_3")->setVisible(false);
        layout->getChildByName("btnRemvoead")->setVisible(false);
        for(int i=0;i<6;++i) {
            auto pack=layout->getChildByName("coins")->getChildByName("pack"+number(i+1));
            caption(pack,"lbl_count",number(ice::productGold[i]));
            bind(pack,"button",[this,i]{purchase(i);});
        }
        for(int i=0;i<10;++i) layout->getChildByName("coin"+number(i))->setVisible(false);
    }
    refresh(0);
}
void MapFeatures::purchase(int index) {
    if ((index>=6 && (market::wallet().bundles & (1<<(index-6)))) ||
        market::price(index).empty()) return;
    feedback=market::buyProduct(index) ? "" : tr("serviceUnavailable");
    refresh(0);
}
void MapFeatures::refresh(float) {
    if (!layout) return;
    const auto &r=market::wallet();
    std::string status=feedback;
    if (page==MapPage::Daily) {
        int next=r.nextDay(ice::localDay(std::time(nullptr)));
        for(int i=0;i<7;++i) {
            auto day=layout->getChildByName("day"+number(i+1));
            day->getChildByName("check")->setVisible(i<(next<0 ? r.streak : next));
            day->getChildByName("light")->setVisible(i==next);
        }
        auto b=button(layout,"btn_claim");
        b->setEnabled(next>=0);
        caption(b,"label",tr(next>=0 ? "claim" : "claimed"));
        if (next<0 && status.empty()) status=tr("claimLater");
    } else if (page==MapPage::Video) {
        auto left=std::max<std::int64_t>(0,r.videoTime+3600-std::time(nullptr));
        layout->getChildByName("privacy")->setVisible(market::privacyRequired());
        bool waiting=std::string(market::status())=="serviceBusy";
        auto b=button(layout,"btn_watch");
        b->setEnabled(!left && !waiting);
        caption(b,"label",left ? StringUtils::format("%lld:%02lld",(long long)left/60,(long long)left%60) : tr("watchVideo"));
    } else if (page==MapPage::Shop || page==MapPage::Coins) {
        bool offers=page==MapPage::Shop;
        caption(layout->getChildByName(offers ? "coin_bar" : "nodeTotalGold"),offers ? "lbl_coin" : "lbl_totalGold",number(r.gold));
        for (int i=0;i<(offers?2:6);++i) {
            int index=offers ? 6+i : i;
            auto row=(offers ? layout : layout->getChildByName("coins"))->getChildByName("pack"+number(i+1));
            auto b=button(row,offers ? "btn_buy" : "button");
            auto price=market::price(index);
            bool owned=offers && (r.bundles & (1<<i));
            caption(b,offers ? "label" : "txt",owned ? tr("owned") : price.empty() ? tr("offline") : price);
            b->setEnabled(!price.empty() && !owned);
        }
    }
    if ((page==MapPage::Video || page==MapPage::Shop || page==MapPage::Coins) && status.empty())
        status=tr(market::status());
    note(status);
}
void MapFeatures::supplies() {
    gameUI::panel(this,42,205,636,885,Color4F(.03f,.18f,.32f,.98f));
    const auto &r=market::wallet();
    gameUI::label(this,tr("supplies")+"   •   "+number(r.gold)+" "+tr("coins"),40,360,1000);
    const char *names[]={"food","soap","hammer","stripe","bomb","rainbow"};
    for(int i=0;i<6;++i) {
        int count=i==0?r.food:i==1?r.soap:r.boosters[i-2];
        int price=i==0?ice::foodPrice:i==1?ice::washPrice:ice::boosterPrice;
        int y=875-i*96;
        auto label=gameUI::label(this,tr(names[i])+"  ×"+number(count),30,213,y);
        label->setScale(std::min(1.f,290/std::max(1.f,label->getContentSize().width)));
        gameUI::button(this,number(price)+" "+tr("coins"),515,y,210,67,[this,i]{
            int cost=i==0 ? ice::foodPrice : i==1 ? ice::washPrice : ice::boosterPrice;
            bool affordable=market::wallet().gold>=cost;
            bool ok=market::change([i](ice::Rewards &s){return s.buy(i);});
            show(MapPage::Supplies);
            note(tr(ok ? "received" : affordable ? "saveError" : "lowGold"));
        });
    }
    notice=gameUI::label(this,"",27,360,335);
    gameUI::button(this,tr("back"),360,260,225,68,[this]{back();});
}
void MapFeatures::pets() {
    for(auto child:layout->getChildren()) {
        const auto &name=child->getName();
        child->setVisible(name=="bg" || name=="pet_bar" || name=="top_bar" || name=="bottom_bar");
    }
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("pets/img_items.plist");
    auto tabs=layout->getChildByName("pet_bar");
    for(int i=0;i<5;++i) {
        auto b=button(tabs,number(i+1));
        b->getChildByName("reminder")->setVisible(false);
        auto frame="pets/ui/pet"+number(i+1)+(stage<ice::petUnlocks[i]?"Lock":market::wallet().pet==i?"On":"Off")+".png";
        b->loadTextureNormal(frame,ui::Widget::TextureResType::PLIST);
        bind(tabs,number(i+1),[this,i] {
            if (stage<ice::petUnlocks[i]) {feedback=unlock(ice::petUnlocks[i]);note(feedback);return;}
            if(market::change([&](ice::Rewards &r){return r.selectPet(i,stage);})) show(MapPage::Pets);
        });
    }
    layout->getChildByName("top_bar")->getChildByName("giftBox_7")->setVisible(false);
    auto bar=layout->getChildByName("bottom_bar");
    bar->getChildByName("dress")->getChildByName("new")->setVisible(false);
    bind(bar,"back",[this]{back();});
    const char *actions[]={"food","wash","dress"};
    for(int i=0;i<3;++i) bind(bar,actions[i],[this,i]{petAction(i);});
    const char *roles[]={"roleR","roleB","roleY","roleG","roleP"};
    auto name=std::string("spine/")+roles[market::wallet().pet];
    penguin=spine::SkeletonAnimation::createWithJsonFile(name+".json",name+".atlas");
    penguin->setPosition(360,390); penguin->setScale(.62f);layout->addChild(penguin);
    penguin->setAnimation(0,"idle1",true);
    auto stroke=ui::Layout::create();
    stroke->setPosition({200,480});
    stroke->setContentSize({320,370});
    stroke->setTouchEnabled(true);
    layout->addChild(stroke);
    stroke->addClickEventListener([this](Ref *) {
        penguin->setAnimation(0,"touch",false);
        penguin->addAnimation(0,"idle1",true,0);
    });
    gameUI::button(this,tr("supplies"),360,255,270,70,[this]{fromPet=true;show(MapPage::Supplies);});
    updatePet();
}
void MapFeatures::updatePet() {
    const auto &r=market::wallet();
    auto top=layout->getChildByName("top_bar");
    caption(top,"level",number(r.level));
    caption(top,"count",number(r.smiles)+" / "+number(ice::smileGoal(r.level)));
    static_cast<ui::LoadingBar*>(top->getChildByName("bar"))->setPercent(100.f*r.smiles/ice::smileGoal(r.level));
    // The red penguin deforms its headwear mesh. The other four use untrimmed quads.
    auto attachment=penguin->getAttachment("headwear","headwear");
    spine::AttachmentVertices *rendered=nullptr;
    const float *originalUV=nullptr;
    const float *hatUV=nullptr;
    static const float quad[]={1,1,0,1,0,0,1,0};
    if (attachment && attachment->type==SP_ATTACHMENT_MESH) {
        auto mesh=reinterpret_cast<spMeshAttachment *>(attachment);
        rendered=static_cast<spine::AttachmentVertices *>(mesh->rendererObject);
        hatUV=mesh->regionUVs;
        originalUV=mesh->uvs;
    } else if (attachment && attachment->type==SP_ATTACHMENT_REGION) {
        auto region=reinterpret_cast<spRegionAttachment *>(attachment);
        rendered=static_cast<spine::AttachmentVertices *>(region->rendererObject);
        hatUV=quad;
        originalUV=region->uvs;
    }
    if (rendered) {
        if (!baseHat) baseHat=rendered->_texture;
        int hat=r.hats[r.pet];
        wornHat=hat ? Director::getInstance()->getTextureCache()->addImage(
            "pets/dress/headwear/headwear"+number(hat)+".png") : nullptr;
        rendered->_texture=wornHat ? wornHat.get() : baseHat;
        const float *uv=wornHat ? hatUV : originalUV;
        for(int i=0;i<rendered->_triangles->vertCount;++i)
            rendered->_triangles->verts[i].texCoords={uv[2*i],uv[2*i+1]};
    }
    feedback=tr("food")+" ×"+number(r.food)+"     "+tr("soap")+" ×"+number(r.soap);
    note(feedback);
}
void MapFeatures::petAction(int action) {
    if(!market::change([&](ice::Rewards &r){return r.care(action,stage);})) {
        feedback=tr("restock");note(feedback);return;
    }
    if (action<2) {
        auto item=Sprite::createWithSpriteFrameName(action==0 ? "pets/item/food/1.png" : "pets/item/wash/1.png");
        item->setPosition(action==0 ? Vec2(210,150) : Vec2(360,150));
        layout->addChild(item);
        item->runAction(Sequence::create(MoveTo::create(.3f,{360,680}),FadeOut::create(.2f),RemoveSelf::create(),nullptr));
        if(gameUI::effectsEnabled()) cocos2d::experimental::AudioEngine::play2d(
            action==0 ? "pets/sound.pet.eat.mp3" : "pets/music.soap.use.mp3",false,.6f);
    }
    const char *animations[]={"eat","wash","dress"};
    penguin->setAnimation(0,animations[action],false);
    penguin->addAnimation(0,"idle1",true,0);
    updatePet();
}
