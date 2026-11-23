// Run from the project root. Input is one authored asset path per line in map order.
// Redirect stdout to Resources/original-index.txt. Reasons for omissions go to stderr.
#include "OriginalLevel.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
int main() {
    std::map<std::string,int> reasons;
    std::string path;
    int usable=0;
    while (std::getline(std::cin,path)) {
        std::ifstream f("Resources/"+path);
        if (!f) { std::cerr<<"Missing "<<path<<"\n"; return 1; }
        std::string raw{std::istreambuf_iterator<char>(f),{}};
        try {
            ice::loadOriginalLevel(raw);
            std::cout<<path<<"\n";
            ++usable;
        } catch (const std::invalid_argument &e) { ++reasons[e.what()]; }
    }
    std::cerr<<usable<<" enabled\n";
    for (const auto &r:reasons) std::cerr<<r.second<<"  "<<r.first<<"\n";
}
