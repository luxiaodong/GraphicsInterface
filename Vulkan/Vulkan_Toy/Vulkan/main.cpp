#include "source/shadertoy.h"

int main(int argc, const char * argv[])
{
    ShaderToy app("base.frag");
    
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
