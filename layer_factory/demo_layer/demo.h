/*
 * Copyright (c) 2015-2021 Valve Corporation
 * Copyright (c) 2015-2021 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Mark Lobodzinski <mark@lunarg.com>
 */

#pragma once

#include <unordered_map>
#include <set>
#include "vulkan/vulkan.h"
#include "vk_layer_logging.h"
#include "layer_factory.h"

struct MemoryState {
    std::vector<char> bytes;
    void* srcAddress;

    MemoryState(void* data, size_t length) {
        srcAddress = data;
        bytes.resize(length);
    }

    void Remember() { memcpy(bytes.data(), srcAddress, bytes.size()); }

    void Check() {
        if (memcmp(srcAddress, bytes.data(), bytes.size()) != 0) {
            int nothing = 0;
            nothing++;
        }
    }
};

class MemDemo : public layer_factory {
   public:
    // Constructor for state_tracker
    MemDemo() : number_mem_objects_(0), total_memory_(0), present_count_(0){};

    VkResult PostCallMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size,
                                       VkMemoryMapFlags flags, void** ppData, VkResult result) override;
    void PostCallUnmapMemory(VkDevice device, VkDeviceMemory memory) override;

    VkResult PostCallBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset,
                                              VkResult result) override;

    VkResult PostCallDebugMarkerSetObjectNameEXT(VkDevice device, const VkDebugMarkerObjectNameInfoEXT* pNameInfo,
                                                         VkResult result) override;

    void PostCallDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) override;

    void PostCallCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount,
                                       const VkBufferCopy* pRegions) override;

    VkResult PostCallQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence,
                                         VkResult result) override;

    VkResult PreCallGetFenceStatus(VkDevice device, VkFence fence) override;;

    VkResult PostCallResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags, VkResult result) override;

    void PostCallDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator) override;;

    VkResult PostCallResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkResult result) override;;

    VkResult PreCallQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo);

   private:
    uint32_t number_mem_objects_;
    VkDeviceSize total_memory_;
    uint32_t present_count_;
    std::unordered_map<VkDeviceMemory, VkDeviceSize> mem_size_map_;

    std::unordered_map<VkBuffer, uint8_t*> staticJointsmatricesBuffers;
    std::mutex staticJointsMatricesBuffersMutex;

    std::unordered_map<VkCommandBuffer, std::vector<MemoryState>> unsubmittedMemoryStates;
    std::mutex unsubmittedMemoryStatesMutex;

    std::unordered_map<VkFence, std::vector<MemoryState>> submittedMemoryStates;
    std::mutex submittedMemoryStatesMutex;

    std::unordered_map<VkDeviceMemory, uint8_t*> mappedMemories;
    std::mutex mappedMemoriesMutex;

    std::unordered_map<VkBuffer, uint8_t*> mappedBuffers;
    std::mutex mappedBuffersMutex;
};
