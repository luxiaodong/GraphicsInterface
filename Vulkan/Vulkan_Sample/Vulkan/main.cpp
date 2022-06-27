
#include "sample/triangle/triangle.h"

int main(int argc, const char * argv[])
{
    Triangle app("hello triangle");
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
