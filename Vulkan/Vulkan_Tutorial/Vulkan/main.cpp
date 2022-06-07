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
//#include "HelloTriangle/09_shader_modules.cpp"
//#include "HelloTriangle/10_fixed_functions.cpp"
//#include "HelloTriangle/11_render_passes.cpp"
//#include "HelloTriangle/12_conclusion.cpp"
//#include "HelloTriangle/13_framebuffers.cpp"
//#include "HelloTriangle/14_command_buffers.cpp"
//#include "HelloTriangle/15_rendering_presentation.cpp"
//#include "HelloTriangle/16_frames_in_flight.cpp"
//#include "HelloTriangle/16_hello_triangle.cpp"
//#include "HelloTriangle/17_swap_chain_recreation.cpp"
//#include "HelloTriangle/18_vertex_input.cpp"
//#include "HelloTriangle/19_vertex_buffer.cpp"
//#include "HelloTriangle/20_staging_buffer.cpp"
//#include "HelloTriangle/21_index_buffer.cpp"
//#include "HelloTriangle/22_descriptor_layout.cpp"
#include "HelloTriangle/23_image_texture.cpp"

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

//#include <iostream>
//using namespace std;
//
//int main ()
//{
//    const int i = 10;
//    int *p = const_cast<int*>(&i);
//    cout <<" i address is "<< &i << endl;
//    cout <<" i value   is "<< i << endl;
//    cout <<" p address is "<< &p << endl;
//    cout <<" p value   is "<< p << endl;
//    cout <<"*p value   is "<< *p << endl;
//
//    cout << "=================================" << endl;
//    *p = 20;
//
//    cout <<" i address is "<< &i << endl;
//    cout <<" i value   is "<< i << endl;
//    cout <<" p address is "<< &p << endl;
//    cout <<" p value   is "<< p << endl;
//    cout <<"*p value   is "<< *p << endl;
//}

