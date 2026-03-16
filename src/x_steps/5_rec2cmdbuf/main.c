#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

int main(void) {
    VkResult result;

    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    VkApplicationInfo appInfo = {0};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Record Command Buffer";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "None";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instanceInfo = {0};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;

    result = vkCreateInstance(&instanceInfo, NULL, &instance);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan instance\n");
        return EXIT_FAILURE;
    }

    uint32_t deviceCount = 0;
    result = vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);
    if (result != VK_SUCCESS || deviceCount == 0) {
        fprintf(stderr, "No Vulkan-compatible GPU found\n");
        vkDestroyInstance(instance, NULL);
        return EXIT_FAILURE;
    }

    VkPhysicalDevice physicalDevices[16];
    if (deviceCount > 16) {
        deviceCount = 16;
    }

    result = vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to enumerate physical devices\n");
        vkDestroyInstance(instance, NULL);
        return EXIT_FAILURE;
    }

    physicalDevice = physicalDevices[0];

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    printf("Using device: %s\n", deviceProperties.deviceName);

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);
    if (queueFamilyCount == 0) {
        fprintf(stderr, "No queue families found\n");
        vkDestroyInstance(instance, NULL);
        return EXIT_FAILURE;
    }

    VkQueueFamilyProperties queueFamilies[32];
    if (queueFamilyCount > 32) {
        queueFamilyCount = 32;
    }

    vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice,
        &queueFamilyCount,
        queueFamilies
    );

    uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsQueueFamilyIndex = i;
            break;
        }
    }

    if (graphicsQueueFamilyIndex == UINT32_MAX) {
        fprintf(stderr, "No graphics-capable queue family found\n");
        vkDestroyInstance(instance, NULL);
        return EXIT_FAILURE;
    }

    float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueInfo = {0};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceInfo = {0};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;

    result = vkCreateDevice(physicalDevice, &deviceInfo, NULL, &device);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create logical device\n");
        vkDestroyInstance(instance, NULL);
        return EXIT_FAILURE;
    }

    vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);

    VkCommandPoolCreateInfo poolInfo = {0};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
    poolInfo.flags = 0;

    result = vkCreateCommandPool(device, &poolInfo, NULL, &commandPool);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create command pool\n");
        vkDestroyDevice(device, NULL);
        vkDestroyInstance(instance, NULL);
        return EXIT_FAILURE;
    }

    VkCommandBufferAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    result = vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to allocate command buffer\n");
        vkDestroyCommandPool(device, commandPool, NULL);
        vkDestroyDevice(device, NULL);
        vkDestroyInstance(instance, NULL);
        return EXIT_FAILURE;
    }

    VkCommandBufferBeginInfo beginInfo = {0};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = NULL;

    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to begin command buffer recording\n");
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        vkDestroyCommandPool(device, commandPool, NULL);
        vkDestroyDevice(device, NULL);
        vkDestroyInstance(instance, NULL);
        return EXIT_FAILURE;
    }

    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to end command buffer recording\n");
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        vkDestroyCommandPool(device, commandPool, NULL);
        vkDestroyDevice(device, NULL);
        vkDestroyInstance(instance, NULL);
        return EXIT_FAILURE;
    }

    printf("Logical device created successfully\n");
    printf("Graphics queue family index: %u\n", graphicsQueueFamilyIndex);
    printf("Queue handle acquired successfully\n");
    printf("Command pool created successfully\n");
    printf("Command buffer allocated successfully\n");
    printf("Command buffer recording began successfully\n");
    printf("Command buffer recording ended successfully\n");

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    vkDestroyCommandPool(device, commandPool, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroyInstance(instance, NULL);

    return EXIT_SUCCESS;
}
