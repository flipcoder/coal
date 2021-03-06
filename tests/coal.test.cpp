#include <catch.hpp>
#include <string>
#include <memory>
#include "coal/coal.h"
using namespace std;

TEST_CASE("sound","[sound]") {
    SECTION("play"){
        Coal coal;
        coal::Space space(512);
        //space.delay = true;
        
        auto buffer = std::make_shared<coal::Buffer>("test.wav");
        auto listener = std::make_shared<coal::Listener>();
        auto source = std::make_shared<coal::Source>();
        source->add(buffer);
        space.add(source);

        source->play();

        while(source->playing)
            space.update();
    }
}

TEST_CASE("stream","[stream]") {
    SECTION("stream"){
        Coal coal;
        coal::Space space;
        
        auto stream = std::make_shared<coal::Stream>("test2.ogg");
        //auto listener = std::make_shared<coal::Listener>();
        auto source = std::make_shared<coal::Source>();
        source->add(stream);
        space.add(source);

        source->play();
        //stream->loop = true;

        while(source->playing)
            space.update();
    }
}

TEST_CASE("test","[test]") {
    SECTION("test"){
        
    }
}

