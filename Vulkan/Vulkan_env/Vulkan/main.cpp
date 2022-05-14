//
//  main.cpp
//  Vulkan_00
//
//  Created by luxiaodong on 2022/5/13.
//

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>


#include <iostream>

int main(int argc, const char * argv[]) {

//#ifdef VK_ICD_FILENAMES
//    std::cout << "OK!\n";
//#else
//    std::cout << "NOT OK!\n";
//#endif
    
    std::cout << "Hello, World!\n";
    
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(400, 300, "Vulkan window", nullptr, nullptr);
    
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,nullptr);
    std::cout << extensionCount << " extensions supported" << std::endl;
    
    glm::mat4 matrix;
    glm::vec4 vec;
    auto test = matrix * vec;
//    std::cout << test;
    
    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }
    
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
