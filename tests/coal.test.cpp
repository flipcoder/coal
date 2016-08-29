#include <catch.hpp>
#include <string>
#include <memory>
#include "coal/coal.h"
using namespace std;

TEST_CASE("coal","[coal]") {
    SECTION("basic usage"){
        Coal coal;
        coal::Space space;
        
        auto buffer = std::make_shared<coal::Buffer>("test.wav");
        auto listener = std::make_shared<coal::Listener>();
        cout << buffer->buffer.size() << endl;
        auto source = std::make_shared<coal::Source>();
        source->add(buffer);
        space.add(listener, source);
        source->play();

        //while(source->playing)
        //    space.update();
    }
}

