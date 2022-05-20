//
//  main.cpp
//  Vulkan_00
//
//  Created by luxiaodong on 2022/5/13.
//

//#include "HelloTriangle/01_base_code.cpp"
//#include "HelloTriangle/02_instance.cpp"
//#include "HelloTriangle/03_validation_layers.cpp"
//#include "HelloTriangle/03_validation_layers_message_callback.cpp"
//#include "HelloTriangle/04_physical_devices.cpp"
//#include "HelloTriangle/04_physical_devices_logic_devices_queue.cpp"
//#include "HelloTriangle/05_window_surface.cpp"
//#include "HelloTriangle/06_swap_chain.cpp"
//#include "HelloTriangle/07_image_views.cpp"
//#include "HelloTriangle/08_graphics_pipeline.cpp"
#include "HelloTriangle/09_shader_modules.cpp"

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
