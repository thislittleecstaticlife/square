//
// utilities.h
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

#pragma once

#include <vulkan/vulkan.h>

//====----------------------------------------------------------------------====
//
// * Utilities
//
//====----------------------------------------------------------------------====

#define ARRAY_LENGTH(Array_)     (uint32_t)( sizeof(Array_)/sizeof(Array_[0]) )
#define IS_FLAG_SET(Val_, Flag_) ( 0 != (Val_ & Flag_) )

//====----------------------------------------------------------------------====
//
// * Physical device
//
//====----------------------------------------------------------------------====

// * findFirstGPU
//
VkResult findFirstGPU(VkInstance instance, VkPhysicalDevice* pPhysicalDevice);

//====----------------------------------------------------------------------====
//
// * Queue family
//
//====----------------------------------------------------------------------====

// * findGraphicsAndComputeQueueFamily
//
VkResult findGraphicsAndComputeQueueFamily( VkPhysicalDevice device,
                                            uint32_t*        pQueueFamilyIndex );

//====----------------------------------------------------------------------====
//
// * Memory
//
//====----------------------------------------------------------------------====

// * hasProperties
//
bool hasProperties( const VkMemoryType*   memoryType,
                    VkMemoryPropertyFlags requestedProperties ) [[unsequenced]];

// * hasMemoryProperties
//
bool hasMemoryProperties
(
    const VkPhysicalDeviceMemoryProperties* memoryProperties,
    uint32_t                                memoryTypeIndex,
    VkMemoryPropertyFlags                   requestedProperties
)
    [[unsequenced]];

// * findMemoryTypeIndex
//
VkResult findMemoryTypeIndex
(
    const VkPhysicalDeviceMemoryProperties* memoryProperties,
    uint32_t                                memoryTypeIndexBits,
    VkMemoryPropertyFlags                   requestedProperties,
    uint32_t*                               pMemoryTypeIndex
);

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
);

//====----------------------------------------------------------------------====
//
// * Command buffers
//
//====----------------------------------------------------------------------====

VkResult submitCommandBuffer( VkDevice        device,
                              VkQueue         queue,
                              VkCommandBuffer commandBuffer );

