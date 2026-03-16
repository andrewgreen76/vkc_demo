#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

int main() {

    VkInstance instance;

    VkApplicationInfo appInfo = {0};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Device List";
    appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
    appInfo.pEngineName = "None";
    appInfo.engineVersion = VK_MAKE_VERSION(1,0,0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    if (vkCreateInstance(&createInfo, NULL, &instance) != VK_SUCCESS) {
        printf("Failed to create Vulkan instance\n");
        return EXIT_FAILURE;
    }

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);

    if (deviceCount == 0) {
        printf("No Vulkan-compatible GPUs found\n");
        vkDestroyInstance(instance, NULL);
        return EXIT_FAILURE;
    }

    VkPhysicalDevice devices[16];
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices);

    printf("Detected %u Vulkan device(s):\n\n", deviceCount);

    for (uint32_t i = 0; i < deviceCount; i++) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(devices[i], &properties);

        printf("Device %u: %s\n", i, properties.deviceName);
    }

    vkDestroyInstance(instance, NULL);

    return EXIT_SUCCESS;
}
