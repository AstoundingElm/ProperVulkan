#include <stdio.h>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string.h>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "linmath.hpp"
#define WINDOW_HEIGHT 500
#define WINDOW_WIDTH 500
#define MAX_FRAMES_IN_FLIGHT 2

#include "errors.cpp"

#include "utils.cpp"
//#include "camera.hpp"
#include "camera.cpp"

typedef struct {
  vec3 position;
  vec3 color;
} Vertex;

static uint32_t vertexCount = 6;
static Vertex vertexData[] = {
    (Vertex){.position = {1.0, 0.0, 0.0}, .color = {1.0, 0.0, 0.0}},
    (Vertex){.position = {0.0, 1.0, 0.0}, .color = {0.0, 1.0, 0.0}},
    (Vertex){.position = {0.0, 0.0, 1.0}, .color = {0.0, 0.0, 1.0}},
    (Vertex){.position = {1.0, 0.0, 1.0}, .color = {1.0, 0.0, 0.0}},
    (Vertex){.position = {0.0, 1.0, 0.0}, .color = {0.0, 1.0, 0.0}},
    (Vertex){.position = {1.0, 0.0, 1.0}, .color = {0.0, 0.0, 1.0}},
};

struct VulkContext{

    VkInstance instance;
     VkDebugUtilsMessengerEXT callback;
     VkPhysicalDevice physicalDevice;
     GLFWwindow *pWindow;
     VkSurfaceKHR surface;
     VkExtent2D swapchainExtent;
VkDevice device;
     VkQueue graphicsQueue;
      VkCommandPool commandPool;
        VkSurfaceFormatKHR surfaceFormat;
        VkSwapchainKHR swapchain;
          VkDeviceMemory depthImageMemory;
  VkImage depthImage;
  //VkImageView depthImageView;
  VkRenderPass renderPass;
  VkPipelineLayout graphicsPipelineLayout;
  VkPipeline graphicsPipeline;
  VkBuffer vertexBuffer;
  VkDeviceMemory vertexBufferMemory;
  VkCommandBuffer pVertexDisplayCommandBuffers[MAX_FRAMES_IN_FLIGHT];
  VkSemaphore pImageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
  VkSemaphore pRenderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
  VkFence pInFlightFences[MAX_FRAMES_IN_FLIGHT];
};
VulkContext context;

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              UNUSED VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              UNUSED void *pUserData) {

  /* set severity */
  ErrSeverity errSeverity = ERR_LEVEL_UNKNOWN;
  switch (messageSeverity) {
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    errSeverity = ERR_LEVEL_INFO;
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
    errSeverity = ERR_LEVEL_INFO;
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
    errSeverity = ERR_LEVEL_WARN;
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
    errSeverity = ERR_LEVEL_ERROR;
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
    errSeverity = ERR_LEVEL_UNKNOWN;
    break;
  }
  /* log error */
  LOG_ERROR_ARGS(errSeverity, "vulkan validation layer: %s",
                 pCallbackData->pMessage);
  return (VK_FALSE);
};

ErrVal getPresentQueueFamilyIndex(uint32_t *pQueueFamilyIndex,
                                  const VkPhysicalDevice physicalDevice,
                                  const VkSurfaceKHR surface) {
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           NULL);
  if (queueFamilyCount == 0) {
    LOG_ERROR(ERR_LEVEL_WARN, "no queues found");
    return (ERR_NOTSUPPORTED);
  }
  VkQueueFamilyProperties *arr = (VkQueueFamilyProperties *)malloc(
      queueFamilyCount * sizeof(VkQueueFamilyProperties));
  if (!arr) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL, "Failed to get present queue index: %s",
                   strerror(errno));
    PANIC();
  }
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           arr);
  for (uint32_t i = 0; i < queueFamilyCount; i++) {
    VkBool32 surfaceSupport;
    VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(
        physicalDevice, i, surface, &surfaceSupport);
    if (res == VK_SUCCESS && surfaceSupport) {
      *pQueueFamilyIndex = i;
      free(arr);
      return (ERR_OK);
    }
  }
  free(arr);
  return (ERR_NOTSUPPORTED);
}


ErrVal new_Instance(                            //
    VkInstance *pInstance,                      //
    const uint32_t enabledLayerCount,           //
    const char *const *ppEnabledLayerNames,     //
    const uint32_t enabledExtensionCount,       //
    const char *const *ppEnabledExtensionNames, //
    const bool enableGLFWRequiredExtensions,    //
    const bool enableDebugRequiredExtensions,   //
    const char *appname                         //
) {
  // first create a new malloced list of all extensions we need

  // this variable represents the total number of extensions (including debug
  // and glfw)
  uint32_t allExtensionCount = enabledExtensionCount;

  if (enableGLFWRequiredExtensions) {
    uint32_t glfwExtensionCount = 0;
    glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (glfwExtensionCount == 0) {
      LOG_ERROR(ERR_LEVEL_FATAL, "Error fetching required GLFW extensions");
      PANIC();
    }
    allExtensionCount += glfwExtensionCount;
  }

  if (enableDebugRequiredExtensions) {
    allExtensionCount++;
  }

  // allocate space for all extensions
  const char **ppAllExtensionNames =
     (const char **) malloc(allExtensionCount * sizeof(const char *));
  if (ppAllExtensionNames == NULL) {
    LOG_ERROR(ERR_LEVEL_FATAL, "Could not allocate memory");
    PANIC();
  }

  // this represents the current end position to add to
  size_t currentPosition = 0;

  // append glfw extensions if necessary
  if (enableGLFWRequiredExtensions) {
    // get the list of glfw extensions
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensionNames =
        glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // copy each name to the total list of extensions
    for (size_t i = 0; i < glfwExtensionCount; i++) {
      ppAllExtensionNames[currentPosition] = glfwExtensionNames[i];
      currentPosition++;
    }
  }

  // append debug extensions if neccessary
  if (enableDebugRequiredExtensions) {
    ppAllExtensionNames[currentPosition] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    currentPosition++;
  }

  // append our custom enabled extensions
  for (size_t i = 0; i < enabledExtensionCount; i++) {
    ppAllExtensionNames[currentPosition] = ppEnabledExtensionNames[i];
    currentPosition++;
  }

  /* Create app info */
  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = appname;
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "vulkan_utils.c";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_2;

  /* Create info */
  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;
  createInfo.enabledExtensionCount = allExtensionCount;
  createInfo.ppEnabledExtensionNames = ppAllExtensionNames;
  createInfo.enabledLayerCount = enabledLayerCount;
  createInfo.ppEnabledLayerNames = ppEnabledLayerNames;
  /* Actually create instance */
  VkResult result = vkCreateInstance(&createInfo, NULL, pInstance);
  if (result != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL, "Failed to create instance, error code: %s",
                   vkstrerror(result));
    PANIC();
  }

  // free our list of all the extensions
  free(ppAllExtensionNames);

  return (ERR_OK);
};

ErrVal new_DebugCallback(VkDebugUtilsMessengerEXT *pCallback,
                         const VkInstance instance) {
  VkDebugUtilsMessengerCreateInfoEXT createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = //VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT|
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = debugCallback;

  /* Returns a function pointer */
  PFN_vkCreateDebugUtilsMessengerEXT func =
      (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          instance, "vkCreateDebugUtilsMessengerEXT");
  if (!func) {
    LOG_ERROR(ERR_LEVEL_FATAL, "Failed to find extension function");
    PANIC();
  }else{printf("alrighty");}
  VkResult result = func(instance, &createInfo, NULL, pCallback);
  if (result != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL,
                   "Failed to create debug callback, error code: %s",
                   vkstrerror(result));
    PANIC();
  }
  return (ERR_NOTSUPPORTED);
};

ErrVal getQueueFamilyIndexByCapability(uint32_t *pQueueFamilyIndex,
                                       const VkPhysicalDevice device,
                                       const VkQueueFlags bit) {
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);
  if (queueFamilyCount == 0) {
    LOG_ERROR(ERR_LEVEL_WARN, "no device queues found");
    return (ERR_NOTSUPPORTED);
  }
  VkQueueFamilyProperties *pFamilyProperties =
      (VkQueueFamilyProperties *)malloc(queueFamilyCount *
                                        sizeof(VkQueueFamilyProperties));
  if (!pFamilyProperties) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL, "Failed to get device queue index: %s",
                   strerror(errno));
    PANIC();
  }
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                           pFamilyProperties);
  for (uint32_t i = 0; i < queueFamilyCount; i++) {
    if (pFamilyProperties[i].queueCount > 0 &&
        (pFamilyProperties[0].queueFlags & bit)) {
      free(pFamilyProperties);
      *pQueueFamilyIndex = i;
      return (ERR_OK);
    }
  }
  free(pFamilyProperties);
  LOG_ERROR(ERR_LEVEL_ERROR, "no suitable device queue found");
  return (ERR_NOTSUPPORTED);
}

ErrVal getPhysicalDevice(VkPhysicalDevice *pDevice, const VkInstance instance) {
  uint32_t deviceCount = 0;
  VkResult res = vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);
  if (res != VK_SUCCESS || deviceCount == 0) {
    LOG_ERROR(ERR_LEVEL_WARN, "no Vulkan capable device found");
    return (ERR_NOTSUPPORTED);
  }
  VkPhysicalDevice *arr = (VkPhysicalDevice *)malloc(deviceCount * sizeof(VkPhysicalDevice));
  if (!arr) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL, "failed to get physical device: %s",
                   strerror(errno));
    PANIC();
  }
  vkEnumeratePhysicalDevices(instance, &deviceCount, arr);

  VkPhysicalDeviceProperties deviceProperties;
  VkPhysicalDevice selectedDevice = VK_NULL_HANDLE;
  for (uint32_t i = 0; i < deviceCount; i++) {
    /* TODO confirm it has required properties */
    vkGetPhysicalDeviceProperties(arr[i], &deviceProperties);
    uint32_t deviceQueueIndex;
    uint32_t ret = getQueueFamilyIndexByCapability(&deviceQueueIndex, arr[i],
                                                   VK_QUEUE_GRAPHICS_BIT |
                                                       VK_QUEUE_COMPUTE_BIT);
    if (ret == VK_SUCCESS) {
      selectedDevice = arr[i];
      break;
    }
  }
  free(arr);
  if (selectedDevice == VK_NULL_HANDLE) {
    LOG_ERROR(ERR_LEVEL_WARN, "no suitable Vulkan device found");
    return (ERR_NOTSUPPORTED);
  } else {
    *pDevice = selectedDevice;
    return (ERR_OK);
  }
}

/**
 * Deletes VkDevice created in new_Device
 */
void delete_Device(VkDevice *pDevice) {
  vkDestroyDevice(*pDevice, NULL);
  *pDevice = VK_NULL_HANDLE;
};

ErrVal new_GlfwWindow(GLFWwindow **ppGlfwWindow, const char *name,
                      VkExtent2D dimensions) {
  /* Not resizable */
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  *ppGlfwWindow = glfwCreateWindow((int)dimensions.width,
                                   (int)dimensions.height, name, NULL, NULL);
  if (*ppGlfwWindow == NULL) {
    LOG_ERROR(ERR_LEVEL_ERROR, "failed to create GLFW window");
    return (ERR_UNKNOWN);
  }
  return (ERR_OK);
}

/* Creates a new window surface using the glfw libraries. This must be deleted
 * with the delete_Surface function*/
ErrVal new_SurfaceFromGLFW(VkSurfaceKHR *pSurface, GLFWwindow *pWindow,
                           const VkInstance instance) {
  VkResult res = glfwCreateWindowSurface(instance, pWindow, NULL, pSurface);
  if (res != VK_SUCCESS) {
    LOG_ERROR(ERR_LEVEL_FATAL, "failed to create surface, quitting");
    PANIC();
  }
  return (ERR_OK);
};

ErrVal getExtentWindow(VkExtent2D *pExtent, GLFWwindow *pWindow) {
  int width;
  int height;
  glfwGetFramebufferSize(pWindow, &width, &height);
  pExtent->width = (uint32_t)width;
  pExtent->height = (uint32_t)height;
  return (ERR_OK);
};

ErrVal new_Device(VkDevice *pDevice, const VkPhysicalDevice physicalDevice,
                  const uint32_t queueFamilyIndex,
                  const uint32_t enabledExtensionCount,
                  const char *const *ppEnabledExtensionNames) {
  VkPhysicalDeviceFeatures deviceFeatures {};
  VkDeviceQueueCreateInfo queueCreateInfo {};
  queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
  queueCreateInfo.queueCount = 1;
  float queuePriority = 1.0f;
  queueCreateInfo.pQueuePriorities = &queuePriority;

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pQueueCreateInfos = &queueCreateInfo;
  createInfo.queueCreateInfoCount = 1;
  createInfo.pEnabledFeatures = &deviceFeatures;
  createInfo.enabledExtensionCount = enabledExtensionCount;
  createInfo.ppEnabledExtensionNames = ppEnabledExtensionNames;
  createInfo.enabledLayerCount = 0;

  VkResult res = vkCreateDevice(physicalDevice, &createInfo, NULL, pDevice);
  if (res != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_ERROR, "Failed to create device, error code: %s",
                   vkstrerror(res));
    PANIC();
  }
  return (ERR_OK);
};
ErrVal getQueue(VkQueue *pQueue, const VkDevice device,
                const uint32_t deviceQueueIndex) {
  vkGetDeviceQueue(device, deviceQueueIndex, 0, pQueue);
  return (ERR_OK);
};

ErrVal new_CommandPool(VkCommandPool *pCommandPool, const VkDevice device,
                       const uint32_t queueFamilyIndex) {
  VkCommandPoolCreateInfo poolInfo {};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = queueFamilyIndex;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  VkResult ret = vkCreateCommandPool(device, &poolInfo, NULL, pCommandPool);
  if (ret != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_ERROR, "failed to create command pool %s",
                   vkstrerror(ret));
    return (ERR_UNKNOWN);
  }
  return (ERR_OK);
};


ErrVal getPreferredSurfaceFormat(VkSurfaceFormatKHR *pSurfaceFormat,
                                 const VkPhysicalDevice physicalDevice,
                                 const VkSurfaceKHR surface) {
  uint32_t formatCount = 0;
  VkSurfaceFormatKHR *pSurfaceFormats;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount,
                                       NULL);
  if (formatCount == 0) {
    return (ERR_NOTSUPPORTED);
  }

  pSurfaceFormats =
      (VkSurfaceFormatKHR *)malloc(formatCount * sizeof(VkSurfaceFormatKHR));
  if (!pSurfaceFormats) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL, "could not get preferred format: %s",
                   strerror(errno));
    PANIC();
  }
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount,
                                       pSurfaceFormats);

  VkSurfaceFormatKHR preferredFormat {};
  if (formatCount == 1 && pSurfaceFormats[0].format == VK_FORMAT_UNDEFINED) {
    /* If it has no preference, use our own */
    preferredFormat = pSurfaceFormats[0];
  } else if (formatCount != 0) {
    /* we default to the first one in the list */
    preferredFormat = pSurfaceFormats[0];
    /* However,  we check to make sure that what we want is in there
     */
    for (uint32_t i = 0; i < formatCount; i++) {
      VkSurfaceFormatKHR availableFormat = pSurfaceFormats[i];
      if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
          availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        preferredFormat = availableFormat;
      }
    }
  } else {
    LOG_ERROR(ERR_LEVEL_ERROR, "no formats available");
    free(pSurfaceFormats);
    return (ERR_NOTSUPPORTED);
  }

  free(pSurfaceFormats);

  *pSurfaceFormat = preferredFormat;
  return (ERR_OK);
};

ErrVal new_Swapchain(VkSwapchainKHR *pSwapchain, uint32_t *pImageCount,
                     const VkSwapchainKHR oldSwapchain,
                     const VkSurfaceFormatKHR surfaceFormat,
                     const VkPhysicalDevice physicalDevice,
                     const VkDevice device, const VkSurfaceKHR surface,
                     const VkExtent2D extent, const uint32_t graphicsIndex,
                     const uint32_t presentIndex) {
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                            &capabilities);

  // it's important to note that minImageCount isn't necessarily the size of the
  // swapchain we get
  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = surface;
  createInfo.minImageCount = capabilities.minImageCount + 1;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  uint32_t queueFamilyIndices[] = {graphicsIndex, presentIndex};
  if (graphicsIndex != presentIndex) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;  /* Optional */
    createInfo.pQueueFamilyIndices = NULL; /* Optional */
  }

  createInfo.preTransform = capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  /* guaranteed to be available */
  createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = oldSwapchain;
  VkResult res = vkCreateSwapchainKHR(device, &createInfo, NULL, pSwapchain);
  if (res != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_ERROR,
                   "Failed to create swap chain, error code: %s",
                   vkstrerror(res));
    PANIC();
  }

  VkResult imageCountRes =
      vkGetSwapchainImagesKHR(device, *pSwapchain, pImageCount, NULL);
  if (imageCountRes != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_ERROR,
                   "Failed to retrieve swap chain image count, error code: %s",
                   vkstrerror(res));
    PANIC();
  }

  return (ERR_OK);
};
ErrVal getSwapchainImages(         //
    VkImage *pSwapchainImages,     //
    const uint32_t imageCount,     //
    const VkDevice device,         //
    const VkSwapchainKHR swapchain //
) {

  // we are going to try to write in imageCount images, but its possible that
  // we get less or more
  uint32_t imagesWritten = imageCount;
  VkResult res = vkGetSwapchainImagesKHR(device, swapchain, &imagesWritten,
                                         pSwapchainImages);
  if (res != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_ERROR,
                   "failed to get swap chain images, error: %s",
                   vkstrerror(res));
    return (ERR_UNKNOWN);
  }

  if (imagesWritten != imageCount) {
    LOG_ERROR_ARGS(ERR_LEVEL_ERROR, "expected %u images, but only %u written",
                   imageCount, imagesWritten);
  }

  return (ERR_OK);
};

ErrVal new_ImageView(VkImageView *pImageView, const VkDevice device,
                     const VkImage image, const VkFormat format,
                     const uint32_t aspectMask) {
  VkImageViewCreateInfo createInfo  {};
  createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  createInfo.image = image;
  createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  createInfo.format = format;
  createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.subresourceRange.aspectMask = aspectMask;
  createInfo.subresourceRange.baseMipLevel = 0;
  createInfo.subresourceRange.levelCount = 1;
  createInfo.subresourceRange.baseArrayLayer = 0;
  createInfo.subresourceRange.layerCount = 1;
  VkResult ret = vkCreateImageView(device, &createInfo, NULL, pImageView);
  if (ret != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL,
                   "could not create image view, error code: %s",
                   vkstrerror(ret));
    PANIC();
  }
  return (ERR_OK);
};

void delete_ImageView(VkImageView *pImageView, VkDevice device) {
  vkDestroyImageView(device, *pImageView, NULL);
  *pImageView = VK_NULL_HANDLE;
}

void delete_SwapchainImageViews( //
    VkImageView *pImageViews,    //
    const uint32_t imageCount,   //
    const VkDevice device        //
) {
  for (uint32_t i = 0; i < imageCount; i++) {
    delete_ImageView(&pImageViews[i], device);
  }
}


ErrVal new_SwapchainImageViews(      //
    VkImageView *pImageViews,        //
    const VkImage *pSwapchainImages, //
    const uint32_t imageCount,       //
    const VkDevice device,           //
    const VkFormat format            //
) {
  for (uint32_t i = 0; i < imageCount; i++) {
    ErrVal ret = new_ImageView(&(pImageViews[i]), device, pSwapchainImages[i],
                               format, VK_IMAGE_ASPECT_COLOR_BIT);
    if (ret != ERR_OK) {
      LOG_ERROR(ERR_LEVEL_ERROR, "could not create swap chain image views");
      delete_SwapchainImageViews(pImageViews, i, device);
      return (ret);
    }
  }
  return (ERR_OK);
};

ErrVal getMemoryTypeIndex(uint32_t *memoryTypeIndex,
                          const uint32_t memoryTypeBits,
                          const VkMemoryPropertyFlags memoryPropertyFlags,
                          const VkPhysicalDevice physicalDevice) {

  /* Retrieve memory properties */
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
  /* Check each memory type to see if it conforms to our requirements */
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((memoryTypeBits &
         (1 << i)) && /* TODO figure out what's going on over here */
        (memProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) ==
            memoryPropertyFlags) {
      *memoryTypeIndex = i;
      return (ERR_OK);
    }
  }
  LOG_ERROR(ERR_LEVEL_ERROR, "failed to find suitable memory type");
  return (ERR_MEMORY);
}

ErrVal new_Image(                           //
    VkImage *pImage,                        //
    VkDeviceMemory *pImageMemory,           //
    const VkExtent2D dimensions,            //
    const VkFormat format,                  //
    const VkImageTiling tiling,             //
    const VkImageUsageFlags usage,          //
    const VkMemoryPropertyFlags properties, //
    const VkPhysicalDevice physicalDevice,  //
    const VkDevice device                   //
) {
  VkImageCreateInfo imageInfo {};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = dimensions.width;
  imageInfo.extent.height = dimensions.height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = format;
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VkResult createImageResult = vkCreateImage(device, &imageInfo, NULL, pImage);
  if (createImageResult != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_ERROR, "failed to create image: %s",
                   vkstrerror(createImageResult));
    return (ERR_UNKNOWN);
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(device, *pImage, &memRequirements);

  VkMemoryAllocateInfo allocInfo {};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;

  ErrVal memGetResult = getMemoryTypeIndex(&allocInfo.memoryTypeIndex,
                                           memRequirements.memoryTypeBits,
                                           properties, physicalDevice);

  if (memGetResult != ERR_OK) {
    LOG_ERROR(ERR_LEVEL_ERROR, "failed to create image: allocation failed");
    return (ERR_MEMORY);
  }

  VkResult allocateResult =
      vkAllocateMemory(device, &allocInfo, NULL, pImageMemory);
  if (allocateResult != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_ERROR, "failed to create image: %s",
                   vkstrerror(allocateResult));
    return (ERR_MEMORY);
  }

  VkResult bindResult = vkBindImageMemory(device, *pImage, *pImageMemory, 0);
  if (bindResult != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_ERROR, "failed to create image: %s",
                   vkstrerror(bindResult));
    return (ERR_UNKNOWN);
  }
  return (ERR_OK);
}

void delete_Image(VkImage *pImage, const VkDevice device) {
  vkDestroyImage(device, *pImage, NULL);
}

/* Gets image format of depth */
void getDepthFormat(VkFormat *pFormat) {
  /* TODO we might want to redo this so that there are more compatible images */
  *pFormat = VK_FORMAT_D32_SFLOAT;
}

ErrVal new_DepthImage(VkImage *pImage, VkDeviceMemory *pImageMemory,
                      const VkExtent2D swapchainExtent,
                      const VkPhysicalDevice physicalDevice,
                      const VkDevice device) {
  VkFormat depthFormat {};
  getDepthFormat(&depthFormat);
  ErrVal retVal = new_Image(
      pImage, pImageMemory, swapchainExtent, depthFormat,
      VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, physicalDevice, device);
  if (retVal != ERR_OK) {
    LOG_ERROR(ERR_LEVEL_ERROR, "failed to create depth image");
    return (retVal);
  }

  return (ERR_OK);
}

ErrVal new_DepthImageView(VkImageView *pImageView, const VkDevice device,
                          const VkImage depthImage) {
  VkFormat depthFormat;
  getDepthFormat(&depthFormat);
  ErrVal retVal = new_ImageView(pImageView, device, depthImage, depthFormat,
                                VK_IMAGE_ASPECT_DEPTH_BIT);
  if (retVal != ERR_OK) {
    LOG_ERROR(ERR_LEVEL_ERROR, "failed to create depth image view");
  }
  return (retVal);
};

ErrVal new_ShaderModule(VkShaderModule *pShaderModule, const VkDevice device,
                        const uint32_t codeSize, const uint32_t *pCode) {
  VkShaderModuleCreateInfo createInfo {};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = codeSize;
  createInfo.pCode = pCode;
  VkResult res = vkCreateShaderModule(device, &createInfo, NULL, pShaderModule);
  if (res != VK_SUCCESS) {
    LOG_ERROR(ERR_LEVEL_FATAL, "failed to create shader module");
    return (ERR_UNKNOWN);
  }
  return (ERR_OK);
};

ErrVal new_VertexDisplayRenderPass(VkRenderPass *pRenderPass,
                                   const VkDevice device,
                                   const VkFormat swapchainImageFormat) {
  VkAttachmentDescription colorAttachment {};
  colorAttachment.format = swapchainImageFormat;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentDescription depthAttachment {};
  getDepthFormat(&depthAttachment.format);
  depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription pAttachments[2];
  pAttachments[0] = colorAttachment;
  pAttachments[1] = depthAttachment;

  VkAttachmentReference colorAttachmentRef {};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthAttachmentRef {};
  depthAttachmentRef.attachment = 1;
  depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

  VkRenderPassCreateInfo renderPassInfo {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 2;
  renderPassInfo.pAttachments = pAttachments;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;

  VkSubpassDependency dependency {};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  VkResult res = vkCreateRenderPass(device, &renderPassInfo, NULL, pRenderPass);
  if (res != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL, "Could not create render pass, error: %s",
                   vkstrerror(res));
    PANIC();
  }
  return (ERR_OK);
};

ErrVal new_VertexDisplayPipelineLayout(VkPipelineLayout *pPipelineLayout,
                                       const VkDevice device) {
  VkPushConstantRange pushConstantRange {};
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(mat4x4);
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 0;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
  VkResult res = vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL,
                                        pPipelineLayout);
  if (res != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL,
                   "failed to create pipeline layout with error: %s",
                   vkstrerror(res));
    PANIC();
  }
  return (ERR_OK);
}

void delete_PipelineLayout(VkPipelineLayout *pPipelineLayout,
                           const VkDevice device) {
  vkDestroyPipelineLayout(device, *pPipelineLayout, NULL);
  *pPipelineLayout = VK_NULL_HANDLE;
}

ErrVal new_VertexDisplayPipeline(VkPipeline *pGraphicsPipeline,
                                 const VkDevice device,
                                 const VkShaderModule vertShaderModule,
                                 const VkShaderModule fragShaderModule,
                                 const VkExtent2D extent,
                                 const VkRenderPass renderPass,
                                 const VkPipelineLayout pipelineLayout) {
  VkPipelineShaderStageCreateInfo vertShaderStageInfo {};
  vertShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vertShaderModule;
  vertShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo fragShaderStageInfo {};
  fragShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo shaderStages[2] = {vertShaderStageInfo,
                                                     fragShaderStageInfo};

  VkVertexInputBindingDescription bindingDescription {};
  bindingDescription.binding = 0;
  bindingDescription.stride = sizeof(Vertex);
  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription attributeDescriptions[2];

  attributeDescriptions[0].binding = 0;
  attributeDescriptions[0].location = 0;
  attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[0].offset = offsetof(Vertex, position);

  attributeDescriptions[1].binding = 0;
  attributeDescriptions[1].location = 1;
  attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[1].offset = offsetof(Vertex, color);

  VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.vertexAttributeDescriptionCount = 2;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkViewport viewport {};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)extent.width;
  viewport.height = (float)extent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor {};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent = extent;

  VkPipelineDepthStencilStateCreateInfo depthStencil {};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.stencilTestEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo viewportState {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer {};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_NONE; /* VK_CULL_MODE_BACK_BIT; */
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling {};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState colorBlendAttachment {};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo colorBlending {};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f;
  colorBlending.blendConstants[1] = 0.0f;
  colorBlending.blendConstants[2] = 0.0f;
  colorBlending.blendConstants[3] = 0.0f;

  VkGraphicsPipelineCreateInfo pipelineInfo {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.layout = pipelineLayout;
  pipelineInfo.renderPass = renderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL,
                                pGraphicsPipeline) != VK_SUCCESS) {
    LOG_ERROR(ERR_LEVEL_FATAL, "failed to create graphics pipeline!");
    PANIC();
  }
  return (ERR_OK);
}

void delete_Pipeline(VkPipeline *pPipeline, const VkDevice device) {
  vkDestroyPipeline(device, *pPipeline, NULL);
}

ErrVal new_Framebuffer(VkFramebuffer *pFramebuffer, const VkDevice device,
                       const VkRenderPass renderPass,
                       const VkImageView imageView,
                       const VkImageView depthImageView,
                       const VkExtent2D swapchainExtent) {
  VkFramebufferCreateInfo framebufferInfo {};
  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass = renderPass;
  framebufferInfo.attachmentCount = 2;
  VkImageView view[] = {imageView, depthImageView};
  framebufferInfo.pAttachments = view;//(VkImageView[]){imageView, depthImageView};//( VkImageView *)imageView, depthImageView;
  framebufferInfo.width = swapchainExtent.width;
  framebufferInfo.height = swapchainExtent.height;
  framebufferInfo.layers = 1;
  VkResult res =
      vkCreateFramebuffer(device, &framebufferInfo, NULL, pFramebuffer);
  if (res == VK_SUCCESS) {
    return (ERR_OK);
  } else {
    LOG_ERROR_ARGS(ERR_LEVEL_WARN, "failed to create framebuffers: %s",
                   vkstrerror(res));
    return (ERR_UNKNOWN);
  }
}

void delete_Framebuffer(VkFramebuffer *pFramebuffer, VkDevice device) {
  vkDestroyFramebuffer(device, *pFramebuffer, NULL);
  *pFramebuffer = VK_NULL_HANDLE;
}

ErrVal new_SwapchainFramebuffers(VkFramebuffer *pFramebuffers,
                                 const VkDevice device,
                                 const VkRenderPass renderPass,
                                 const VkExtent2D swapchainExtent,
                                 const uint32_t imageCount,
                                 const VkImageView depthImageView,
                                 const VkImageView *pSwapchainImageViews) {
  for (uint32_t i = 0; i < imageCount; i++) {
    ErrVal retVal = new_Framebuffer(&pFramebuffers[i], device, renderPass,
                                    pSwapchainImageViews[i], depthImageView,
                                    swapchainExtent);
    if (retVal != ERR_OK) {
      LOG_ERROR(ERR_LEVEL_ERROR, "could not create framebuffers");
     // delete_SwapchainFramebuffers(pFramebuffers, i, device);
      return (retVal);
    }
  }
  return (ERR_OK);
}

/*void delete_SwapchainFramebuffers(VkFramebuffer *pFramebuffers,
                                  const uint32_t imageCount,
                                  const VkDevice device) {
  for (uint32_t i = 0; i < imageCount; i++) {
    //delete_Framebuffer(&pFramebuffers[i], device);
  }
}*/

/*void delete_CommandPool(VkCommandPool *pCommandPool, const VkDevice device) {
  vkDestroyCommandPool(device, *pCommandPool, NULL);
}
*/
ErrVal recordVertexDisplayCommandBuffer(                //
    VkCommandBuffer commandBuffer,                      //
    const VkFramebuffer swapchainFramebuffer,           //
    const VkBuffer vertexBuffer,                        //
    const uint32_t vertexCount,                         //
    const VkRenderPass renderPass,                      //
    const VkPipelineLayout vertexDisplayPipelineLayout, //
    const VkPipeline vertexDisplayPipeline,             //
    const VkExtent2D swapchainExtent,                   //
    const mat4x4 cameraTransform,                       //
    const VkClearColorValue clearColor                  //
) {
  VkCommandBufferBeginInfo beginInfo {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VkResult beginRet = vkBeginCommandBuffer(commandBuffer, &beginInfo);

  if (beginRet != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL,
                   "failed to record into graphics command buffer: %s",
                   vkstrerror(beginRet));
    PANIC();
  }

  VkRenderPassBeginInfo renderPassInfo {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = renderPass;
  renderPassInfo.framebuffer = swapchainFramebuffer;
  renderPassInfo.renderArea.offset = (VkOffset2D){0, 0};
  renderPassInfo.renderArea.extent = swapchainExtent;

  VkClearValue pClearColors[2];
  pClearColors[0].color = clearColor;
  pClearColors[1].depthStencil.depth = 1.0f;
  pClearColors[1].depthStencil.stencil = 0;

  renderPassInfo.clearValueCount = 2;
  renderPassInfo.pClearValues = pClearColors;

  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    vertexDisplayPipeline);
  vkCmdPushConstants(commandBuffer, vertexDisplayPipelineLayout,
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4x4),
                     cameraTransform);

  VkBuffer vertexBuffers[] = {vertexBuffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

  vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
  vkCmdEndRenderPass(commandBuffer);

  VkResult endCommandBufferRetVal = vkEndCommandBuffer(commandBuffer);
  if (endCommandBufferRetVal != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL,
                   "Failed to record command buffer, error code: %s",
                   vkstrerror(endCommandBufferRetVal));
    PANIC();
  }
  return (ERR_OK);
}

ErrVal new_Semaphore(VkSemaphore *pSemaphore, const VkDevice device) {
  VkSemaphoreCreateInfo semaphoreInfo {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VkResult ret = vkCreateSemaphore(device, &semaphoreInfo, NULL, pSemaphore);
  if (ret != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_ERROR, "failed to create semaphore: %s",
                   vkstrerror(ret));
    return (ERR_UNKNOWN);
  }
  return (ERR_OK);
}

void delete_Semaphore(VkSemaphore *pSemaphore, const VkDevice device) {
  vkDestroySemaphore(device, *pSemaphore, NULL);
  *pSemaphore = VK_NULL_HANDLE;
}

void delete_Semaphores(VkSemaphore *pSemaphores, const uint32_t semaphoreCount,
                       const VkDevice device) {
  for (uint32_t i = 0; i < semaphoreCount; i++) {
    delete_Semaphore(&pSemaphores[i], device);
  }
}

ErrVal new_Semaphores(VkSemaphore *pSemaphores, const uint32_t semaphoreCount,
                      const VkDevice device) {
  for (uint32_t i = 0; i < semaphoreCount; i++) {
    ErrVal retVal = new_Semaphore(&pSemaphores[i], device);
    if (retVal != ERR_OK) {
      delete_Semaphores(pSemaphores, i, device);
      return (retVal);
    }
  }
  return (ERR_OK);
}

// Note we're creating the fence already signaled!
ErrVal new_Fence(VkFence *pFence, const VkDevice device, const bool signaled) {
  VkFenceCreateInfo fenceInfo {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  if (signaled) {
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  }
  VkResult ret = vkCreateFence(device, &fenceInfo, NULL, pFence);
  if (ret != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL, "failed to create fence: %s",
                   vkstrerror(ret));
    PANIC();
  }
  return (ERR_OK);
}

void delete_Fence(VkFence *pFence, const VkDevice device) {
  vkDestroyFence(device, *pFence, NULL);
  *pFence = VK_NULL_HANDLE;
};

void delete_Fences(VkFence *pFences, const uint32_t fenceCount,
                   const VkDevice device) {
  for (uint32_t i = 0; i < fenceCount; i++) {
    delete_Fence(&pFences[i], device);
  }
}

ErrVal new_Fences( //
    VkFence *pFences,  //
    const uint32_t fenceCount, //
    const VkDevice device, //
    const bool allSignaled //
) {
  for (uint32_t i = 0; i < fenceCount; i++) {
    ErrVal retVal = new_Fence(&pFences[i], device, allSignaled);
    if (retVal != ERR_OK) {
      /* Clean up memory */
     delete_Fences(pFences, i, device);
      return (retVal);
    }
  }
  return (ERR_OK);
}

ErrVal waitAndResetFence(VkFence fence, const VkDevice device) {
  // Wait for the current frame to finish processing
  VkResult waitRet = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
  if (waitRet != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL, "failed to wait for fence: %s",
                   vkstrerror(waitRet));
    PANIC();
  }

  // reset the fence
  VkResult resetRet = vkResetFences(device, 1, &fence);
  if (resetRet != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL, "failed to reset fence: %s",
                   vkstrerror(resetRet));
    PANIC();
  }

  return (ERR_OK);
}

ErrVal getNextSwapchainImage(           //
    uint32_t *pImageIndex,              //
    const VkSwapchainKHR swapchain,     //
    const VkDevice device,              //
    VkSemaphore imageAvailableSemaphore //
) {
  // get the next image from the swapchain
  VkResult nextImageResult = vkAcquireNextImageKHR(
      device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE,
      pImageIndex);
  if (nextImageResult == VK_ERROR_OUT_OF_DATE_KHR) {
    // If the window has been resized, the result will be an out of date error,
    // meaning that the swap chain must be resized
    return (ERR_OUTOFDATE);
  } else if (nextImageResult != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL, "failed to get next frame: %s",
                   vkstrerror(nextImageResult));
    PANIC();
  }

  return (ERR_OK);
}

// Draws a frame to the surface provided, and sets things up for the next frame
ErrVal drawFrame(                        //
    VkCommandBuffer commandBuffer,       //
    VkSwapchainKHR swapchain,            //
    const uint32_t swapchainImageIndex,  //
    VkSemaphore imageAvailableSemaphore, //
    VkSemaphore renderFinishedSemaphore, //
    VkFence inFlightFence,               //
    const VkQueue graphicsQueue,         //
    const VkQueue presentQueue           //
) {

  // Sets up for next frame
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  VkSubmitInfo submitInfo {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

  VkResult queueSubmitResult =
      vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence);
  if (queueSubmitResult != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL, "failed to submit queue: %s",
                   vkstrerror(queueSubmitResult));
    PANIC();
  }

  // Present frame to screen
  VkPresentInfoKHR presentInfo {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &swapchain;
  presentInfo.pImageIndices = &swapchainImageIndex;
  vkQueuePresentKHR(presentQueue, &presentInfo);

  return (ERR_OK);
}

// Deletes a VkSurfaceKHR
/*void delete_Surface(VkSurfaceKHR *pSurface, const VkInstance instance) {
  vkDestroySurfaceKHR(instance, *pSurface, NULL);
  *pSurface = VK_NULL_HANDLE;
}
*/

ErrVal new_CommandBuffers(             //
    VkCommandBuffer *pCommandBuffer,   //
    const uint32_t commandBufferCount, //
    const VkCommandPool commandPool,   //
    const VkDevice device              //
) {
  VkCommandBufferAllocateInfo allocateInfo {};
  allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocateInfo.commandPool = commandPool;
  allocateInfo.commandBufferCount = commandBufferCount;

  VkResult allocateResult =
      vkAllocateCommandBuffers(device, &allocateInfo, pCommandBuffer);
  if (allocateResult != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL, "failed to allocate command buffers: %s",
                   vkstrerror(allocateResult));
    PANIC();
  }

  return (ERR_OK);
}


ErrVal new_Buffer_DeviceMemory(VkBuffer *pBuffer, VkDeviceMemory *pBufferMemory,
                               const VkDeviceSize size,
                               const VkPhysicalDevice physicalDevice,
                               const VkDevice device,
                               const VkBufferUsageFlags usage,
                               const VkMemoryPropertyFlags properties) {
  VkBufferCreateInfo bufferInfo {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  /* Create buffer */
  VkResult bufferCreateResult =
      vkCreateBuffer(device, &bufferInfo, NULL, pBuffer);
  if (bufferCreateResult != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_ERROR, "failed to create buffer: %s",
                   vkstrerror(bufferCreateResult));
    return (ERR_UNKNOWN);
  }
  /* Allocate memory for buffer */
  VkMemoryRequirements memoryRequirements;
  vkGetBufferMemoryRequirements(device, *pBuffer, &memoryRequirements);

  VkMemoryAllocateInfo allocateInfo {};
  allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocateInfo.allocationSize = memoryRequirements.size;
  /* Get the type of memory required, handle errors */
  ErrVal getMemoryTypeRetVal = getMemoryTypeIndex(
      &allocateInfo.memoryTypeIndex, memoryRequirements.memoryTypeBits,
      properties, physicalDevice);
  if (getMemoryTypeRetVal != ERR_OK) {
    LOG_ERROR(ERR_LEVEL_ERROR, "failed to get type of memory to allocate");
    return (ERR_MEMORY);
  }

  /* Actually allocate memory */
  VkResult memoryAllocateResult =
      vkAllocateMemory(device, &allocateInfo, NULL, pBufferMemory);
  if (memoryAllocateResult != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_ERROR, "failed to allocate memory for buffer: %s",
                   vkstrerror(memoryAllocateResult));
    return (ERR_ALLOCFAIL);
  }
  vkBindBufferMemory(device, *pBuffer, *pBufferMemory, 0);
  return (ERR_OK);
}


// submits a copy to the queue, you'll later need to wait for idle
ErrVal copyBuffer(VkBuffer destinationBuffer, const VkBuffer sourceBuffer,
                  const VkDeviceSize size, const VkCommandPool commandPool,
                  const VkQueue queue, const VkDevice device) {
  VkCommandBuffer copyCommandBuffer;
  ErrVal createResult =
      new_CommandBuffers(&copyCommandBuffer, 1, commandPool, device);
  if (createResult != ERR_OK) {
  }

  VkCommandBufferBeginInfo beginInfo {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VkResult beginRet = vkBeginCommandBuffer(copyCommandBuffer, &beginInfo);
  if (beginRet != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL, "Failed to begin copy command buffer: %s",
                   vkstrerror(beginRet));
    PANIC();
  }

VkBufferCopy copyRegion = { .srcOffset = 0, .dstOffset = 0, .size = size};

  //VkBufferCopy copyRegion = {.size = size, .srcOffset = 0, .dstOffset = 0};
  vkCmdCopyBuffer(copyCommandBuffer, sourceBuffer, destinationBuffer, 1,
                  &copyRegion);

  // End buffer
  VkResult bufferEndResult = vkEndCommandBuffer(copyCommandBuffer);
  if (bufferEndResult != VK_SUCCESS) {
    /* Clean up resources */
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL, "failed to end command buffer: %s",
                   vkstrerror(bufferEndResult));
    PANIC();
  }

  // wait for completion with a fence
  VkFence fence;
  if (new_Fence(&fence, device, false) != ERR_OK) {
    LOG_ERROR(ERR_LEVEL_FATAL, "Failed to make copy fence");
    PANIC();
  }

  VkSubmitInfo submitInfo {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &copyCommandBuffer;

  VkResult queueSubmitResult = vkQueueSubmit(queue, 1, &submitInfo, fence);
  if (queueSubmitResult != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL,
                   "failed to submit command buffer to queue: %s",
                   vkstrerror(queueSubmitResult));
    PANIC();
  }

  VkResult waitRet = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
  if (waitRet != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_FATAL, "failed to wait for fence: %s",
                   vkstrerror(waitRet));
    PANIC();
  }

  delete_Fence(&fence, device);

  return (ERR_OK);
};

void delete_Buffer(VkBuffer *pBuffer, const VkDevice device) {
  vkDestroyBuffer(device, *pBuffer, NULL);
  *pBuffer = VK_NULL_HANDLE;
}

void delete_DeviceMemory(VkDeviceMemory *pDeviceMemory, const VkDevice device) {
  vkFreeMemory(device, *pDeviceMemory, NULL);
  *pDeviceMemory = VK_NULL_HANDLE;
}

ErrVal copyToDeviceMemory(VkDeviceMemory *pDeviceMemory,
                          const VkDeviceSize deviceSize, const void *source,
                          const VkDevice device) {
  void *data;
  VkResult mapMemoryResult =
      vkMapMemory(device, *pDeviceMemory, 0, deviceSize, 0, &data);

  /* On failure */
  if (mapMemoryResult != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_ERROR,
                   "failed to copy to device memory: failed to map memory: %s",
                   vkstrerror(mapMemoryResult));
    return (ERR_MEMORY);
  }

  /* If it was successful, go on and actually copy it, making sure to unmap once
   * done */
  memcpy(data, source, (size_t)deviceSize);
  vkUnmapMemory(device, *pDeviceMemory);
  return (ERR_OK);
}

ErrVal new_VertexBuffer(VkBuffer *pBuffer, VkDeviceMemory *pBufferMemory,
                        const Vertex *pVertices, const uint32_t vertexCount,
                        const VkDevice device,
                        const VkPhysicalDevice physicalDevice,
                        const VkCommandPool commandPool, const VkQueue queue) {
  /* Construct staging buffers */
  VkDeviceSize bufferSize = sizeof(Vertex) * vertexCount;
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  ErrVal stagingBufferCreateResult = new_Buffer_DeviceMemory(
      &stagingBuffer, &stagingBufferMemory, bufferSize, physicalDevice, device,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  if (stagingBufferCreateResult != ERR_OK) {
    LOG_ERROR(
        ERR_LEVEL_ERROR,
        "failed to create vertex buffer: failed to create staging buffer");
    return (stagingBufferCreateResult);
  }

  /* Copy data to staging buffer, making sure to clean up leaks */
  ErrVal copyResult =
      copyToDeviceMemory(&stagingBufferMemory, bufferSize, pVertices, device);
  if (copyResult != ERR_OK) {
    LOG_ERROR(ERR_LEVEL_ERROR,
              "failed to create vertex buffer: could not map memory");
    delete_Buffer(&stagingBuffer, device);
    delete_DeviceMemory(&stagingBufferMemory, device);
    return (copyResult);
  }

  /* Create vertex buffer and allocate memory for it */
  ErrVal vertexBufferCreateResult = new_Buffer_DeviceMemory(
      pBuffer, pBufferMemory, bufferSize, physicalDevice, device,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  /* Handle errors */
  if (vertexBufferCreateResult != ERR_OK) {
    /* Delete the temporary staging buffers */
    LOG_ERROR(ERR_LEVEL_ERROR, "failed to create vertex buffer");
    delete_Buffer(&stagingBuffer, device);
    delete_DeviceMemory(&stagingBufferMemory, device);
    return (vertexBufferCreateResult);
  }

  /* Copy the data over from the staging buffer to the vertex buffer */
  copyBuffer(*pBuffer, stagingBuffer, bufferSize, commandPool, queue, device);

  /* Delete the temporary staging buffers */
  delete_Buffer(&stagingBuffer, device);
  delete_DeviceMemory(&stagingBufferMemory, device);

  return (ERR_OK);
}



// creates a command buffer that hasn't yet been begun

void delete_CommandBuffers(            //
    VkCommandBuffer *pCommandBuffers,  //
    const uint32_t commandBufferCount, //
    const VkCommandPool commandPool,   //
    const VkDevice device              //
) {
  vkFreeCommandBuffers(device, commandPool, commandBufferCount,
                       pCommandBuffers);
  for (uint32_t i = 0; i < commandBufferCount; i++) {
    pCommandBuffers[i] = VK_NULL_HANDLE;
  }
}




ErrVal new_ComputePipeline(VkPipeline *pPipeline,
                           const VkPipelineLayout pipelineLayout,
                           const VkShaderModule shaderModule,
                           const VkDevice device) {

  VkPipelineShaderStageCreateInfo shaderStageCreateInfo {};
  shaderStageCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStageCreateInfo.module = shaderModule;
  shaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  shaderStageCreateInfo.pName = "main";

  VkComputePipelineCreateInfo computePipelineCreateInfo {};
  computePipelineCreateInfo.sType =
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.layout = pipelineLayout;
  computePipelineCreateInfo.stage = shaderStageCreateInfo;

  VkResult ret = vkCreateComputePipelines(
      device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, NULL, pPipeline);
  if (ret != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_ERROR, "failed to create compute pipelines %s",
                   vkstrerror(ret));
    return (ERR_UNKNOWN);
  }
  return (ERR_OK);
}

ErrVal new_ComputeStorageDescriptorSetLayout(
    VkDescriptorSetLayout *pDescriptorSetLayout, const VkDevice device) {
  VkDescriptorSetLayoutBinding storageLayoutBinding {};
  storageLayoutBinding.binding = 0;
  storageLayoutBinding.descriptorCount = 1;
  storageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  storageLayoutBinding.pImmutableSamplers = NULL;
  storageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  VkDescriptorSetLayoutCreateInfo layoutInfo {};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &storageLayoutBinding;
  VkResult retVal = vkCreateDescriptorSetLayout(device, &layoutInfo, NULL,
                                                pDescriptorSetLayout);
  if (retVal != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_ERROR,
                   "failed to create descriptor set layout: %s",
                   vkstrerror(retVal));
    return (ERR_UNKNOWN);
  }
  return (ERR_OK);
}

void delete_DescriptorSetLayout(VkDescriptorSetLayout *pDescriptorSetLayout,
                                const VkDevice device) {
  vkDestroyDescriptorSetLayout(device, *pDescriptorSetLayout, NULL);
  *pDescriptorSetLayout = VK_NULL_HANDLE;
}

ErrVal new_DescriptorPool(VkDescriptorPool *pDescriptorPool,
                          const VkDescriptorType descriptorType,
                          const uint32_t maxAllocFrom, const VkDevice device) {
  VkDescriptorPoolSize descriptorPoolSize;
  descriptorPoolSize.type = descriptorType;
  descriptorPoolSize.descriptorCount = maxAllocFrom;

  VkDescriptorPoolCreateInfo poolInfo {};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &descriptorPoolSize;
  poolInfo.maxSets = maxAllocFrom;

  /* Actually create descriptor pool */
  VkResult ret =
      vkCreateDescriptorPool(device, &poolInfo, NULL, pDescriptorPool);

  if (ret != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_ERROR, "failed to create descriptor pool; %s",
                   vkstrerror(ret));
    return (ERR_UNKNOWN);
  } else {
    return (ERR_OK);
  }
}

void delete_DescriptorPool(VkDescriptorPool *pDescriptorPool,
                           const VkDevice device) {
  vkDestroyDescriptorPool(device, *pDescriptorPool, NULL);
  *pDescriptorPool = VK_NULL_HANDLE;
}

ErrVal new_ComputeBufferDescriptorSet(
    VkDescriptorSet *pDescriptorSet, const VkBuffer computeBufferDescriptorSet,
    const VkDeviceSize computeBufferSize,
    const VkDescriptorSetLayout descriptorSetLayout,
    const VkDescriptorPool descriptorPool, const VkDevice device) {

  VkDescriptorSetAllocateInfo allocateInfo {};
  allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocateInfo.descriptorPool = descriptorPool;
  allocateInfo.descriptorSetCount = 1;
  allocateInfo.pSetLayouts = &descriptorSetLayout;
  VkResult allocateDescriptorSetRetVal =
      vkAllocateDescriptorSets(device, &allocateInfo, pDescriptorSet);
  if (allocateDescriptorSetRetVal != VK_SUCCESS) {
    LOG_ERROR_ARGS(ERR_LEVEL_ERROR, "failed to allocate descriptor sets: %s",
                   vkstrerror(allocateDescriptorSetRetVal));
    return (ERR_MEMORY);
  }

  VkDescriptorBufferInfo bufferInfo {};
  bufferInfo.buffer = computeBufferDescriptorSet;
  bufferInfo.range = computeBufferSize;
  bufferInfo.offset = 0;

  VkWriteDescriptorSet descriptorWrites {};
  descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrites.dstSet = *pDescriptorSet;
  descriptorWrites.dstBinding = 0;
  descriptorWrites.dstArrayElement = 0;
  descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  descriptorWrites.descriptorCount = 1;
  descriptorWrites.pBufferInfo = &bufferInfo;
  descriptorWrites.pImageInfo = NULL;
  descriptorWrites.pTexelBufferView = NULL;
  vkUpdateDescriptorSets(device, 1, &descriptorWrites, 0, NULL);
  return (ERR_OK);
}

void delete_DescriptorSets(VkDescriptorSet **ppDescriptorSets) {
  free(*ppDescriptorSets);
  *ppDescriptorSets = NULL;
}

int main(){
glfwInit();

  const uint32_t validationLayerCount = 1;
  const char *ppValidationLayerNames[1] = {"VK_LAYER_KHRONOS_validation"};

 //  Create instance 
  //VkInstance instance;
  new_Instance(&context.instance, validationLayerCount, ppValidationLayerNames, 0, NULL,
               true, true, "herro");

 // Enable vulkan logging to stdout 
 
  new_DebugCallback(&context.callback, context.instance);

  //VkPhysicalDevice physicalDevice;
  getPhysicalDevice(&context.physicalDevice, context.instance);

 
  //GLFWwindow *pWindow;
  new_GlfwWindow(&context.pWindow, "error",
                 (VkExtent2D){.width = WINDOW_WIDTH, .height = WINDOW_HEIGHT});
 // VkSurfaceKHR surface;
  new_SurfaceFromGLFW(&context.surface, context.pWindow, context.instance);

 
  uint32_t graphicsIndex;
  uint32_t computeIndex;
  uint32_t presentIndex;
  {
    uint32_t ret1 = getQueueFamilyIndexByCapability(
        &graphicsIndex, context.physicalDevice, VK_QUEUE_GRAPHICS_BIT);
    uint32_t ret2 = getQueueFamilyIndexByCapability(
        &computeIndex, context.physicalDevice, VK_QUEUE_COMPUTE_BIT);
    uint32_t ret3 =
        getPresentQueueFamilyIndex(&presentIndex, context.physicalDevice, context.surface);
   
    if (ret1 != VK_SUCCESS || ret2 != VK_SUCCESS || ret3 != VK_SUCCESS) {
      LOG_ERROR(ERR_LEVEL_FATAL, "unable to acquire indices\n");
      PANIC();
    }
  };

   //VkExtent2D swapchainExtent;
  getExtentWindow(&context.swapchainExtent, context.pWindow);

  /* we want to use swapchains to reduce tearing */
  const uint32_t deviceExtensionCount = 1;
  const char *ppDeviceExtensionNames[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  /*create device */
  //VkDevice device;
  new_Device(&context.device, context.physicalDevice, graphicsIndex, deviceExtensionCount,
             ppDeviceExtensionNames);

 // VkQueue graphicsQueue;
  getQueue(&context.graphicsQueue, context.device, graphicsIndex);
  VkQueue computeQueue;
  getQueue(&computeQueue, context.device, computeIndex);
  VkQueue presentQueue;
  getQueue(&presentQueue, context.device, presentIndex);

  /* We can create command buffers from the command pool */
 // VkCommandPool commandPool;
  new_CommandPool(&context.commandPool, context.device, graphicsIndex);

  /* get preferred format of screen*/
 // VkSurfaceFormatKHR surfaceFormat;
  getPreferredSurfaceFormat(&context.surfaceFormat, context.physicalDevice, context.surface);
//VkSwapchainKHR swapchain;
  uint32_t swapchainImageCount;
  new_Swapchain(&context.swapchain, &swapchainImageCount, VK_NULL_HANDLE, context.surfaceFormat,
                context.physicalDevice, context.device, context.surface, context.swapchainExtent, graphicsIndex,
                presentIndex);

  // there are swapchainImageCount swapchainImages
  VkImage *pSwapchainImages = (VkImage *)malloc(swapchainImageCount * sizeof(VkImage));
  getSwapchainImages(pSwapchainImages, swapchainImageCount, context.device, context.swapchain);

  // there are swapchainImageCount swapchainImageViews
  VkImageView *pSwapchainImageViews =
      (VkImageView *)malloc(swapchainImageCount * sizeof(VkImageView));
  new_SwapchainImageViews(pSwapchainImageViews, pSwapchainImages,
                          swapchainImageCount, context.device, context.surfaceFormat.format);

  /* Create depth buffer */
  //VkDeviceMemory depthImageMemory;
  //VkImage depthImage;
  new_DepthImage(&context.depthImage, &context.depthImageMemory, context.swapchainExtent,
                 context.physicalDevice, context.device);
  VkImageView depthImageView;
  new_DepthImageView(&depthImageView, context.device, context.depthImage);

VkShaderModule fragShaderModule;
  {
    uint32_t *fragShaderFileContents;
    uint32_t fragShaderFileLength;
    readShaderFile("/home/petermiller/Desktop/vulkan-triangle-v1-master/assets/shaders/shader.frag.spv", &fragShaderFileLength,
                   &fragShaderFileContents);
    new_ShaderModule(&fragShaderModule, context.device, fragShaderFileLength,
                     fragShaderFileContents);
    free(fragShaderFileContents);
  }

  VkShaderModule vertShaderModule;
  {
    uint32_t *vertShaderFileContents;
    uint32_t vertShaderFileLength;
    readShaderFile("/home/petermiller/Desktop/vulkan-triangle-v1-master/assets/shaders/shader.vert.spv", &vertShaderFileLength,
                   &vertShaderFileContents);
    new_ShaderModule(&vertShaderModule, context.device, vertShaderFileLength,
                     vertShaderFileContents);
    free(vertShaderFileContents);
  }

  /* Create graphics pipeline */
  //VkRenderPass renderPass;
  new_VertexDisplayRenderPass(&context.renderPass, context.device, context.surfaceFormat.format);

 // VkPipelineLayout graphicsPipelineLayout;
  new_VertexDisplayPipelineLayout(&context.graphicsPipelineLayout, context.device);

 // VkPipeline graphicsPipeline;
  new_VertexDisplayPipeline(&context.graphicsPipeline, context.device, vertShaderModule,
                            fragShaderModule, context.swapchainExtent, context.renderPass,
                            context.graphicsPipelineLayout);

  VkFramebuffer *pSwapchainFramebuffers =
     (VkFramebuffer *)malloc(swapchainImageCount * sizeof(VkFramebuffer));
  new_SwapchainFramebuffers(pSwapchainFramebuffers, context.device, context.renderPass,
                            context.swapchainExtent, swapchainImageCount,
                            depthImageView, pSwapchainImageViews);

  //VkBuffer vertexBuffer;
  //VkDeviceMemory vertexBufferMemory;
  new_VertexBuffer(&context.vertexBuffer, &context.vertexBufferMemory, vertexData, vertexCount,
                   context.device, context.physicalDevice, context.commandPool, context.graphicsQueue);

  //VkCommandBuffer pVertexDisplayCommandBuffers[MAX_FRAMES_IN_FLIGHT];
  new_CommandBuffers(context.pVertexDisplayCommandBuffers, MAX_FRAMES_IN_FLIGHT, context.commandPool, context.device);
//VkSemaphore pImageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
  new_Semaphores(context.pImageAvailableSemaphores, MAX_FRAMES_IN_FLIGHT, context.device);
 // VkSemaphore pRenderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
  new_Semaphores(context.pRenderFinishedSemaphores, MAX_FRAMES_IN_FLIGHT, context.device);
 // VkFence pInFlightFences[MAX_FRAMES_IN_FLIGHT];
  new_Fences(context.pInFlightFences, MAX_FRAMES_IN_FLIGHT, context.device,
             true); // fences start off signaled

  // create camera
  vec3 loc = {0.0f, 0.0f, 0.0f};
  Camera camera = new_Camera(loc, context.swapchainExtent);

  // this number counts which frame we're on
  // up to MAX_FRAMES_IN_FLIGHT, at whcich points it resets to 0
  uint32_t currentFrame = 0;

  /*wait till close*/
  while (!glfwWindowShouldClose(context.pWindow)) {
    glfwPollEvents();

    // wait for last frame to finish
    waitAndResetFence(context.pInFlightFences[currentFrame], context.device);

    // the imageIndex is the index of the swapchain framebuffer that is
    // available next
    uint32_t imageIndex;
    // this function will return immediately,
    //  so we use the semaphore to tell us when the image is actually available,
    //  (ready for rendering to)
    ErrVal result =
        getNextSwapchainImage(&imageIndex, context.swapchain, context.device,
                              context.pImageAvailableSemaphores[currentFrame]);

    // if the window is resized
    /*if (result == ERR_OUTOFDATE) {
      vkDeviceWaitIdle(context.device);

      delete_SwapchainFramebuffers(pSwapchainFramebuffers, swapchainImageCount,
                                   context.device);
      free(pSwapchainFramebuffers);
      delete_Pipeline(&context.graphicsPipeline, device);
      delete_PipelineLayout(&graphicsPipelineLayout, device);
      delete_RenderPass(&renderPass, device);
      delete_SwapchainImageViews(pSwapchainImageViews, swapchainImageCount,
                                 device);
      free(pSwapchainImageViews);
      free(pSwapchainImages);
      delete_Swapchain(&swapchain, device);

      // delete depth buffer
      delete_ImageView(&depthImageView, device);
      delete_Image(&depthImage, device);
      delete_DeviceMemory(&depthImageMemory, device);

      // get new window size
      getExtentWindow(&swapchainExtent, pWindow);
      resizeCamera(&camera, swapchainExtent);

     
      new_Swapchain(&swapchain, &swapchainImageCount, swapchain, surfaceFormat,
                    physicalDevice, device, surface, swapchainExtent,
                    graphicsIndex, presentIndex);

      pSwapchainImages = malloc(swapchainImageCount * sizeof(VkImage));
      getSwapchainImages(pSwapchainImages, swapchainImageCount, device,
                         swapchain);

      pSwapchainImageViews = malloc(swapchainImageCount * sizeof(VkImageView));
      new_SwapchainImageViews(pSwapchainImageViews, pSwapchainImages,
                              swapchainImageCount, device,
                              surfaceFormat.format);

      // Create depth image
      new_DepthImage(&depthImage, &depthImageMemory, swapchainExtent,
                     physicalDevice, device);
      new_DepthImageView(&depthImageView, device, depthImage);

      
      new_VertexDisplayRenderPass(&renderPass, device, surfaceFormat.format);
      new_VertexDisplayPipelineLayout(&graphicsPipelineLayout, device);
      new_VertexDisplayPipeline(&graphicsPipeline, device, vertShaderModule,
                                fragShaderModule, swapchainExtent, renderPass,
                                graphicsPipelineLayout);
      pSwapchainFramebuffers =
          malloc(swapchainImageCount * sizeof(VkFramebuffer));
      new_SwapchainFramebuffers(pSwapchainFramebuffers, device, renderPass,
                                swapchainExtent, swapchainImageCount,
                                depthImageView, pSwapchainImageViews);

      // finally we can retry getting the swapchain
      getNextSwapchainImage(&imageIndex, swapchain, device,
                            pImageAvailableSemaphores[currentFrame]);
    }*/

    // update camera
   updateCamera(&camera, context.pWindow);
    mat4x4 mvp;
    getMvpCamera(mvp, &camera);

    // record buffer
    recordVertexDisplayCommandBuffer(                //
        context.pVertexDisplayCommandBuffers[currentFrame],  //
        pSwapchainFramebuffers[imageIndex],          //
        context.vertexBuffer,                                //
        vertexCount,                                 //
        context.renderPass,                                  //
        context.graphicsPipelineLayout,                      //
        context.graphicsPipeline,                            //
        context.swapchainExtent,                             //
        mvp,                                         //
        (VkClearColorValue){.float32 = {0, 0, 0, 0}} //
    );

    drawFrame(                                      //
        context.pVertexDisplayCommandBuffers[currentFrame], //
        context.swapchain,                                  //
        imageIndex,                                 //
        context.pImageAvailableSemaphores[currentFrame],    //
        context.pRenderFinishedSemaphores[currentFrame],    //
        context.pInFlightFences[currentFrame],              //
        context.graphicsQueue,                              //
        presentQueue                                //
    );

    // increment frame
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
  }

  /*cleanup*/
  /*vkDeviceWaitIdle(device);
  delete_ShaderModule(&fragShaderModule, device);
  delete_ShaderModule(&vertShaderModule, device);

  delete_Fences(pInFlightFences, MAX_FRAMES_IN_FLIGHT, device);
  delete_Semaphores(pRenderFinishedSemaphores, MAX_FRAMES_IN_FLIGHT, device);
  delete_Semaphores(pImageAvailableSemaphores, MAX_FRAMES_IN_FLIGHT, device);

  delete_CommandBuffers(pVertexDisplayCommandBuffers, MAX_FRAMES_IN_FLIGHT,
                        commandPool, device);
  delete_CommandPool(&commandPool, device);

  delete_SwapchainFramebuffers(pSwapchainFramebuffers, swapchainImageCount,
                               device);
  free(pSwapchainFramebuffers);
  delete_Pipeline(&graphicsPipeline, device);
  delete_PipelineLayout(&graphicsPipelineLayout, device);
  delete_Buffer(&vertexBuffer, device);
  delete_DeviceMemory(&vertexBufferMemory, device);
  delete_RenderPass(&renderPass, device);
  delete_SwapchainImageViews(pSwapchainImageViews, swapchainImageCount, device);
  free(pSwapchainImageViews);
  free(pSwapchainImages);
  delete_Swapchain(&swapchain, device);
  delete_ImageView(&depthImageView, device);
  delete_Image(&depthImage, device);
  delete_DeviceMemory(&depthImageMemory, device);
  delete_Device(&device);
  delete_Surface(&surface, instance);
  delete_DebugCallback(&callback, instance);
  delete_Instance(&instance);
*/
  glfwTerminate();
  return (EXIT_SUCCESS);
printf("hello");
};
