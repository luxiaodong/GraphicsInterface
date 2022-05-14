//
//  main.cpp
//  Vulkan_00
//
//  Created by luxiaodong on 2022/5/13.
//

#include "HelloTriangle/00_base_code.cpp"

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
