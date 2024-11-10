/////////////////////////////////////////////////////////////////////////////////////
// Vulkan VK_KHR_fragment_shader_barycentric extension example
/////////////////////////////////////////////////////////////////////////////////////
//                               License:
/////////////////////////////////////////////////////////////////////////////////////
//
//                            zlib license                                
//                                                                            
// Copyright (C) 2024, Benjamin Rosseaux (benjamin@rosseaux.de)
//                                                                            
// This software is provided 'as-is', without any express or implied          
// warranty. In no event will the authors be held liable for any damages      
// arising from the use of this software.                                     
//                                                                           
// Permission is granted to anyone to use this software for any purpose,      
// including commercial applications, and to alter it and redistribute it     
// freely, subject to the following restrictions:                             
//                                                                            
// 1. The origin of this software must not be misrepresented; you must not    
//    claim that you wrote the original software. If you use this software    
//    in a product, an acknowledgement in the product documentation would be  
//    appreciated but is not required.                                        
// 2. Altered source versions must be plainly marked as such, and must not be 
//    misrepresented as being the original software.                          
// 3. This notice may not be removed or altered from any source distribution. 
//
/////////////////////////////////////////////////////////////////////////////////////
// This example demonstrates the use of the `VK_KHR_fragment_shader_barycentric` 
// extension in Vulkan. The extension provides the Barycentric coordinates of the 
// fragment within the triangle. This example renders a simple triangle, coloring 
// the fragments based on the Barycentric coordinates and adding a thick white 
// border line around it.
/////////////////////////////////////////////////////////////////////////////////////

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> // For fmax and fmin

#define WIDTH 800
#define HEIGHT 600

const char* vertShaderPath = "triangle.vert.spv";
const char* fragShaderPath = "triangle.frag.spv";

#ifndef VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME
#define VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME "VK_KHR_fragment_shader_barycentric"
#endif

static VkInstance instance;
static VkSurfaceKHR surface;
static VkDevice device;
static VkSwapchainKHR swapChain;
static VkImage* swapChainImages;
static VkImageView* swapChainImageViews;
static uint32_t swapChainImageCount;
static VkFormat swapChainImageFormat;
static VkExtent2D swapChainExtent;
static VkPipeline graphicsPipeline;
static VkRenderPass renderPass;
static VkFramebuffer* swapChainFramebuffers;
static VkCommandPool commandPool;
static VkCommandBuffer* commandBuffers;
static VkQueue graphicsQueue;
static VkQueue presentQueue;
static VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
static VkPipelineLayout pipelineLayout;
static VkSemaphore imageAvailableSemaphore;
static VkSemaphore renderFinishedSemaphore;
static VkFence inFlightFence;

// Variable to keep track of the Vulkan version used
static uint32_t usedApiVersion = VK_API_VERSION_1_2;

typedef struct {
    uint32_t graphicsFamily;
    uint32_t presentFamily;
    VkBool32 graphicsFamilyFound;
    VkBool32 presentFamilyFound;
} QueueFamilyIndices;

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR* formats;
    uint32_t formatCount;
    VkPresentModeKHR* presentModes;
    uint32_t presentModeCount;
} SwapChainSupportDetails;

void cleanup(SDL_Window* window) {
    vkDeviceWaitIdle(device);

    // Destroy synchronization objects
    vkDestroyFence(device, inFlightFence, NULL);
    vkDestroySemaphore(device, renderFinishedSemaphore, NULL);
    vkDestroySemaphore(device, imageAvailableSemaphore, NULL);

    // Destroy framebuffers
    for (size_t i = 0; i < swapChainImageCount; i++) {
        vkDestroyFramebuffer(device, swapChainFramebuffers[i], NULL);
    }
    free(swapChainFramebuffers);

    // Destroy pipeline and render pass
    vkDestroyPipeline(device, graphicsPipeline, NULL);
    vkDestroyPipelineLayout(device, pipelineLayout, NULL);
    vkDestroyRenderPass(device, renderPass, NULL);

    // Destroy image views
    for (size_t i = 0; i < swapChainImageCount; i++) {
        vkDestroyImageView(device, swapChainImageViews[i], NULL);
    }
    free(swapChainImageViews);

    // Destroy swapchain
    vkDestroySwapchainKHR(device, swapChain, NULL);

    // Free command buffers
    vkFreeCommandBuffers(device, commandPool, swapChainImageCount, commandBuffers);
    free(commandBuffers);

    // Destroy command pool
    vkDestroyCommandPool(device, commandPool, NULL);

    // Destroy device
    vkDestroyDevice(device, NULL);

    // Destroy surface and instance
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);

    // Free swap chain images array
    free(swapChainImages);

    SDL_DestroyWindow(window);
    SDL_Quit();
}

void createInstance(SDL_Window* window) {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan VK_KHR_fragment_shader_barycentric example";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    // Set desired Vulkan API version to 1.3
    appInfo.apiVersion = VK_API_VERSION_1_3;

    // Get required extensions from SDL
    unsigned int sdlExtensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, NULL)) {
        fprintf(stderr, "Failed to get SDL Vulkan extensions\n");
        exit(1);
    }
    const char** sdlExtensions = malloc(sizeof(const char*) * sdlExtensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, sdlExtensions)) {
        fprintf(stderr, "Failed to get SDL Vulkan extensions\n");
        exit(1);
    }

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = sdlExtensionCount;
    createInfo.ppEnabledExtensionNames = sdlExtensions;

    // Optionally enable validation layers
    const char* validationLayers[] = {
        "VK_LAYER_KHRONOS_validation"
    };
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = validationLayers;

    if (vkCreateInstance(&createInfo, NULL, &instance) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan instance!\n");
        exit(1);
    }

    free(sdlExtensions);
}

void createSurface(SDL_Window* window) {
    if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
        fprintf(stderr, "Failed to create window surface!\n");
        exit(1);
    }
}

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices = {0};

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);

    VkQueueFamilyProperties* queueFamilies = malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
            indices.graphicsFamilyFound = VK_TRUE;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
            indices.presentFamilyFound = VK_TRUE;
        }

        if (indices.graphicsFamilyFound && indices.presentFamilyFound) {
            break;
        }
    }

    free(queueFamilies);
    return indices;
}

VkBool32 checkDeviceExtensionSupport(VkPhysicalDevice device, const char** requiredExtensions, uint32_t requiredExtensionCount) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);
    VkExtensionProperties* availableExtensions = malloc(sizeof(VkExtensionProperties) * extensionCount);
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, availableExtensions);

    uint32_t requiredFound = 0;
    for (uint32_t i = 0; i < requiredExtensionCount; i++) {
        for (uint32_t j = 0; j < extensionCount; j++) {
            if (strcmp(requiredExtensions[i], availableExtensions[j].extensionName) == 0) {
                requiredFound++;
                break;
            }
        }
    }

    free(availableExtensions);
    return requiredFound == requiredExtensionCount;
}

void pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);
    if (deviceCount == 0) {
        fprintf(stderr, "Failed to find GPUs with Vulkan support!\n");
        exit(1);
    }

    VkPhysicalDevice* devices = malloc(sizeof(VkPhysicalDevice) * deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices);

    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    uint32_t bestApiVersion = 0;

    const char* requiredDeviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME
    };
    uint32_t requiredDeviceExtensionCount = sizeof(requiredDeviceExtensions) / sizeof(requiredDeviceExtensions[0]);

    for (uint32_t i = 0; i < deviceCount; i++) {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(devices[i], &deviceProperties);

        // Check for Vulkan 1.3 support
        if (deviceProperties.apiVersion >= VK_API_VERSION_1_3) {
            if (!checkDeviceExtensionSupport(devices[i], requiredDeviceExtensions, requiredDeviceExtensionCount)) {
                continue;
            }
            bestDevice = devices[i];
            bestApiVersion = VK_API_VERSION_1_3;
            break;
        } else if (deviceProperties.apiVersion >= VK_API_VERSION_1_2 && bestApiVersion < VK_API_VERSION_1_2) {
            if (!checkDeviceExtensionSupport(devices[i], requiredDeviceExtensions, requiredDeviceExtensionCount)) {
                continue;
            }
            bestDevice = devices[i];
            bestApiVersion = VK_API_VERSION_1_2;
        }
        // Add further else-if blocks if you wish to support earlier versions
    }

    if (bestDevice == VK_NULL_HANDLE) {
        fprintf(stderr, "Failed to find a suitable GPU with Vulkan 1.2 or 1.3 support and required extensions!\n");
        exit(1);
    }

    physicalDevice = bestDevice;
    usedApiVersion = bestApiVersion;

    // Check if the selected device has the necessary queue families
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    if (!indices.graphicsFamilyFound || !indices.presentFamilyFound) {
        fprintf(stderr, "Selected GPU does not have required queue families!\n");
        exit(1);
    }

    free(devices);
}

void createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfos[2];
    uint32_t queueCreateInfoCount = 0;

    if (indices.graphicsFamily == indices.presentFamily) {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = indices.graphicsFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos[0] = queueCreateInfo;
        queueCreateInfoCount = 1;
    } else {
        VkDeviceQueueCreateInfo queueCreateInfo1 = {};
        queueCreateInfo1.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo1.queueFamilyIndex = indices.graphicsFamily;
        queueCreateInfo1.queueCount = 1;
        queueCreateInfo1.pQueuePriorities = &queuePriority;
        queueCreateInfos[0] = queueCreateInfo1;

        VkDeviceQueueCreateInfo queueCreateInfo2 = {};
        queueCreateInfo2.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo2.queueFamilyIndex = indices.presentFamily;
        queueCreateInfo2.queueCount = 1;
        queueCreateInfo2.pQueuePriorities = &queuePriority;
        queueCreateInfos[1] = queueCreateInfo2;

        queueCreateInfoCount = 2;
    }

    // List of required device extensions
    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME
    };
    uint32_t deviceExtensionCount = sizeof(deviceExtensions) / sizeof(deviceExtensions[0]);

    // Zero-initialize all feature structures
    VkPhysicalDeviceFeatures2 deviceFeatures2 = {};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR barycentricFeatures = {};
    barycentricFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR;
    barycentricFeatures.pNext = NULL;

    // Chain the barycentric features to the device features
    deviceFeatures2.pNext = &barycentricFeatures;

    // Query the physical device features
    vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);

    // Check if fragmentShaderBarycentric is supported
    if (!barycentricFeatures.fragmentShaderBarycentric) {
        fprintf(stderr, "Physical device does not support fragmentShaderBarycentric feature!\n");
        exit(1);
    }

    // Enable the barycentric feature
    barycentricFeatures.fragmentShaderBarycentric = VK_TRUE;

    // Create the logical device
    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = queueCreateInfoCount;
    createInfo.pQueueCreateInfos = queueCreateInfos;
    createInfo.enabledExtensionCount = deviceExtensionCount;
    createInfo.ppEnabledExtensionNames = deviceExtensions;
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = NULL;

    // Use VkPhysicalDeviceFeatures2 to enable features
    createInfo.pNext = &deviceFeatures2;
    createInfo.pEnabledFeatures = NULL; // Not used when using VkPhysicalDeviceFeatures2

    if (vkCreateDevice(physicalDevice, &createInfo, NULL, &device) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create logical device!\n");
        exit(1);
    }

    vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
}

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.formatCount, NULL);
    if (details.formatCount != 0) {
        details.formats = malloc(sizeof(VkSurfaceFormatKHR) * details.formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.formatCount, details.formats);
    } else {
        details.formats = NULL;
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.presentModeCount, NULL);
    if (details.presentModeCount != 0) {
        details.presentModes = malloc(sizeof(VkPresentModeKHR) * details.presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.presentModeCount, details.presentModes);
    } else {
        details.presentModes = NULL;
    }

    return details;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const VkSurfaceFormatKHR* availableFormats, uint32_t formatCount) {
    for (uint32_t i = 0; i < formatCount; i++) {
        if (availableFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormats[i];
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const VkPresentModeKHR* availablePresentModes, uint32_t presentModeCount) {
    for (uint32_t i = 0; i < presentModeCount; i++) {
        if (availablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentModes[i];
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR* capabilities) {
    if (capabilities->currentExtent.width != UINT32_MAX) {
        return capabilities->currentExtent;
    } else {
        VkExtent2D actualExtent = {WIDTH, HEIGHT};
        actualExtent.width = fmax(capabilities->minImageExtent.width, fmin(capabilities->maxImageExtent.width, actualExtent.width));
        actualExtent.height = fmax(capabilities->minImageExtent.height, fmin(capabilities->maxImageExtent.height, actualExtent.height));
        return actualExtent;
    }
}

VkImageView createImageView(VkImage image, VkFormat format) {
    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &createInfo, NULL, &imageView) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create image view!\n");
        exit(1);
    }

    return imageView;
}

void createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats, swapChainSupport.formatCount);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes, swapChainSupport.presentModeCount);
    VkExtent2D extent = chooseSwapExtent(&swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily, indices.presentFamily};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device, &createInfo, NULL, &swapChain) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create swap chain!\n");
        exit(1);
    }

    vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCount, NULL);
    swapChainImages = malloc(sizeof(VkImage) * swapChainImageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCount, swapChainImages);

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;

    // Create image views for the swapchain images
    swapChainImageViews = malloc(swapChainImageCount * sizeof(VkImageView));
    for (size_t i = 0; i < swapChainImageCount; i++) {
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat);
    }

    // Clean up allocated arrays in swapChainSupport
    free(swapChainSupport.formats);
    free(swapChainSupport.presentModes);
}

void createRenderPass() {
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, NULL, &renderPass) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create render pass!\n");
        exit(1);
    }
}

VkShaderModule createShaderModule(const char* filePath) {
    FILE* file = fopen(filePath, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", filePath);
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = malloc(fileSize);
    fread(buffer, 1, fileSize, file);
    fclose(file);

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = fileSize;
    createInfo.pCode = (uint32_t*)buffer;

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, NULL, &shaderModule) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create shader module!\n");
        exit(1);
    }

    free(buffer);
    return shaderModule;
}

void createGraphicsPipeline() {
    VkShaderModule vertShaderModule = createShaderModule(vertShaderPath);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderPath);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapChainExtent.width;
    viewport.height = (float)swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = (VkOffset2D){0, 0};
    scissor.extent = swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &pipelineLayout) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create pipeline layout!\n");
        exit(1);
    }

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &graphicsPipeline) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create graphics pipeline!\n");
        exit(1);
    }

    vkDestroyShaderModule(device, vertShaderModule, NULL);
    vkDestroyShaderModule(device, fragShaderModule, NULL);
}

void createFramebuffers() {
    swapChainFramebuffers = malloc(swapChainImageCount * sizeof(VkFramebuffer));
    for (size_t i = 0; i < swapChainImageCount; i++) {
        VkImageView attachments[] = {
            swapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, NULL, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create framebuffer!\n");
            exit(1);
        }
    }
}

void createCommandPool() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = indices.graphicsFamily;

    if (vkCreateCommandPool(device, &poolInfo, NULL, &commandPool) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create command pool!\n");
        exit(1);
    }
}

void createCommandBuffers() {
    commandBuffers = malloc(swapChainImageCount * sizeof(VkCommandBuffer));

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)swapChainImageCount;

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers) != VK_SUCCESS) {
        fprintf(stderr, "Failed to allocate command buffers!\n");
        exit(1);
    }

    for (size_t i = 0; i < swapChainImageCount; i++) {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS) {
            fprintf(stderr, "Failed to begin recording command buffer!\n");
            exit(1);
        }

        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[i];
        renderPassInfo.renderArea.offset = (VkOffset2D){0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);
        vkCmdEndRenderPass(commandBuffers[i]);

        if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
            fprintf(stderr, "Failed to record command buffer!\n");
            exit(1);
        }
    }
}

void createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(device, &semaphoreInfo, NULL, &imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(device, &semaphoreInfo, NULL, &renderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(device, &fenceInfo, NULL, &inFlightFence) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create synchronization objects!\n");
        exit(1);
    }
}

void drawFrame() {
    vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &inFlightFence);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
        fprintf(stderr, "Failed to submit draw command buffer!\n");
        exit(1);
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapChain;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(presentQueue, &presentInfo);
}

void initVulkan(SDL_Window* window) {
    createInstance(window);
    createSurface(window);
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("Vulkan VK_KHR_fragment_shader_barycentric example", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_VULKAN);

    initVulkan(window);

    int quit = 0;
    SDL_Event event;
    while (!quit) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = 1;
            }
        }
        drawFrame();
    }

    cleanup(window);

    return 0;
}
