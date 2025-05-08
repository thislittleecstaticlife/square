//
// utilities.c
//
//  Copyright © 2025 Robert Guequierre
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "utilities.h"

#include <stdbit.h>

//====----------------------------------------------------------------------====
//
// * Physical device
//
//====----------------------------------------------------------------------====

// * findFirstGPU
//
VkResult findFirstGPU(VkInstance instance, VkPhysicalDevice* pPhysicalDevice)
{
    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

    VkPhysicalDevice physicalDevices[physicalDeviceCount] = {};
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices);

    for (uint32_t ii = 0; ii < physicalDeviceCount; ++ii)
    {
        VkPhysicalDeviceProperties properties = {};
        vkGetPhysicalDeviceProperties(physicalDevices[ii], &properties);

        if ( VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU == properties.deviceType ||
             VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU   == properties.deviceType )
        {
            *pPhysicalDevice = physicalDevices[ii];
            return VK_SUCCESS;
        }
    };

    *pPhysicalDevice = nullptr;
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

//====----------------------------------------------------------------------====
//
// * Queue family
//
//====----------------------------------------------------------------------====

// * findGraphicsAndComputeQueueFamily
//
VkResult findGraphicsAndComputeQueueFamily( VkPhysicalDevice device,
                                            uint32_t*        pQueueFamilyIndex )
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    VkQueueFamilyProperties queueFamilyProperties[queueFamilyCount] = {};

    vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, 
                                              queueFamilyProperties );

    for (uint32_t ii = 0; ii < queueFamilyCount; ++ii)
    {
        auto const queueFlags = queueFamilyProperties[ii].queueFlags;

        if ( IS_FLAG_SET(queueFlags, VK_QUEUE_GRAPHICS_BIT) &&
             IS_FLAG_SET(queueFlags, VK_QUEUE_COMPUTE_BIT) )
        {
            *pQueueFamilyIndex = ii;
            return VK_SUCCESS;
        }
    };

    *pQueueFamilyIndex = 0;
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

//====----------------------------------------------------------------------====
//
// * Memory
//
//====----------------------------------------------------------------------====

// * hasProperties
//
bool hasProperties( const VkMemoryType*   memoryType,
                    VkMemoryPropertyFlags requestedProperties ) [[unsequenced]]
{
    return requestedProperties == (memoryType->propertyFlags & requestedProperties);
}

// * hasMemoryProperties
//
bool hasMemoryProperties
(
    const VkPhysicalDeviceMemoryProperties* memoryProperties,
    uint32_t                                memoryTypeIndex,
    VkMemoryPropertyFlags                   requestedProperties
)
    [[unsequenced]]
{
    return hasProperties( &memoryProperties->memoryTypes[memoryTypeIndex],
                          requestedProperties );
}

// * findMemoryTypeIndex
//
VkResult findMemoryTypeIndex
(
    const VkPhysicalDeviceMemoryProperties* pMemoryProperties,
    uint32_t                                memoryTypeIndexBits,
    VkMemoryPropertyFlags                   requestedProperties,
    uint32_t*                               pMemoryTypeIndex
)
{
    for ( uint32_t index = stdc_trailing_zeros(memoryTypeIndexBits),
                indexBit = 1u << index ;
          index < pMemoryProperties->memoryTypeCount; ++index, indexBit <<= 1 )
    {
        if ( IS_FLAG_SET(memoryTypeIndexBits, indexBit) &&
             hasMemoryProperties(pMemoryProperties, index, requestedProperties) )
        {
            *pMemoryTypeIndex = index;
            return VK_SUCCESS;
        }
    }

    *pMemoryTypeIndex = 0;
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

//====----------------------------------------------------------------------====
//
// * Images
//
//====----------------------------------------------------------------------====

// * createImageAndMemory
//
VkResult createImageAndMemory
(
    VkDevice                                device,
    const VkImageCreateInfo*                pImageInfo,
    const VkPhysicalDeviceMemoryProperties* pMemoryProperties,
    VkMemoryPropertyFlags                   requestedMemoryProperties,
    VkImage*                                pImage,
    VkDeviceMemory*                         pImageMemory
)
{
    VkResult       result      = VK_SUCCESS;
    VkImage        image       = nullptr;
    VkDeviceMemory imageMemory = nullptr;

    do
    {
        result = vkCreateImage(device, pImageInfo, nullptr, &image);

        if (VK_SUCCESS != result) {
            break;
        }

        //  - memory requirements
        VkMemoryRequirements memoryRequirements = {};
        vkGetImageMemoryRequirements(device, image, &memoryRequirements);

        uint32_t memoryTypeIndex = 0;

        result = findMemoryTypeIndex( pMemoryProperties,
                                      memoryRequirements.memoryTypeBits,
                                      requestedMemoryProperties,
                                      &memoryTypeIndex );
        if (VK_SUCCESS != result) {
            break;
        }

        //  - memory
        const VkMemoryAllocateInfo memoryAllocInfo = {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext           = nullptr,
            .allocationSize  = memoryRequirements.size,
            .memoryTypeIndex = memoryTypeIndex
        };

        result = vkAllocateMemory( device, &memoryAllocInfo, nullptr,
                                   &imageMemory );

        if (VK_SUCCESS != result) {
            break;
        }

        result = vkBindImageMemory(device, image, imageMemory, 0);
    }
    while (0);

    if (VK_SUCCESS != result)
    {
        vkDestroyImage(device, image, nullptr);
        image = nullptr;

        vkFreeMemory(device, imageMemory, nullptr);
        imageMemory = nullptr;
    }

    *pImageMemory = imageMemory;
    *pImage       = image;

    return result;
}

//====----------------------------------------------------------------------====
//
// * Command buffers
//
//====----------------------------------------------------------------------====

VkResult submitCommandBuffer( VkDevice        device,
                              VkQueue         queue,
                              VkCommandBuffer commandBuffer )
{
    //   - fence
    const VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };

    VkFence fence = nullptr;

    auto result = vkCreateFence(device, &fenceInfo, nullptr, &fence);

    if (VK_SUCCESS == result)
    {
        const VkSubmitInfo submitInfo = {
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                = nullptr,
            .waitSemaphoreCount   = 0,
            .pWaitSemaphores      = nullptr,
            .pWaitDstStageMask    = nullptr,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &commandBuffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores    = nullptr
        };

        result = vkQueueSubmit(queue, 1, &submitInfo, fence);

        if (VK_SUCCESS == result)
        {
            vkWaitForFences(device, 1, &fence, true, UINT64_MAX);
        }

        vkDestroyFence(device, fence, nullptr);
        fence = nullptr;
    }

    return result;
}

