# Tutorial 02 - use validation layers - 1

Demonstrate how to use validation layers prebuilt from [Validation layer repo][1]

## Pre-requirement:
---------------
1. Download prebuilt validation layer android-binaries-*.zip from [Khronos repo][1]
1. unzip to $this-project-root/vvl-libs

## Added steps from previous example
1. Use vulkan wrappers in common/vulkan_wrapper directory.
1. Add the validation layer into gradle build to pack it into apk.
1. (Optional but nice to have)Confirm the validation layer "VK_LAYER_KHRONOS_validation" is available with `vkEnumerateInstanceLayerProperties()`. 
1. Enable the validation layer and "VK_EXT_debug_report" when creating instance.
1. Register debug callback implemented in "VK_EXT_debug_report" with vkCreateDebugReportCallbackEXT(). Note that application could also use "VK_EXT_debug_utils" implemented in the validatio layer.

# Verification
Planted error: this sample sets [`VkDeviceQueueCreateInfo::pQueuePriorities`][2](https://github.com/googlesamples/android-vulkan-tutorials/blob/master/tutorial02_prebuild_layers/app/src/main/jni/main.cpp#L190) to nullptr,
which will trigger validation layers to notify us from registered callback function
`vkDebugReportCallbackEX_impl();` putting a breakpoint with Android Studio, observe
the breakpoint being triggered.

In logcat, you would see the error message like:
```
E/Vulkan-Debug-Message:: Validation -- Validation Error: [ VUID-VkDeviceQueueCreateInfo-pQueuePriorities-parameter ] Object 0: VK_NULL_HANDLE, type = VK_OBJECT_TYPE_DEVICE; | MessageID = 0xd6d720c6 | vkCreateDevice: required parameter pCreateInfo->pQueueCreateInfos[0].pQueuePriorities specified as NULL. The Vulkan spec states: pQueuePriorities must be a valid pointer to an array of queueCount float values (https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#VUID-VkDeviceQueueCreateInfo-pQueuePriorities-parameter)
```
## Additional References
- [Validation layer history][3]
[1]:https://github.com/KhronosGroup/Vulkan-ValidationLayers/releases/latest
[2]:https://github.com/googlesamples/android-vulkan-tutorials/blob/master/tutorial02_prebuild_layers/app/src/main/jni/main.cpp#L190
[3]:https://www.lunarg.com/wp-content/uploads/2019/04/UberLayer_V3.pdf

