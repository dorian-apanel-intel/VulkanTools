#include "debugMessageTest.h"

#include <sstream>

void DebugMessageTest::PreCallApiFunction(const char *api_name) {
    if (pfnUserCallback) {
        std::stringstream message;
        message << callCounter++ << " " << api_name;
        std::string messageStr = message.str();

        VkDebugUtilsMessengerCallbackDataEXT data;
        data.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
        data.pNext = nullptr;
        data.flags = 0;
        data.pMessageIdName = "MessageID";
        data.messageIdNumber = 12345678;
        data.pMessage = messageStr.c_str();
        data.queueLabelCount = 0;
        data.pQueueLabels = nullptr;
        data.cmdBufLabelCount = 0;
        data.pCmdBufLabels = nullptr;
        data.objectCount = 0;
        data.pObjects = nullptr;

        pfnUserCallback(
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
            &data,
            pUserData);
    }
}

VkResult DebugMessageTest::PostCallCreateDebugUtilsMessengerEXT(VkInstance instance,
                                                                const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                                                const VkAllocationCallbacks* pAllocator,
                                                                VkDebugUtilsMessengerEXT* pMessenger, VkResult result) {
    if (result == VK_SUCCESS && pfnUserCallback == nullptr) {
        pfnUserCallback = pCreateInfo->pfnUserCallback;
        pUserData = pCreateInfo->pUserData;
        mMessenger = *pMessenger;
        DebugMessageTest::Information("Intercepted debug utils messenger");
    } else {
        if (result != VK_SUCCESS) DebugMessageTest::Warning("Failed to create debug utils messenger.");
        if (pfnUserCallback) DebugMessageTest::Warning("Intercepting two debug utils messenger not supported.");
    }

    return VK_SUCCESS;
}

void DebugMessageTest::PreCallDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger,
                                                            const VkAllocationCallbacks* pAllocator) {
    if (mMessenger == messenger) {
        pfnUserCallback = nullptr;
        pUserData = nullptr;
        mMessenger = VK_NULL_HANDLE;
        DebugMessageTest::Information("Removed debug utils messenger");
    } else {
        DebugMessageTest::Warning("Unexpected debug utils messenger destroy");
    }
}

DebugMessageTest debugMessageTest;