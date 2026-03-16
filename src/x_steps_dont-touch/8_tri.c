#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define APP_WIDTH 800
#define APP_HEIGHT 600
#define MAX_FRAMES_IN_FLIGHT 2

#define ARRAY_COUNT(a) ((uint32_t)(sizeof(a) / sizeof((a)[0])))

static const char *kValidationLayers[] = {"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
static const bool kEnableValidationLayers = false;
#else
static const bool kEnableValidationLayers = true;
#endif

typedef struct {
    uint32_t graphicsFamily;
    uint32_t presentFamily;
    bool hasGraphics;
    bool hasPresent;
} QueueFamilyIndices;

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR *formats;
    uint32_t formatCount;
    VkPresentModeKHR *presentModes;
    uint32_t presentModeCount;
} SwapChainSupportDetails;

typedef struct {
    GLFWwindow *window;

    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkSwapchainKHR swapchain;
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;
    VkImage *swapchainImages;
    uint32_t swapchainImageCount;
    VkImageView *swapchainImageViews;
    VkFramebuffer *swapchainFramebuffers;

    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    VkCommandPool commandPool;
    VkCommandBuffer *commandBuffers;

    VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];
    uint32_t currentFrame;
} App;

static void fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
}

static bool checkValidationLayerSupport(void) {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, NULL);
    VkLayerProperties *availableLayers = malloc(sizeof(VkLayerProperties) * layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

    for (uint32_t i = 0; i < ARRAY_COUNT(kValidationLayers); ++i) {
        bool found = false;
        for (uint32_t j = 0; j < layerCount; ++j) {
            if (strcmp(kValidationLayers[i], availableLayers[j].layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            free(availableLayers);
            return false;
        }
    }

    free(availableLayers);
    return true;
}

static void createInstance(App *app) {
    if (kEnableValidationLayers && !checkValidationLayerSupport()) {
        fail("Validation layers requested but unavailable");
    }

    VkApplicationInfo appInfo = {0};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "None";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    VkInstanceCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;

    if (kEnableValidationLayers) {
        createInfo.enabledLayerCount = ARRAY_COUNT(kValidationLayers);
        createInfo.ppEnabledLayerNames = kValidationLayers;
    }

    if (vkCreateInstance(&createInfo, NULL, &app->instance) != VK_SUCCESS) {
        fail("Failed to create Vulkan instance");
    }
}

static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices = {0};
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, NULL);
    VkQueueFamilyProperties *families = malloc(sizeof(VkQueueFamilyProperties) * count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families);

    for (uint32_t i = 0; i < count; i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
            indices.hasGraphics = true;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
            indices.hasPresent = true;
        }

        if (indices.hasGraphics && indices.hasPresent) {
            break;
        }
    }

    free(families);
    return indices;
}

static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapChainSupportDetails details = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.formatCount, NULL);
    if (details.formatCount > 0) {
        details.formats = malloc(sizeof(VkSurfaceFormatKHR) * details.formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.formatCount, details.formats);
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.presentModeCount, NULL);
    if (details.presentModeCount > 0) {
        details.presentModes = malloc(sizeof(VkPresentModeKHR) * details.presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.presentModeCount, details.presentModes);
    }

    return details;
}

static void freeSwapChainSupport(SwapChainSupportDetails *details) {
    free(details->formats);
    free(details->presentModes);
}

static bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices = findQueueFamilies(device, surface);
    if (!indices.hasGraphics || !indices.hasPresent) {
        return false;
    }

    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);
    VkExtensionProperties *available = malloc(sizeof(VkExtensionProperties) * extensionCount);
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, available);

    bool hasSwapchain = false;
    for (uint32_t i = 0; i < extensionCount; ++i) {
        if (strcmp(available[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            hasSwapchain = true;
            break;
        }
    }
    free(available);

    if (!hasSwapchain) {
        return false;
    }

    SwapChainSupportDetails swapDetails = querySwapChainSupport(device, surface);
    bool ok = swapDetails.formatCount > 0 && swapDetails.presentModeCount > 0;
    freeSwapChainSupport(&swapDetails);
    return ok;
}

static void pickPhysicalDevice(App *app) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(app->instance, &count, NULL);
    if (count == 0) {
        fail("No Vulkan-compatible GPU found");
    }

    VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * count);
    vkEnumeratePhysicalDevices(app->instance, &count, devices);

    for (uint32_t i = 0; i < count; ++i) {
        if (isDeviceSuitable(devices[i], app->surface)) {
            app->physicalDevice = devices[i];
            break;
        }
    }

    free(devices);
    if (app->physicalDevice == VK_NULL_HANDLE) {
        fail("No suitable GPU found");
    }
}

static void createLogicalDevice(App *app) {
    QueueFamilyIndices indices = findQueueFamilies(app->physicalDevice, app->surface);

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfos[2] = {0};
    uint32_t queueInfoCount = 0;

    queueInfos[queueInfoCount].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfos[queueInfoCount].queueFamilyIndex = indices.graphicsFamily;
    queueInfos[queueInfoCount].queueCount = 1;
    queueInfos[queueInfoCount].pQueuePriorities = &priority;
    queueInfoCount++;

    if (indices.presentFamily != indices.graphicsFamily) {
        queueInfos[queueInfoCount].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[queueInfoCount].queueFamilyIndex = indices.presentFamily;
        queueInfos[queueInfoCount].queueCount = 1;
        queueInfos[queueInfoCount].pQueuePriorities = &priority;
        queueInfoCount++;
    }

    const char *deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = queueInfoCount;
    createInfo.pQueueCreateInfos = queueInfos;
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    if (kEnableValidationLayers) {
        createInfo.enabledLayerCount = ARRAY_COUNT(kValidationLayers);
        createInfo.ppEnabledLayerNames = kValidationLayers;
    }

    if (vkCreateDevice(app->physicalDevice, &createInfo, NULL, &app->device) != VK_SUCCESS) {
        fail("Failed to create logical device");
    }

    vkGetDeviceQueue(app->device, indices.graphicsFamily, 0, &app->graphicsQueue);
    vkGetDeviceQueue(app->device, indices.presentFamily, 0, &app->presentQueue);
}

static VkSurfaceFormatKHR chooseSurfaceFormat(const VkSurfaceFormatKHR *formats, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return formats[i];
        }
    }
    return formats[0];
}

static VkPresentModeKHR choosePresentMode(const VkPresentModeKHR *modes, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return modes[i];
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR *capabilities, GLFWwindow *window) {
    if (capabilities->currentExtent.width != UINT32_MAX) {
        return capabilities->currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    VkExtent2D actualExtent = {
        .width = (uint32_t)width,
        .height = (uint32_t)height,
    };

    if (actualExtent.width < capabilities->minImageExtent.width) {
        actualExtent.width = capabilities->minImageExtent.width;
    }
    if (actualExtent.width > capabilities->maxImageExtent.width) {
        actualExtent.width = capabilities->maxImageExtent.width;
    }
    if (actualExtent.height < capabilities->minImageExtent.height) {
        actualExtent.height = capabilities->minImageExtent.height;
    }
    if (actualExtent.height > capabilities->maxImageExtent.height) {
        actualExtent.height = capabilities->maxImageExtent.height;
    }

    return actualExtent;
}

static void createSwapchain(App *app) {
    SwapChainSupportDetails details = querySwapChainSupport(app->physicalDevice, app->surface);
    VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(details.formats, details.formatCount);
    VkPresentModeKHR presentMode = choosePresentMode(details.presentModes, details.presentModeCount);
    VkExtent2D extent = chooseExtent(&details.capabilities, app->window);

    uint32_t imageCount = details.capabilities.minImageCount + 1;
    if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount) {
        imageCount = details.capabilities.maxImageCount;
    }

    QueueFamilyIndices indices = findQueueFamilies(app->physicalDevice, app->surface);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily, indices.presentFamily};

    VkSwapchainCreateInfoKHR createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = app->surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = details.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(app->device, &createInfo, NULL, &app->swapchain) != VK_SUCCESS) {
        freeSwapChainSupport(&details);
        fail("Failed to create swap chain");
    }

    vkGetSwapchainImagesKHR(app->device, app->swapchain, &app->swapchainImageCount, NULL);
    app->swapchainImages = malloc(sizeof(VkImage) * app->swapchainImageCount);
    vkGetSwapchainImagesKHR(app->device, app->swapchain, &app->swapchainImageCount, app->swapchainImages);

    app->swapchainImageFormat = surfaceFormat.format;
    app->swapchainExtent = extent;

    freeSwapChainSupport(&details);
}

static void createImageViews(App *app) {
    app->swapchainImageViews = malloc(sizeof(VkImageView) * app->swapchainImageCount);

    for (uint32_t i = 0; i < app->swapchainImageCount; ++i) {
        VkImageViewCreateInfo createInfo = {0};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = app->swapchainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = app->swapchainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(app->device, &createInfo, NULL, &app->swapchainImageViews[i]) != VK_SUCCESS) {
            fail("Failed to create image views");
        }
    }
}

static uint32_t *readFileBinary(const char *path, size_t *sizeOut) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Could not open shader file: %s\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size <= 0 || (size % 4) != 0) {
        fclose(f);
        fprintf(stderr, "Invalid SPIR-V size for %s\n", path);
        return NULL;
    }

    uint32_t *buffer = malloc((size_t)size);
    if (fread(buffer, 1, (size_t)size, f) != (size_t)size) {
        free(buffer);
        fclose(f);
        fprintf(stderr, "Failed to read file: %s\n", path);
        return NULL;
    }

    fclose(f);
    *sizeOut = (size_t)size;
    return buffer;
}

static VkShaderModule createShaderModule(App *app, const uint32_t *code, size_t size) {
    VkShaderModuleCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = size;
    createInfo.pCode = code;

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(app->device, &createInfo, NULL, &shaderModule) != VK_SUCCESS) {
        fail("Failed to create shader module");
    }

    return shaderModule;
}

static void createRenderPass(App *app) {
    VkAttachmentDescription colorAttachment = {0};
    colorAttachment.format = app->swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {0};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency = {0};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo = {0};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(app->device, &renderPassInfo, NULL, &app->renderPass) != VK_SUCCESS) {
        fail("Failed to create render pass");
    }
}

static void createGraphicsPipeline(App *app) {
    size_t vertSize = 0;
    size_t fragSize = 0;
    uint32_t *vertCode = readFileBinary("shaders/triangle.vert.spv", &vertSize);
    uint32_t *fragCode = readFileBinary("shaders/triangle.frag.spv", &fragSize);

    if (!vertCode || !fragCode) {
        fail("Missing shaders. Build shaders/triangle.vert.spv and shaders/triangle.frag.spv first.");
    }

    VkShaderModule vertShader = createShaderModule(app, vertCode, vertSize);
    VkShaderModule fragShader = createShaderModule(app, fragCode, fragSize);

    free(vertCode);
    free(fragCode);

    VkPipelineShaderStageCreateInfo vertStage = {0};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertShader;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage = {0};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragShader;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

    VkPipelineVertexInputStateCreateInfo vertexInput = {0};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {0};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport = {0};
    viewport.width = (float)app->swapchainExtent.width;
    viewport.height = (float)app->swapchainExtent.height;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {0};
    scissor.extent = app->swapchainExtent;

    VkPipelineViewportStateCreateInfo viewportState = {0};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {0};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling = {0};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {0};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending = {0};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if (vkCreatePipelineLayout(app->device, &pipelineLayoutInfo, NULL, &app->pipelineLayout) != VK_SUCCESS) {
        fail("Failed to create pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo = {0};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = app->pipelineLayout;
    pipelineInfo.renderPass = app->renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(app->device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &app->graphicsPipeline) != VK_SUCCESS) {
        fail("Failed to create graphics pipeline");
    }

    vkDestroyShaderModule(app->device, fragShader, NULL);
    vkDestroyShaderModule(app->device, vertShader, NULL);
}

static void createFramebuffers(App *app) {
    app->swapchainFramebuffers = malloc(sizeof(VkFramebuffer) * app->swapchainImageCount);

    for (uint32_t i = 0; i < app->swapchainImageCount; ++i) {
        VkImageView attachments[] = {app->swapchainImageViews[i]};

        VkFramebufferCreateInfo framebufferInfo = {0};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = app->renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = app->swapchainExtent.width;
        framebufferInfo.height = app->swapchainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(app->device, &framebufferInfo, NULL, &app->swapchainFramebuffers[i]) != VK_SUCCESS) {
            fail("Failed to create framebuffer");
        }
    }
}

static void createCommandPool(App *app) {
    QueueFamilyIndices indices = findQueueFamilies(app->physicalDevice, app->surface);

    VkCommandPoolCreateInfo poolInfo = {0};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = indices.graphicsFamily;

    if (vkCreateCommandPool(app->device, &poolInfo, NULL, &app->commandPool) != VK_SUCCESS) {
        fail("Failed to create command pool");
    }
}

static void createCommandBuffers(App *app) {
    app->commandBuffers = malloc(sizeof(VkCommandBuffer) * app->swapchainImageCount);

    VkCommandBufferAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = app->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = app->swapchainImageCount;

    if (vkAllocateCommandBuffers(app->device, &allocInfo, app->commandBuffers) != VK_SUCCESS) {
        fail("Failed to allocate command buffers");
    }

    for (uint32_t i = 0; i < app->swapchainImageCount; ++i) {
        VkCommandBufferBeginInfo beginInfo = {0};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkBeginCommandBuffer(app->commandBuffers[i], &beginInfo);

        VkRenderPassBeginInfo renderPassInfo = {0};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = app->renderPass;
        renderPassInfo.framebuffer = app->swapchainFramebuffers[i];
        renderPassInfo.renderArea.offset = (VkOffset2D){0, 0};
        renderPassInfo.renderArea.extent = app->swapchainExtent;

        VkClearValue clearColor = {{{0.05f, 0.05f, 0.1f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(app->commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(app->commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, app->graphicsPipeline);
        vkCmdDraw(app->commandBuffers[i], 3, 1, 0, 0);
        vkCmdEndRenderPass(app->commandBuffers[i]);

        if (vkEndCommandBuffer(app->commandBuffers[i]) != VK_SUCCESS) {
            fail("Failed to record command buffer");
        }
    }
}

static void createSyncObjects(App *app) {
    VkSemaphoreCreateInfo semaphoreInfo = {0};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {0};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(app->device, &semaphoreInfo, NULL, &app->imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(app->device, &semaphoreInfo, NULL, &app->renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(app->device, &fenceInfo, NULL, &app->inFlightFences[i]) != VK_SUCCESS) {
            fail("Failed to create sync objects");
        }
    }
}

static void drawFrame(App *app) {
    vkWaitForFences(app->device, 1, &app->inFlightFences[app->currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        app->device,
        app->swapchain,
        UINT64_MAX,
        app->imageAvailableSemaphores[app->currentFrame],
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return;
    }

    vkResetFences(app->device, 1, &app->inFlightFences[app->currentFrame]);

    VkSemaphore waitSemaphores[] = {app->imageAvailableSemaphores[app->currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSemaphores[] = {app->renderFinishedSemaphores[app->currentFrame]};

    VkSubmitInfo submitInfo = {0};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &app->commandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(app->graphicsQueue, 1, &submitInfo, app->inFlightFences[app->currentFrame]) != VK_SUCCESS) {
        fail("Failed to submit draw command buffer");
    }

    VkSwapchainKHR swapchains[] = {app->swapchain};
    VkPresentInfoKHR presentInfo = {0};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(app->presentQueue, &presentInfo);
    app->currentFrame = (app->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

static void initWindow(App *app) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    app->window = glfwCreateWindow(APP_WIDTH, APP_HEIGHT, "Vulkan Triangle", NULL, NULL);
    if (!app->window) {
        fail("Failed to create GLFW window");
    }
}

static void initVulkan(App *app) {
    createInstance(app);

    if (glfwCreateWindowSurface(app->instance, app->window, NULL, &app->surface) != VK_SUCCESS) {
        fail("Failed to create window surface");
    }

    pickPhysicalDevice(app);
    createLogicalDevice(app);
    createSwapchain(app);
    createImageViews(app);
    createRenderPass(app);
    createGraphicsPipeline(app);
    createFramebuffers(app);
    createCommandPool(app);
    createCommandBuffers(app);
    createSyncObjects(app);
}

static void cleanup(App *app) {
    vkDeviceWaitIdle(app->device);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(app->device, app->renderFinishedSemaphores[i], NULL);
        vkDestroySemaphore(app->device, app->imageAvailableSemaphores[i], NULL);
        vkDestroyFence(app->device, app->inFlightFences[i], NULL);
    }

    vkFreeCommandBuffers(app->device, app->commandPool, app->swapchainImageCount, app->commandBuffers);
    free(app->commandBuffers);

    vkDestroyCommandPool(app->device, app->commandPool, NULL);

    for (uint32_t i = 0; i < app->swapchainImageCount; ++i) {
        vkDestroyFramebuffer(app->device, app->swapchainFramebuffers[i], NULL);
    }
    free(app->swapchainFramebuffers);

    vkDestroyPipeline(app->device, app->graphicsPipeline, NULL);
    vkDestroyPipelineLayout(app->device, app->pipelineLayout, NULL);
    vkDestroyRenderPass(app->device, app->renderPass, NULL);

    for (uint32_t i = 0; i < app->swapchainImageCount; ++i) {
        vkDestroyImageView(app->device, app->swapchainImageViews[i], NULL);
    }
    free(app->swapchainImageViews);
    free(app->swapchainImages);

    vkDestroySwapchainKHR(app->device, app->swapchain, NULL);
    vkDestroyDevice(app->device, NULL);
    vkDestroySurfaceKHR(app->instance, app->surface, NULL);
    vkDestroyInstance(app->instance, NULL);

    glfwDestroyWindow(app->window);
    glfwTerminate();
}

int main(void) {
    App app = {0};
    initWindow(&app);
    initVulkan(&app);

    while (!glfwWindowShouldClose(app.window)) {
        glfwPollEvents();
        drawFrame(&app);
    }

    cleanup(&app);
    return EXIT_SUCCESS;
}
