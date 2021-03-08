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

#include "demo.h"
#include <sstream>

static uint32_t display_rate = 60;

VkResult MemDemo::PostCallMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size,
                                    VkMemoryMapFlags flags, void** ppData, VkResult result) {
    // Assuming zero offset
    if(result == VK_SUCCESS && offset == 0) {
        std::lock_guard<std::mutex> lock(mappedMemoriesMutex);
        mappedMemories.emplace(memory, static_cast<uint8_t*>(*ppData));
    }
    return result;
}

void MemDemo::PostCallUnmapMemory(VkDevice device, VkDeviceMemory memory)
{
    std::lock_guard<std::mutex> lock(mappedMemoriesMutex);
    mappedMemories.erase(memory);
}

VkResult MemDemo::PostCallBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset,
                                           VkResult result) {
    // Assuming bind of seeked buffer is after vkMapMemory
    if (result == VK_SUCCESS) {
        std::lock_guard<std::mutex> lock(mappedMemoriesMutex);
        auto mappedMemory = mappedMemories.find(memory);
        if (mappedMemory != mappedMemories.end()) {
            std::lock_guard<std::mutex> lock(mappedBuffersMutex);
            mappedBuffers.emplace(buffer, mappedMemory->second /*memory pointer*/);
        }
    }
    return result;
}

VkResult MemDemo::PostCallDebugMarkerSetObjectNameEXT(VkDevice device, const VkDebugMarkerObjectNameInfoEXT* pNameInfo,
                                                      VkResult result) {
    if (result == VK_SUCCESS) {
        if (pNameInfo->objectType == VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT) {
            if (strcmp("staticJointsMatrices", pNameInfo->pObjectName) == 0) {
                VkBuffer buffer = (VkBuffer)pNameInfo->object;
                std::lock_guard<std::mutex> lock(mappedBuffersMutex);
                auto bufferToMemory = mappedBuffers.find(buffer);
                if (bufferToMemory != mappedBuffers.end())
                {
                    uint8_t* mappedMemoryPtr = bufferToMemory->second;
                    std::lock_guard<std::mutex> lock(staticJointsMatricesBuffersMutex);
                    staticJointsmatricesBuffers.emplace(buffer, mappedMemoryPtr);
                }
            }
        }
        PostCallApiFunction("vkDebugMarkerSetObjectNameEXT", result);
    }
    return result;
}

void MemDemo::PostCallDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) {
    std::lock_guard<std::mutex> lock(staticJointsMatricesBuffersMutex);
    staticJointsmatricesBuffers.erase(buffer);
}

void MemDemo::PostCallCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount,
                                    const VkBufferCopy* pRegions)
{
    std::lock_guard<std::mutex> lock(staticJointsMatricesBuffersMutex);
    auto bufferIter = staticJointsmatricesBuffers.find(srcBuffer);
    if (bufferIter != staticJointsmatricesBuffers.end()) {
        // This is staticJointsDrawMatrices, add it to CB
        std::lock_guard<std::mutex> lock(unsubmittedMemoryStatesMutex);

        unsubmittedMemoryStates[commandBuffer].push_back(MemoryState(bufferIter->second,  // data
                                                                     pRegions[0].size));
    }
}

VkResult MemDemo::PostCallQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence,
                                      VkResult result){
    VkCommandBuffer commandBuffer = pSubmits[0].pCommandBuffers[0];  // Assumption
    std::lock_guard<std::mutex> lock(unsubmittedMemoryStatesMutex);
    auto cbToMemoryState = unsubmittedMemoryStates.find(commandBuffer);
    if (cbToMemoryState != unsubmittedMemoryStates.end()) {
        // Found CB with copy to staticJointsDrawMatrices
        // Record that in fence
        assert(fence != VK_NULL_HANDLE);
        std::lock_guard<std::mutex> lock(submittedMemoryStatesMutex);
        auto insertResult = submittedMemoryStates.emplace(fence, std::move(cbToMemoryState->second));

        std::vector<MemoryState>& states = insertResult.first->second;
        for (MemoryState& s : states)
        {
            s.Remember(); // Dump memory state for later check
        }
        unsubmittedMemoryStates.erase(cbToMemoryState); // CB not needed anymore, got state in fence.
    }
    return result;
}

VkResult MemDemo::PreCallGetFenceStatus(VkDevice device, VkFence fence) {
    std::lock_guard<std::mutex> lock(submittedMemoryStatesMutex);
    auto sumbmittedMemoryStatesIt = submittedMemoryStates.find(fence);
    if (sumbmittedMemoryStatesIt != submittedMemoryStates.end())
    {
        std::vector<MemoryState>& states = sumbmittedMemoryStatesIt->second;
        for (size_t i = 0; i < states.size(); ++i) {
            states[i].Check();
        }
        submittedMemoryStates.erase(sumbmittedMemoryStatesIt);
    }
    return VK_SUCCESS;
}

VkResult MemDemo::PostCallResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags, VkResult result) {
    if (result == VK_SUCCESS)
    {
        std::lock_guard<std::mutex> lock(unsubmittedMemoryStatesMutex);
        // Forget memory state just in case it was not submitted.
        unsubmittedMemoryStates.erase(commandBuffer);
    }
    return result;
}

void MemDemo::PostCallDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator) {
    std::lock_guard<std::mutex> lock(submittedMemoryStatesMutex);
    submittedMemoryStates.erase(fence); // Just in case GetFenceStatus was not called.
}

VkResult MemDemo::PostCallResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkResult result) {
    std::lock_guard<std::mutex> lock(submittedMemoryStatesMutex);
    for (uint32_t i = 0; i < fenceCount; ++i)
    {
        submittedMemoryStates.erase(pFences[i]);  // Just in case GetFenceStatus was not called.
    }
    return VK_SUCCESS;
}

VkResult MemDemo::PreCallQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo) {
/*
    present_count_++;
    if (present_count_ >= display_rate) {
        present_count_ = 0;

        std::stringstream message;
        message << "Memory Allocation Count: " << number_mem_objects_ << "\n";
        message << "Total Memory Allocation Size: " << total_memory_ << "\n\n";

        // Various text output options:
        // Call through simplified interface
        MemDemo::Information(message.str());

#ifdef _WIN32
        // On Windows, call OutputDebugString to send output to the MSVC output window or debug out
        std::string str = message.str();
        LPCSTR cstr = str.c_str();
        OutputDebugStringA(cstr);
#endif

        // Option 3, use printf to stdout
        printf("Demo layer: %s\n", message.str().c_str());
    }*/

    return VK_SUCCESS;
}

MemDemo demo_mem_layer;
