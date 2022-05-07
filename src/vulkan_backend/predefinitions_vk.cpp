#define VK_BACKEND
#include "predefinitions_vk.h"

std::vector<memory_description_tgfx> memdescs;
queue_id presentationqueue = nullptr;
queue_id allgraphicsqueue;
texture_tgfx_handle swapchaintextures[2] = {};


//These are used only in vk backend
//Don't access these outside of the project
namespace backend_vk_private {
    VkApplicationInfo Application_Info = {};
    VkInstance vkinst = VK_NULL_HANDLE;

    //GPU
    VkDevice logicaldevice = VK_NULL_HANDLE;
    VkPhysicalDevice physicaldevice = VK_NULL_HANDLE;
    VkPhysicalDeviceFeatures physicaldevice_features = {};
    std::vector<VkQueueFamilyProperties> queuefam_props;
    VkPhysicalDeviceMemoryProperties memory_props = {};
    std::vector<queuefam_vk*> queuefams;
    VkPhysicalDeviceDescriptorIndexingProperties descindexinglimits;
    VkPhysicalDeviceInlineUniformBlockPropertiesEXT uniformblocklimits;


    //Window
    GLFWwindow* window_glfwhandle = nullptr;
    VkSurfaceKHR window_surface = VK_NULL_HANDLE;
    VkSwapchainKHR window_swapchain = VK_NULL_HANDLE;


    unsigned int thread_count = 0;
}
void printer(result_tgfx result, const char* text) {

}
#include <string>
void ThrowIfFailed(VkResult result, const char* text) {
    if (result != VK_SUCCESS) {
        std::exception((std::to_string(result) + text).c_str());
    }
}