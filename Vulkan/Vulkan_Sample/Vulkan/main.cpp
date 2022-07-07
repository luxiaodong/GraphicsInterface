
#include "sample/triangle/triangle.h"
#include "sample/pipelines/pipelines.h"

int main(int argc, const char * argv[])
{
//    Triangle app("triangle");
    Pipelines app("pipeline");
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
