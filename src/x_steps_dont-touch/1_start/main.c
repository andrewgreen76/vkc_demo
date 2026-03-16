#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

int main() {
    VkInstance instance;

    VkApplicationInfo appInfo = {0};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Minimal Vulkan App";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    VkResult result = vkCreateInstance(&createInfo, NULL, &instance);

    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan instance\n");
        return EXIT_FAILURE;
    }

    printf("Vulkan instance created successfully\n");

    vkDestroyInstance(instance, NULL);

    return EXIT_SUCCESS;
}
