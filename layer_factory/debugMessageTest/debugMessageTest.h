#pragma once

#include "vulkan/vulkan.h"
#include "vk_layer_logging.h"
#include "layer_factory.h"

class DebugMessageTest : public layer_factory {
   public:
    DebugMessageTest() {}

   private:
    void PreCallApiFunction(const char *api_name) override;

    VkResult PostCallCreateDebugUtilsMessengerEXT(VkInstance instance,
                                                          const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                                          const VkAllocationCallbacks* pAllocator,
                                                          VkDebugUtilsMessengerEXT* pMessenger, VkResult result) override;

    void PreCallDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger,
                                                      const VkAllocationCallbacks* pAllocator) override;

  private:
    PFN_vkDebugUtilsMessengerCallbackEXT pfnUserCallback = nullptr;
    void* pUserData = nullptr;
    VkDebugUtilsMessengerEXT mMessenger = VK_NULL_HANDLE;
    uint64_t callCounter = 0;
};
