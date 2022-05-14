//
//  main.cpp
//  Vulkan_00
//
//  Created by luxiaodong on 2022/5/13.
//

//#include "HelloTriangle/01_base_code.cpp"
//#include "HelloTriangle/02_instance.cpp"
#include "HelloTriangle/03_validation_layers.cpp"

int main(int argc, const char * argv[])
{
    HelloTriangleApplication app;
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
