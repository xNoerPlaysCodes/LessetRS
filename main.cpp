/*

    This program is based off of the GLFW-Vulkan example for ImGui.
    Look at the include paths below for some guidance on how you should structure things.
    First bit of my code is at line 58
*/
#include "../imgui.h"
#include "../imgui/backends/imgui_impl_vulkan.h"
#include "../imgui/backends/imgui_impl_glfw.h"
#include "../imgui/misc/cpp/imgui_stdlib.h"
#include "../implot/implot.h"
#include "../implot/implot_internal.h"
#include "NotoSansMathRegular.hpp"
#include "imgui_styles.h"
#include "lesset.hpp"
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <stdio.h>          
#include <stdlib.h>         
#include <unordered_map>
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <fstream>
#include <nfd.hpp>

// Volk headers
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
#define VOLK_IMPLEMENTATION
#include <volk.h>
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// #define APP_USE_UNLIMITED_FRAME_RATE
#ifdef _DEBUG
#define APP_USE_VULKAN_DEBUG_REPORT
static VkDebugReportCallbackEXT g_DebugReport = VK_NULL_HANDLE;
#endif

// Data
static VkAllocationCallbacks*   g_Allocator = nullptr;
static VkInstance               g_Instance = VK_NULL_HANDLE;
static VkPhysicalDevice         g_PhysicalDevice = VK_NULL_HANDLE;
static VkDevice                 g_Device = VK_NULL_HANDLE;
static uint32_t                 g_QueueFamily = (uint32_t)-1;
static VkQueue                  g_Queue = VK_NULL_HANDLE;
static VkPipelineCache          g_PipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool         g_DescriptorPool = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static uint32_t                 g_MinImageCount = 2;
static bool                     g_SwapChainRebuild = false;

struct sliderData
{
    std::array<float,3> nums{0,0,0};
    bool animate{};
};

struct Instance
{
    std::string name;
    std::unordered_map<std::string,std::string> variables{};
    std::unordered_map<std::string, lessetB::Function> functions;
    std::unordered_map<std::string, char> protectedFunctionDefinitions; // Protects some functions from being undef'd
    std::unordered_map<std::string,sliderData> sliders;
    std::string lastScriptOutput;
};



#define TRIMMEDDECIMALPLACES 3       // Decimal places trimmed from coordinates display for special points
#define HIGH_PRECISION_DRAW_DELAY 50 // Delay in frames between no movement and recalculating of graphs at full precision
#define MANY_GRAPHS 100              // When the checkbox allowing the disabling of all graphs is added
#define MAX_COMMANDS 100000          // Hard cap for scripts
#define MAX_GRAPH_POINTS_BASE 8000   // Medium graphing precision, other precisions are based off of this one

// I can't describe these very well. They're used in finding discontinuities.
#define MAX_CHANGE_FACTOR_FIRST 6 
#define MAX_CHANGE_FACTOR_SECOND 3 





bool isNoisy(const std::vector<double> &pointsX, const std::vector<double> &pointsY, size_t i, int maxIndividualGraphPointsMultiplier);
bool addIdentifier(Instance &data,const std::pair<std::string,std::string> &newVariable);
int addClosingParentheses(std::string &equation);
bool IsPlotHidden();

// -> Line 434




static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}
static void check_vk_result(VkResult err)
{
    if (err == VK_SUCCESS)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

#ifdef APP_USE_VULKAN_DEBUG_REPORT
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
    (void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix; // Unused arguments
    fprintf(stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n", objectType, pMessage);
    return VK_FALSE;
}
#endif // APP_USE_VULKAN_DEBUG_REPORT

static bool IsExtensionAvailable(const ImVector<VkExtensionProperties>& properties, const char* extension)
{
    for (const VkExtensionProperties& p : properties)
        if (strcmp(p.extensionName, extension) == 0)
            return true;
    return false;
}

static void SetupVulkan(ImVector<const char*> instance_extensions)
{
    VkResult err;
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
    volkInitialize();
#endif

    // Create Vulkan Instance
    {
        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

        // Enumerate available extensions
        uint32_t properties_count;
        ImVector<VkExtensionProperties> properties;
        vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
        properties.resize(properties_count);
        err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.Data);
        check_vk_result(err);

        // Enable required extensions
        if (IsExtensionAvailable(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
            instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
        {
            instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
#endif

        // Enabling validation layers
#ifdef APP_USE_VULKAN_DEBUG_REPORT
        const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = layers;
        instance_extensions.push_back("VK_EXT_debug_report");
#endif

        // Create Vulkan Instance
        create_info.enabledExtensionCount = (uint32_t)instance_extensions.Size;
        create_info.ppEnabledExtensionNames = instance_extensions.Data;
        err = vkCreateInstance(&create_info, g_Allocator, &g_Instance);
        check_vk_result(err);
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
        volkLoadInstance(g_Instance);
#endif

        // Setup the debug report callback
#ifdef APP_USE_VULKAN_DEBUG_REPORT
        auto f_vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(g_Instance, "vkCreateDebugReportCallbackEXT");
        IM_ASSERT(f_vkCreateDebugReportCallbackEXT != nullptr);
        VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
        debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
        debug_report_ci.pfnCallback = debug_report;
        debug_report_ci.pUserData = nullptr;
        err = f_vkCreateDebugReportCallbackEXT(g_Instance, &debug_report_ci, g_Allocator, &g_DebugReport);
        check_vk_result(err);
#endif
    }

    // Select Physical Device (GPU)
    g_PhysicalDevice = ImGui_ImplVulkanH_SelectPhysicalDevice(g_Instance);
    IM_ASSERT(g_PhysicalDevice != VK_NULL_HANDLE);

    // Select graphics queue family
    g_QueueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(g_PhysicalDevice);
    IM_ASSERT(g_QueueFamily != (uint32_t)-1);

    // Create Logical Device (with 1 queue)
    {
        ImVector<const char*> device_extensions;
        device_extensions.push_back("VK_KHR_swapchain");

        // Enumerate physical device extension
        uint32_t properties_count;
        ImVector<VkExtensionProperties> properties;
        vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &properties_count, nullptr);
        properties.resize(properties_count);
        vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &properties_count, properties.Data);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
        if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
            device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

        const float queue_priority[] = { 1.0f };
        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = g_QueueFamily;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;
        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
        create_info.pQueueCreateInfos = queue_info;
        create_info.enabledExtensionCount = (uint32_t)device_extensions.Size;
        create_info.ppEnabledExtensionNames = device_extensions.Data;
        err = vkCreateDevice(g_PhysicalDevice, &create_info, g_Allocator, &g_Device);
        check_vk_result(err);
        vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);
    }

    // Create Descriptor Pool
    // If you wish to load e.g. additional textures you may need to falter pools sizes and maxSets.
    {
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE },
            { VK_DESCRIPTOR_TYPE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE },
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 0;
        for (VkDescriptorPoolSize& pool_size : pool_sizes)
            pool_info.maxSets += pool_size.descriptorCount;
        pool_info.poolSizeCount = (uint32_t)IM_COUNTOF(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(g_Device, &pool_info, g_Allocator, &g_DescriptorPool);
        check_vk_result(err);
    }
}

// All the ImGui_ImplVulkanH_XXX structures/functions are optional helpers used by the demo.
// Your real engine/app may not use them.
static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height)
{
    // Check for WSI support
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, g_QueueFamily, surface, &res);
    if (res != VK_TRUE)
    {
        fprintf(stderr, "Error no WSI support on physical device 0\n");
        exit(-1);
    }

    // Select Surface Format
    const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->Surface = surface;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(g_PhysicalDevice, wd->Surface, requestSurfaceImageFormat, (size_t)IM_COUNTOF(requestSurfaceImageFormat), requestSurfaceColorSpace);

    // Select Present Mode
#ifdef APP_USE_UNLIMITED_FRAME_RATE
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
#else
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(g_PhysicalDevice, wd->Surface, &present_modes[0], IM_COUNTOF(present_modes));
    //printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

    // Create SwapChain, RenderPass, Framebuffer, etc.
    IM_ASSERT(g_MinImageCount >= 2);
    ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, wd, g_QueueFamily, g_Allocator, width, height, g_MinImageCount, 0);
}

static void CleanupVulkan()
{
    vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);

#ifdef APP_USE_VULKAN_DEBUG_REPORT
    // Remove the debug report callback
    auto f_vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(g_Instance, "vkDestroyDebugReportCallbackEXT");
    f_vkDestroyDebugReportCallbackEXT(g_Instance, g_DebugReport, g_Allocator);
#endif // APP_USE_VULKAN_DEBUG_REPORT

    vkDestroyDevice(g_Device, g_Allocator);
    vkDestroyInstance(g_Instance, g_Allocator);
}

static void CleanupVulkanWindow(ImGui_ImplVulkanH_Window* wd)
{
    ImGui_ImplVulkanH_DestroyWindow(g_Instance, g_Device, wd, g_Allocator);
    vkDestroySurfaceKHR(g_Instance, wd->Surface, g_Allocator);
}

static void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data)
{
    VkSemaphore image_acquired_semaphore  = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkResult err = vkAcquireNextImageKHR(g_Device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        g_SwapChainRebuild = true;
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
        return;
    if (err != VK_SUBOPTIMAL_KHR)
        check_vk_result(err);

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
    {
        err = vkWaitForFences(g_Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);    // wait indefinitely instead of periodically checking
        check_vk_result(err);

        err = vkResetFences(g_Device, 1, &fd->Fence);
        check_vk_result(err);
    }
    {
        err = vkResetCommandPool(g_Device, fd->CommandPool, 0);
        check_vk_result(err);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
        check_vk_result(err);
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = wd->RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = wd->Width;
        info.renderArea.extent.height = wd->Height;
        info.clearValueCount = 1;
        info.pClearValues = &wd->ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    // Record dear imgui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

    // Submit command buffer
    vkCmdEndRenderPass(fd->CommandBuffer);
    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &image_acquired_semaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &fd->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &render_complete_semaphore;

        err = vkEndCommandBuffer(fd->CommandBuffer);
        check_vk_result(err);
        err = vkQueueSubmit(g_Queue, 1, &info, fd->Fence);
        check_vk_result(err);
    }
}

static void FramePresent(ImGui_ImplVulkanH_Window* wd)
{
    if (g_SwapChainRebuild)
        return;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &wd->Swapchain;
    info.pImageIndices = &wd->FrameIndex;
    VkResult err = vkQueuePresentKHR(g_Queue, &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        g_SwapChainRebuild = true;
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
        return;
    if (err != VK_SUBOPTIMAL_KHR)
        check_vk_result(err);
    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount; // Now we can use the next set of semaphores
}

// Main code
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Create window with Vulkan context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
    GLFWwindow* window = glfwCreateWindow((int)(1280 * main_scale), (int)(800 * main_scale), "Lesset", nullptr, nullptr);
    if (!glfwVulkanSupported())
    {
        printf("GLFW: Vulkan Not Supported\n");
        return 1;
    }

    ImVector<const char*> extensions;
    uint32_t extensions_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
    for (uint32_t i = 0; i < extensions_count; i++)
        extensions.push_back(glfw_extensions[i]);
    SetupVulkan(extensions);

    // Create Window Surface
    VkSurfaceKHR surface;
    VkResult err = glfwCreateWindowSurface(g_Instance, window, g_Allocator, &surface);
    check_vk_result(err);

    // Create Framebuffers
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
    SetupVulkanWindow(wd, surface, w, h);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    style.Alpha=0.9f;
    style.FrameRounding=3.f;
    style.PopupRounding=3.f;
    style.WindowRounding=3.f;
    style.GrabRounding=3.f;

    // -> Line 477


    // Setup scaling
    // ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)
    io.ConfigDpiScaleFonts = true;          // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
    io.ConfigDpiScaleViewports = true;      // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    //init_info.ApiVersion = VK_API_VERSION_1_3;              // Pass in your value of VkApplicationInfo::apiVersion, otherwise will default to header version.
    init_info.Instance = g_Instance;
    init_info.PhysicalDevice = g_PhysicalDevice;
    init_info.Device = g_Device;
    init_info.QueueFamily = g_QueueFamily;
    init_info.Queue = g_Queue;
    init_info.PipelineCache = g_PipelineCache;
    init_info.DescriptorPool = g_DescriptorPool;
    init_info.MinImageCount = g_MinImageCount;
    init_info.ImageCount = wd->ImageCount;
    init_info.Allocator = g_Allocator;
    init_info.PipelineInfoMain.RenderPass = wd->RenderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Our state ☭

    NFD::Init();

    ImVec4 clear_color = ImVec4(0.f, 0.f, 0.f, 0.f);
    io.Fonts->AddFontFromMemoryCompressedTTF(NotoSansMathRegular_compressed_data,NotoSansMathRegular_compressed_size);

    ImPlotSpec specHidden;
    specHidden.FillAlpha=0;
    specHidden.LineColor=ImVec4(0,0,0,0);
    specHidden.Flags=ImPlotColormapScaleFlags_NoLabel;

    // Fix an issue with auto fitting
    const float defaultBoundsX[]{-10,10};
    const float defaultBoundsY[]{-10,10};

    // Display some text upon viewing a table calculation for the first time to use the graph menu for graphing, 
    // suspecting the user might've wanted to graph by typing into the regular calculation bar.
    bool showGraphingMenuHint{true}; 
    bool hasShownHint{};
    bool showFunctionsMenu{};
    bool showTrivia{};

    // For assignments
    std::string newIdentifierNameVariable;
    std::string newIdentifierValueVariable;
    std::string assignVariableReport;

    // Gets cleared every frame
    std::string nothing;

    bool resultWindow{true};

    // Equation strings
    std::string lastNonEmptyEquation;
    std::vector<std::pair<std::string,std::pair<std::string,lessetB::Function>>> graphsEquations;
    std::string graphEquation;
    std::string nonEmptyGraphEquation;
    std::string equation;
    std::string nonEmptyEquation;
    std::string editingGraphEquation;
    std::string result;

    std::string resultHistory;

    // Graphing
    int maxIndividualGraphPointsMultiplier{2};
    float pastPrecisionDivisors[100]{};
    size_t timeStationary{};
    size_t recalculateGraphIndex{static_cast<size_t>(-1)};
    int maxIndividualGraphPoints {8000}; // Maximum number of points per graph calculated for one screen. (There may be more because of memoization when zooming out) 

    std::pair<std::vector<std::vector<double>>,std::vector<std::vector<double>>> graphsPoints{}; // Big boy
    
    // Settings
    lessetB::Options options{false,-5,5,boost::multiprecision::cpp_complex<lessetB::maxPrecision>(1,0),2,true};
    bool graph{};
    bool markSpecialPoints{true};
    bool gayMode{};
    bool showGayMode{};
    float gayModeSpeed{0.001};
    
    bool prioritizeImplicitMultiplication{true};
    bool showFractions{true};
    bool interpolateDiscontinuities{};

    std::string sliderMin{'0'};
    std::string sliderMax{"10"};
    float sliderMinFloat{};
    float sliderMaxFloat{10};

    std::string aroundTruthinessLeniency="0.01";
    boost::multiprecision::cpp_complex<lessetB::maxPrecision> aroundTruthinessLeniencyFloat{0.01};

    std::string xMin{"-5"};
    std::string xMax{'5'};
    std::string xStep={'1'};
    boost::multiprecision::cpp_complex<lessetB::maxPrecision> xMinFloat{-5};
    boost::multiprecision::cpp_complex<lessetB::maxPrecision> xMaxFloat{5};
    boost::multiprecision::cpp_complex<lessetB::maxPrecision> xStepFloat{0.1};

    bool drawMany_Graphs{true}; // cake_Case
    bool showFps{};

    std::vector<Instance> instances{};
    instances.emplace_back("main");
    
    bool recalculateGraphs{};

    // Scripting
    size_t selectedInstance{};
    std::string newInstanceName;
    std::string filePath;

    ImPlotRect limits{};
    float hueModifier{0.586};
    float saturationModifier{0.7};

    bool hasRunScriptInMainInstance{};
    
    size_t previousGraphsEquationsSize{};
    int tooltipRnd{};
    int lastCalculationPrecision{};
    
    
{
    ImGuiStyle initialStyle = style;
    ImGui::LoadStyleFrom("LessetStyle.ini");
    for(size_t i{}; i<ImGuiCol_COUNT; i++)
    {
        if(initialStyle.Colors[i].x!=style.Colors[i].x ||
           initialStyle.Colors[i].y!=style.Colors[i].y ||
           initialStyle.Colors[i].z!=style.Colors[i].z ||
           initialStyle.Colors[i].w!=style.Colors[i].w)
        {
            hueModifier=0.5;
            saturationModifier=0.5;
        }
    }
}
    // -> Line 614

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Resize swap chain?
        int fb_width, fb_height;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0 && (g_SwapChainRebuild || g_MainWindowData.Width != fb_width || g_MainWindowData.Height != fb_height))
        {
            ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
            ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, wd, g_QueueFamily, g_Allocator, fb_width, fb_height, g_MinImageCount, 0);
            g_MainWindowData.FrameIndex = 0;
            g_SwapChainRebuild = false;
        }
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        {
            lessetB::globals::debugCoutUsed=false;
            nothing.clear();
            recalculateGraphIndex=static_cast<size_t>(-1);
            auto io = ImGui::GetIO();
            ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::SetNextWindowPos(ImVec2(0,0));
            style.WindowRounding=0.f;

            ImGui::Begin("Lesset says hello!",__null,ImGuiWindowFlags_MenuBar|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBackground);                          // Create a window called "Hello, world!" and append into it.
            style.WindowRounding=5.f;
            
            if (ImGui::BeginMenuBar())
            {
                // Scripting menu (These comments are to ctrl f faster lol)
                if(ImGui::BeginMenu("Scripting"))
                {
                    //ImGui::MenuItem("Everything about automation.",NULL,false,false);
                    if(ImGui::BeginMenu("Instances"))
                    {
                        if(ImGui::BeginMenu("Add"))
                        {
                            ImGui::SetNextItemWidth(150);
                            ImGui::InputText("Name",&newInstanceName);

                            if(ImGui::Button("Add Instance"))
                            {
                                bool nameOkay{};
                                if(!newInstanceName.empty()) nameOkay=true;
                                for(size_t i{}; i<instances.size(); i++)
                                {
                                    if(newInstanceName==instances.at(i).name) nameOkay=false;
                                }
                                if(nameOkay) instances.emplace_back(newInstanceName);
                            }  
                            ImGui::EndMenu(); 
                        }     
                        
                        if(instances.size()>1)if(ImGui::BeginMenu("Select"))
                        {
                            for(size_t i{0}; i<instances.size(); i++)
                            {
                                if(ImGui::Button(instances.at(i).name.c_str()))
                                {
                                    selectedInstance=i;
                                    recalculateGraphs=true;
                                }
                            }    
                            ImGui::EndMenu();
                        }

                        if(instances.size()>1) if(ImGui::BeginMenu("Remove"))
                        {
                            ImGui::MenuItem("Click an instance to remove it.",NULL,false,false);
                            for(size_t i{1}; i<instances.size(); i++)
                            {
                                if(ImGui::Button(instances.at(i).name.c_str()))
                                {
                                    if(selectedInstance==i) selectedInstance=0;
                                    instances.erase(instances.begin()+i);
                                }
                            }  
                            ImGui::EndMenu();
                        }

                        ImGui::EndMenu();
                    }
                    ImGui::SetItemTooltip("Encapsulate calculator data.");
                    // Run Menu (Run Script Menu)
                    if (ImGui::BeginMenu("Run"))
                    {
                        if(selectedInstance==0) ImGui::Text("You are using the main instance.");
                        if(selectedInstance==0 && hasRunScriptInMainInstance)
                        {
                            if(ImGui::Button("Clear main instance's data"))
                            {
                                instances.at(0)=Instance();
                                hasRunScriptInMainInstance=false;
                            }
                        }
                        ImGui::InputText("File path to script",&filePath);

                        if(ImGui::Button("Run"))
                        {
                            size_t iterations{};
                            std::ifstream calculationsFile;
                            calculationsFile.open(filePath);
                            if(!calculationsFile.fail())
                            {
                                if(instances.at(selectedInstance).lastScriptOutput!="")
                                {
                                    instances.at(selectedInstance).lastScriptOutput="";
                                }
                                if(selectedInstance==0) hasRunScriptInMainInstance=true;
                                std::string scriptEquation;
                                bool skip{};
                                bool conditionTrue{};
                                
                                while(std::getline(calculationsFile,scriptEquation) && iterations<=MAX_COMMANDS)
                                {
                                    std::string conditionValue;
                                    std::string jumpValue;
                                    iterations++;
                                    for(size_t i{}; i<scriptEquation.length() && scriptEquation.at(i)!='#'; i++)
                                    {
                                        if(std::isspace(scriptEquation.at(i))) scriptEquation.erase(i--,1);
                                    }

                                    if(scriptEquation.find("let")==0 && scriptEquation.find('=')!=std::string::npos)
                                    {
                                        std::string name = scriptEquation.substr(3,scriptEquation.find('=')-3);
                                        if(name.length()>MAX_KEYWORD_LENGTH)
                                        {
                                            instances.at(selectedInstance).lastScriptOutput+="Names cannot be longer than 15 characters.";
                                            continue;
                                        }
                                        std::string eq = scriptEquation.substr(scriptEquation.find('=')+1);
                                        if(lessetB::addVariable(std::pair<std::string, std::string>(name,eq), instances.at(selectedInstance).variables))
                                        {
                                            instances.at(selectedInstance).lastScriptOutput+="Failed assignment " + scriptEquation+'\n';
                                        }
                                        else instances.at(selectedInstance).lastScriptOutput+="Assigned " + name + " value " + instances.at(selectedInstance).variables.at(name)+'\n';
                                        continue;
                                    }
                                    if(scriptEquation.find("endIF")==0)
                                    {
                                        skip=false;
                                        instances.at(selectedInstance).lastScriptOutput+="endIF\n";
                                        continue;
                                    }

                                    if(scriptEquation.find("graph")==0 &&(!skip || conditionTrue==true))
                                    {
                                        for(std::pair<std::string,std::string> var : instances.at(selectedInstance).variables)
                                        {
                                            for(size_t i{}; i<scriptEquation.length(); i++)
                                            {
                                                if(scriptEquation.find("numof",i)==i)
                                                {
                                                    if(scriptEquation.find(var.first)<=i+6)
                                                    {
                                                        scriptEquation.replace(i,5+var.first.length(),var.second);
                                                    }
                                                }
                                            }
                                        }

                                        if(0==graphsEquations.size()) 
                                        {
                                            instances.at(selectedInstance).lastScriptOutput+="Graphing "+scriptEquation.substr(5)+'\n';
                                            graphsEquations.push_back({scriptEquation.substr(5),std::pair<std::string,lessetB::Function>()});
                                            recalculateGraphs=true;
                                        }
                                        else for(size_t i{}; i<graphsEquations.size(); i++)
                                        {
                                            if(graphsEquations.at(i).first==scriptEquation.substr(5)) break;
                                            else if(i>=graphsEquations.size()-1) 
                                            {
                                                instances.at(selectedInstance).lastScriptOutput+="Graphing "+scriptEquation.substr(5)+'\n';
                                                graphsEquations.push_back({scriptEquation.substr(5),std::pair<std::string,lessetB::Function>()});
                                                recalculateGraphs=true;
                                            }
                                        }
                                        continue;
                                    }

                                    if(scriptEquation.find("fn")==0)
                                    {
                                        std::string fnSig = scriptEquation.substr(2,scriptEquation.find('=')-2);
                                        bool hasLparen{};
                                        bool hasRparen{};
                                        bool failed{};
                                        for(char c : fnSig)
                                        {
                                            if(fnSig.find('=')!=std::string::npos)
                                            {
                                                instances.at(selectedInstance).lastScriptOutput+="Function definition failed, bad signature.\n";
                                                failed=true;
                                            }
                                            if(c=='(')
                                            {
                                                if(hasLparen) 
                                                {
                                                    instances.at(selectedInstance).lastScriptOutput+="Function definition failed, bad signature.\n";
                                                    failed=true;
                                                }
                                                hasLparen=true;
                                            } 
                                            if(c==')')
                                            {
                                                if(hasRparen)
                                                {
                                                    instances.at(selectedInstance).lastScriptOutput+="Function definition failed, bad signature.\n";
                                                    failed=true;
                                                }
                                                hasRparen=true;
                                            } 
                                        }
                                        std::string fnName = fnSig.substr(0,fnSig.find('('));
                                        if(fnName.length()>MAX_KEYWORD_LENGTH)
                                        {
                                            instances.at(selectedInstance).lastScriptOutput+="Function definition failed, names cannot be longer than 15 characters.\n";
                                            continue;
                                        }
                                        
                                        for(size_t i{}; i<fnName.length(); i++)
                                        {
                                            char c = fnName.at(i);
                                            if(!lessetB::isNameValid(fnName) || i>=MAX_KEYWORD_LENGTH)
                                            {
                                                failed=true;
                                                instances.at(selectedInstance).lastScriptOutput+="Function definition failed, name contained invalid characters\n";
                                                break;
                                            }
                                        }
                                        

                                        std::string definition = scriptEquation.substr(scriptEquation.find('=')+1);
                                        if(definition=="")
                                        {
                                            instances.at(selectedInstance).lastScriptOutput+="Function definition failed, empty definition\n";
                                            failed=true;
                                        }
                                        lessetB::Function fn = lessetB::getFnFromSignature(scriptEquation.substr(2));
                                        if(fn.argNames.size()==0)
                                        {
                                            instances.at(selectedInstance).lastScriptOutput+="Function definition failed\n";   
                                            failed=true;
                                        }
                                        if(failed) continue;

                                        if(lessetB::globals::multiArgFunctions.find(fnName)==lessetB::globals::multiArgFunctions.end() &&
                                        (lessetB::globals::symbols.find(fnName)==lessetB::globals::symbols.end() || lessetB::globals::symbols.find(fnName)->second!=lessetB::token_t::FUNCTION) )
                                        {
                                            if(instances.at(selectedInstance).functions.find(fnName)!=instances.at(selectedInstance).functions.end())
                                            {
                                                if(instances.at(selectedInstance).functions.find(fnName)->second.definition!=definition) instances.at(selectedInstance).functions.at(fnName)=fn;
                                            }
                                            else instances.at(selectedInstance).functions.emplace(fnName,fn);
                                            instances.at(selectedInstance).protectedFunctionDefinitions.emplace(fnName,'c');
                                            instances.at(selectedInstance).lastScriptOutput+="Defined function " + fnSig + " = " + definition + '\n';
                                            continue;
                                        }
                                        else
                                        {
                                            instances.at(selectedInstance).lastScriptOutput+="Function definition failed, cannot shadow predefined functions\n";
                                            continue;
                                        }
                                    }


                                    if(scriptEquation.find("IF")==0)
                                    {
                                        std::string subEquation=scriptEquation.substr(2);
                                        if(subEquation!="")
                                        {
                                            for(size_t i{}; i<subEquation.length(); i++)
                                            {
                                                if(std::isspace(subEquation.at(i)))
                                                {
                                                    subEquation.erase(i--,1);
                                                }
                                            }
                                            size_t subEquationLength=subEquation.length();
                                            lessetB::Options ifOptions=options;
                                            ifOptions.prettyPrinting=false;
                                            evaluateEquation(ifOptions, true, false, subEquation, nothing,conditionValue,instances.at(selectedInstance).variables, instances.at(selectedInstance).functions);

                                            
                                            if(lessetB::isRealNumber(conditionValue) && static_cast<boost::multiprecision::cpp_complex<lessetB::maxPrecision>>(conditionValue).real()>=1)
                                            {
                                                conditionTrue=true;
                                            }
                                            if(calculationsFile.peek()=='\n') for(; calculationsFile.peek()=='\n'; calculationsFile.seekg(static_cast<size_t>(calculationsFile.tellg())+1));
                                            skip=true;
                                            if(conditionTrue) instances.at(selectedInstance).lastScriptOutput+="beginIF\n";
                                            continue;
                                        }
                                    }
                                    if(scriptEquation.find("endIF")==0 && conditionTrue)
                                    {
                                        conditionTrue=false;
                                    }

                                    if(scriptEquation.find("jump")==0 && (!skip || conditionTrue==true))
                                    {
                                        std::string jumpEquation=scriptEquation.substr(4);
                                        if(jumpEquation!="")
                                        {
                                            long long jumpDestination{};
                                            for(size_t i{}; i<jumpEquation.length(); i++)
                                            {
                                                if(std::isspace(jumpEquation.at(i)))
                                                {
                                                    jumpEquation.erase(i--,1);
                                                }
                                            }

                                            size_t subEquationLength=jumpEquation.length();
                                            lessetB::Options jumpOptions=options;
                                            jumpOptions.prettyPrinting=false;
                                            evaluateEquation(jumpOptions, true, false, jumpEquation, nothing,jumpValue,instances.at(selectedInstance).variables, instances.at(selectedInstance).functions); // Lines with # are comments

                                            std::from_chars(jumpValue.data(),jumpValue.data()+jumpValue.length(),jumpDestination);

                                            calculationsFile.seekg(std::ios::beg);
                                            if(jumpDestination<1) jumpDestination=1;
                                            for(size_t i{}; i<jumpDestination-1; i++)
                                            {
                                                calculationsFile.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
                                            }

                                            if(calculationsFile.peek()=='\n') for(; calculationsFile.peek()=='\n'; 
                                            calculationsFile.seekg(static_cast<size_t>(calculationsFile.tellg())+1));
                                            conditionTrue=false;
                                            skip=false;
                                            instances.at(selectedInstance).lastScriptOutput+="Jumped to line "+std::to_string(jumpDestination)+'\n';
                                            continue;
                                        }
                                        
                                    }

                                    if(!skip || conditionTrue==true)
                                    {
                                        evaluateEquation(options, true, true, scriptEquation, nothing,instances.at(selectedInstance).lastScriptOutput,instances.at(selectedInstance).variables, instances.at(selectedInstance).functions); // Lines with # are comments
                                    }
                                    if(calculationsFile.peek()=='\n') for(; calculationsFile.peek()=='\n'; calculationsFile.seekg(static_cast<size_t>(calculationsFile.tellg())+1));
                                }
                                calculationsFile.close();
                            }
                        }

                        if(instances.at(selectedInstance).lastScriptOutput!="")
                        {
                            if(ImGui::BeginMenu("Output"))
                            {
                                ImGui::Text("Output of script:\n%s", instances.at(selectedInstance).lastScriptOutput.c_str());
                                ImGui::EndMenu();
                            }
                        }
                        
                        ImGui::EndMenu();
                    }
                    
                    ImGui::Separator();
                    // Variables Menu
                    if (ImGui::BeginMenu("Variables"))
                    {
                        if (ImGui::BeginMenu("Add or Change"))
                        {
                            ImGui::InputText("Name",&newIdentifierNameVariable);
                            if(newIdentifierNameVariable.length()>MAX_KEYWORD_LENGTH) newIdentifierNameVariable=newIdentifierNameVariable.substr(0,MAX_KEYWORD_LENGTH);
                            ImGui::InputText("Value",&newIdentifierValueVariable);
                            if(ImGui::Button("Assign"))
                            {
                                recalculateGraphs=true;
                                if(lessetB::addVariable(std::pair<std::string,std::string>(newIdentifierNameVariable,newIdentifierValueVariable),instances.at(selectedInstance).variables)) 
                                    assignVariableReport="Cannot use this name.";
                                
                                else 
                                {
                                    boost::multiprecision::cpp_complex<lessetB::maxPrecision> value = static_cast<boost::multiprecision::cpp_complex<lessetB::maxPrecision>>(instances.at(selectedInstance).variables.find(newIdentifierNameVariable)->second);
                                    assignVariableReport="Assigned \"" + newIdentifierNameVariable + "\" value " + value.str(9);
                                }
                            }
                            ImGui::SameLine();
                            ImGui::Text("%s",assignVariableReport.c_str());

                            ImGui::EndMenu();
                        }
                        if (ImGui::BeginMenu("Show"))
                        {
                            if(instances.at(selectedInstance).variables.size()==0) ImGui::MenuItem("You have no variables.",NULL,false,false);
                            else 
                            {
                                ImGui::MenuItem("Click a variable to delete it.",NULL,false,false);
                                ImGui::Separator();
                            }
                            std::string formatted;
                            size_t i{};
                            for(std::pair<std::string,std::string> var : instances.at(selectedInstance).variables)
                            {
                                instances.at(selectedInstance).sliders.emplace(std::pair<std::string,sliderData>{var.first,sliderData{static_cast<float>(static_cast<boost::multiprecision::cpp_complex_100>(var.second).real()),0,10}});
                                formatted=var.first+" = "+static_cast<boost::multiprecision::cpp_complex<lessetB::maxPrecision>>(var.second).str(15);
                                if(ImGui::MenuItem(formatted.c_str()))
                                {
                                    instances.at(selectedInstance).variables.erase(var.first);
                                    instances.at(selectedInstance).sliders.erase(var.first);
                                    recalculateGraphs=true;
                                    break;
                                }
                                ImGui::SetNextItemWidth(110);
                                ImGui::DragFloat(std::string("##min"+std::to_string(i)).c_str(),instances.at(selectedInstance).sliders.at(var.first).nums.data()+1,0.2,-999999,999999,"Min: %.3f");
                                ImGui::SetNextItemWidth(110);
                                ImGui::SameLine();
                                ImGui::DragFloat(std::string("##max"+std::to_string(i)).c_str(),instances.at(selectedInstance).sliders.at(var.first).nums.data()+2,0.2,-999999,999999,"Max: %.3f");
                                ImGui::SameLine();
                                ImGui::Checkbox(std::string("Animate##"+std::to_string(i)).c_str(), &instances.at(selectedInstance).sliders.at(var.first).animate);
                                ImGui::SetItemTooltip("This will cause all graphs to be recalculated every frame. Don't fry your computer lol");
                                if(ImGui::SliderFloat(std::string("##"+std::to_string(i)).c_str(),instances.at(selectedInstance).sliders.at(var.first).nums.data(),instances.at(selectedInstance).sliders.at(var.first).nums[1],instances.at(selectedInstance).sliders.at(var.first).nums[2]))
                                {
                                    lessetB::addVariable(std::pair<std::string,std::string>{var.first,std::to_string(instances.at(selectedInstance).sliders.at(var.first).nums[0])}, instances.at(selectedInstance).variables);
                                    recalculateGraphs=true;
                                }
                                if(i<instances.at(selectedInstance).variables.size()-1) ImGui::Separator();
                                i++;
                            }
                            ImGui::EndMenu();
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }
                ImGui::SetItemTooltip("Automate certain things.");

                // Constants Menu
                if (ImGui::BeginMenu("Constants"))
                {
                    if(ImGui::BeginMenu("Mathematics"))
                    {
                        if(ImGui::MenuItem("π")) equation.append("π");
                        ImGui::SetItemTooltip("3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117068");
                        
                        if(ImGui::MenuItem("ℯ")) equation.append("ℯ");
                        ImGui::SetItemTooltip("2.718281828459045235360287471352662497757247093699959574966967627724076630353547594571382178525166427");
                        
                        if(ImGui::MenuItem("γ")) equation.append("γ");
                        ImGui::SetItemTooltip("0.5772156649015328606065120900824024310421593359399235988057672348848677267776646709369470632917467495");

                        if(ImGui::MenuItem("φ")) equation.append("φ");
                        ImGui::SetItemTooltip("1.618033988749894848204586834365638117720309179805762862135448622705260462818902449707207204189391137");
                        
                        if(ImGui::MenuItem("ξ")) equation.append("ξ");
                        ImGui::SetItemTooltip("A random number between 0 and 1.");

                        if(ImGui::MenuItem("rndint")) equation.append("rndint");
                        ImGui::SetItemTooltip("A random positive integer.");

                        if(ImGui::MenuItem("τ")) equation.append("τ");
                        ImGui::SetItemTooltip("6.283185307179586476925286766559005768394338798750211641949889184615632812572417997256069650684234136");
                        
                        if(ImGui::MenuItem("∞")) equation.append("∞");
                        ImGui::SetItemTooltip("Infinity... infinity... infinity... infinity... infinity...");

                        if(ImGui::MenuItem("prc")) equation.append("prc");
                        ImGui::SetItemTooltip("0.01");

                        if(ImGui::MenuItem("ppm")) equation.append("ppm");
                        ImGui::SetItemTooltip("1×10^-06");

                        if(ImGui::MenuItem("ppb")) equation.append("ppb");
                        ImGui::SetItemTooltip("1×10^-09");

                        if(ImGui::MenuItem("ppt")) equation.append("ppt");
                        ImGui::SetItemTooltip("1×10^-12");

                        if(ImGui::MenuItem("dgr")) equation.append("dgr");
                        ImGui::SetItemTooltip("0.01745329251994329576923690768488612713442871888541725456097191440171009114603449443682241569634509482...\nDegrees to Radiants. Try entering with numbers into trig functions.");

                        if(ImGui::MenuItem("rad")) equation.append("rad");
                        ImGui::SetItemTooltip("57.29577951308232087679815481410517033240547246656432154916024386120284714832155263244096899585111095...");
                        
                        ImGui::EndMenu();
                    }

                    if(ImGui::BeginMenu("Physics"))
                    {
                        if(ImGui::MenuItem("c")) equation.append("ec");
                        ImGui::SetItemTooltip("Speed of light, 299792458");

                        if(ImGui::MenuItem("ec")) equation.append("ec");
                        ImGui::SetItemTooltip("Elementary charge, 1.602176634×10^-19");

                        if(ImGui::MenuItem("G")) equation.append("G");
                        ImGui::SetItemTooltip("Gravitational Constant, 6.6743×10^-11");

                        if(ImGui::MenuItem("g")) equation.append("g");
                        ImGui::SetItemTooltip("Gravity on Earth's surface, 9.80665");
                        
                        if(ImGui::MenuItem("α")) equation.append("α");
                        ImGui::SetItemTooltip("0.0072973525693");

                        if(ImGui::MenuItem("me")) equation.append("me");
                        ImGui::SetItemTooltip("Mass of Earth, 5.9722×10^24");

                        if(ImGui::MenuItem("H0")) equation.append("H0");
                        ImGui::SetItemTooltip("2.2×10^-18");

                        if(ImGui::MenuItem("Z0")) equation.append("Z0");
                        ImGui::SetItemTooltip("376.730313668");

                        if(ImGui::MenuItem("U0")) equation.append("U0");
                        ImGui::SetItemTooltip("1.25663706212×10^-06");

                        if(ImGui::MenuItem("E0")) equation.append("E0");
                        ImGui::SetItemTooltip("8.8541878128×10^-12");

                        if(ImGui::MenuItem("ma")) equation.append("ma");
                        ImGui::SetItemTooltip("1.6605390666×10^-27");

                        if(ImGui::MenuItem("R")) equation.append("R");
                        ImGui::SetItemTooltip("8.31446261815");

                        if(ImGui::MenuItem("Na")) equation.append("Na");
                        ImGui::SetItemTooltip("6.02214076×10^23");

                        if(ImGui::MenuItem("o")) equation.append("o");
                        ImGui::SetItemTooltip("5.670374419×10^-08");

                        if(ImGui::MenuItem("k")) equation.append("k");
                        ImGui::SetItemTooltip("1.380649×10^-23");

                        if(ImGui::MenuItem("a")) equation.append("a");
                        ImGui::SetItemTooltip("0.0072973525693");

                        if(ImGui::MenuItem("h")) equation.append("h");
                        ImGui::SetItemTooltip("6.62607015×10^-34");

                        ImGui::EndMenu();
                    }
                    
                    ImGui::EndMenu();
                }
                ImGui::SetItemTooltip("Add certain constants to your equation.");


                // Graph Menu
                if (ImGui::BeginMenu("Graph"))
                {
                    showGraphingMenuHint=false;
                
                    if(ImGui::Button("Add"))
                    {
                        graphsEquations.push_back({std::string(std::to_string(graphsEquations.size()+1)),std::pair<std::string,lessetB::Function>()});
                        recalculateGraphIndex=graphsEquations.size()-1;
                        graphsPoints.first.emplace_back(std::vector<double>());
                        graphsPoints.second.emplace_back(std::vector<double>());
                    }

                    char nameIndex{};
                    size_t nameNumber{};
                    size_t nameNumberLength{std::to_string((graphsEquations.size()+17)/18).length()};
                    if(graphsEquations.size()<19) nameNumberLength=0; 
                    
                    if(graphsEquations.size()>0) 
                    {
                        ImGui::SameLine(53+(nameNumberLength)*5);
                        if(ImGui::Button("Remove All"))
                        {
                            graphsEquations.clear();
                            graphsPoints.first.clear();
                            graphsPoints.second.clear();

                            graphsEquations.shrink_to_fit();
                            graphsPoints.first.shrink_to_fit();
                            graphsPoints.second.shrink_to_fit();

                            lessetB::globals::points.first.clear();
                            lessetB::globals::points.second.clear();
                            instances.at(selectedInstance).functions.clear();
                            lessetB::globals::tokenMemory.clear();
                        }
                    }
                    if(graphsEquations.size()>0)
                    {
                        ImGui::SameLine();
                        ImGui::MenuItem("Equation left, function signature right.",NULL,false,false);
                    }

                    for(size_t i{}; i<graphsEquations.size(); i++)
                    {                        
                        std::string fnSig=graphsEquations.at(i).second.first+"(";
                        for(size_t j{}; j<graphsEquations.at(i).second.second.argNames.size(); j++)
                        {
                            std::string argName = graphsEquations.at(i).second.second.argNames.at(j);
                            fnSig+=argName;
                            if(j<graphsEquations.at(i).second.second.argNames.size()-1) fnSig+=",";
                        }
                        fnSig+=")";
                        if(fnSig=="()") fnSig.clear();
                        std::string previousEditingGraphEquation = editingGraphEquation;
                        editingGraphEquation=graphsEquations.at(i).first;
                        
                        if(ImGui::InputText(std::string("##" + std::to_string(i)).c_str(), &editingGraphEquation))
                        {
                            std::string editingGraphEquationClosedParentheses=editingGraphEquation;
                            addClosingParentheses(editingGraphEquationClosedParentheses);
                            if(!(graphsEquations.at(i).first==editingGraphEquationClosedParentheses || editingGraphEquationClosedParentheses=="" || editingGraphEquationClosedParentheses.length()>1000))
                            {
                                std::string fnName;
                                if(fnSig.find('(')!=std::string::npos)
                                {
                                    fnName=fnSig.substr(0,fnSig.find('('));
                                }
                                if(fnName!="")
                                {                            
                                    if(lessetB::globals::multiArgFunctions.find(fnName)==lessetB::globals::multiArgFunctions.end() &&
                                    (lessetB::globals::symbols.find(fnName)==lessetB::globals::symbols.end() || lessetB::globals::symbols.find(fnName)->second!=lessetB::token_t::FUNCTION) )
                                    {
        
                                        if(instances.at(selectedInstance).functions.find(fnName)!=instances.at(selectedInstance).functions.end())
                                        {
                                            if(instances.at(selectedInstance).functions.find(fnName)->second.definition!=editingGraphEquation)
                                            {
                                                instances.at(selectedInstance).functions.at(fnName).definition=editingGraphEquation;
                                            }
                                        }
                                        else
                                        {
                                            instances.at(selectedInstance).functions.emplace(fnName,editingGraphEquation);
                                        }
                                        graphsEquations.at(i).second=std::pair<std::string,lessetB::Function>(fnName,lessetB::getFnFromArgs(graphsEquations.at(i).first,fnSig));
                                    }
                                    recalculateGraphs=true;
                                }

                                graphsEquations.at(i).first=editingGraphEquationClosedParentheses; 
                                recalculateGraphIndex=i;
                                
                            }
                        }
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(80);
                        if(ImGui::InputText(std::string("###" + std::to_string(i)).c_str(), &fnSig))
                        {
                            if(fnSig.find(')')==std::string::npos) fnSig.push_back(')');
                            std::string fnName;
                            if(fnSig.find('(')!=std::string::npos)
                            {
                                fnName=fnSig.substr(0,fnSig.find('('));
                            }
                            lessetB::isNameValid(fnName, true);
                            // std::cout<<fnName<<'\n';
                            if(fnName!="" && fnName.find('x')==std::string::npos)
                            {                            
                                if(instances.at(selectedInstance).protectedFunctionDefinitions.find(fnName)==instances.at(selectedInstance).protectedFunctionDefinitions.end() &&
                                   lessetB::globals::multiArgFunctions.find(fnName)==lessetB::globals::multiArgFunctions.end() &&
                                   (lessetB::globals::symbols.find(fnName)==lessetB::globals::symbols.end() || lessetB::globals::symbols.find(fnName)->second!=lessetB::token_t::FUNCTION) )
                                {
                                    graphsEquations.at(i).second=std::pair<std::string,lessetB::Function>(fnName,lessetB::getFnFromArgs(graphsEquations.at(i).first,fnSig));
                                    if(instances.at(selectedInstance).functions.find(fnName)!=instances.at(selectedInstance).functions.end())
                                    {
                                        if(instances.at(selectedInstance).functions.find(fnName)->second.definition!=graphsEquations.at(i).second.second.definition ||
                                           instances.at(selectedInstance).functions.find(fnName)->second.argNames!=graphsEquations.at(i).second.second.argNames)
                                        {
                                            instances.at(selectedInstance).functions.at(fnName)=graphsEquations.at(i).second.second;
                                        }
                                    }
                                    else instances.at(selectedInstance).functions.emplace(fnName,lessetB::getFnFromArgs(graphsEquations.at(i).first,fnSig));
                                }
                                else ImGui::SetItemTooltip("Cannot use this name.");
                                
                            }
                        }
                        ImGui::SameLine();
                        if(ImGui::Button(std::string("Remove##" + std::to_string(i)).c_str()))
                        {
                            if(instances.at(selectedInstance).functions.find(graphsEquations.at(i).second.first)!=instances.at(selectedInstance).functions.end())
                            {
                                instances.at(selectedInstance).functions.erase(graphsEquations.at(i).second.first);
                                lessetB::globals::tokenMemory.clear();
                            }

                            graphsEquations.erase(graphsEquations.begin()+i);

                            if(graphsPoints.first.size()!=0)
                            {
                                graphsPoints.first.erase(graphsPoints.first.begin()+i);
                                graphsPoints.second.erase(graphsPoints.second.begin()+i);
                                graphsPoints.first.shrink_to_fit();
                                graphsPoints.second.shrink_to_fit();
                                recalculateGraphs=true;
                            }
                        }
                        if(editingGraphEquation.length()>1000)
                        {
                            ImGui::Text("Equation should not be longer than 1000 characters.\nPlease, what are you doing?");
                        }
                    
                    }

                    // Erase functions which's definition has been removed or if it was renamed, unless defined in a script
                    std::unordered_map<std::string, char> names;
                    for(size_t i{}; i<graphsEquations.size(); i++)
                    {
                        names.emplace(graphsEquations.at(i).second.first,'c');
                    }
                    names.insert_range(instances.at(selectedInstance).protectedFunctionDefinitions);
                    for(std::pair<std::string,lessetB::Function> fn : instances.at(selectedInstance).functions)
                    {
                        if(names.find(fn.first)==names.end())
                        {
                            instances.at(selectedInstance).functions.erase(fn.first);
                            break;
                        }
                    }

                    ImGui::EndMenu();
                }
                ImGui::SetItemTooltip("Graph functions.");

                // Options Menu
                if(ImGui::BeginMenu("Options"))
                {

                    if(ImGui::BeginMenu("Tables"))
                    {
                        ImGui::SetNextItemWidth(375);
                        if(ImGui::InputText("Min", &xMin,ImGuiInputTextFlags_CharsScientific))
                        {
                            lessetB::Options evalOptions{false,
                                                            0,
                                                            0,
                                                            0,
                                                            0,
                                                            false,
                                                            0,
                                                            "",
                                                            0,
                                                            ""};
                            std::string tmp=xMin;
                            lessetB::evaluateEquation(evalOptions,true,false,tmp,nothing,xMin,instances.at(selectedInstance).variables, instances.at(selectedInstance).functions);
                        }

                        if(lessetB::isRealNumber(xMin) && lessetB::isRealNumber(xMax) && xMin!="" && xMax!="")
                        {
                            if(options.xMax.real()<options.xMin.real()) ImGui::SetItemTooltip("Max is currently less than Min.");
                        }

                        if(lessetB::isRealNumber(xMin) && xMin!="" && xMin!="-")
                        {
                            options.xMin=static_cast<boost::multiprecision::cpp_complex<lessetB::maxPrecision>>(xMin);
                        }
                        
                        ImGui::SetNextItemWidth(375);
                        if(ImGui::InputText("Max", &xMax,ImGuiInputTextFlags_CharsScientific))
                        {
                            lessetB::Options evalOptions{false,
                                                            0,
                                                            0,
                                                            0,
                                                            0,
                                                            false,
                                                            0,
                                                            "",
                                                            0,
                                                            ""};
                            std::string tmp=xMax;
                            lessetB::evaluateEquation(evalOptions,true,false,tmp,nothing,xMax,instances.at(selectedInstance).variables, instances.at(selectedInstance).functions);
                        }

                        if(lessetB::isRealNumber(xMin) && lessetB::isRealNumber(xMax) && xMin!="" && xMax!="")
                        {
                            if(options.xMax.real()<options.xMin.real()) ImGui::SetItemTooltip("Max is currently less than Min.");
                        }

                        if(lessetB::isRealNumber(xMax) && xMax!="" && xMax!="-")
                        {
                            options.xMax=static_cast<boost::multiprecision::cpp_complex<lessetB::maxPrecision>>(xMax);
                        }
                        
                        ImGui::SetNextItemWidth(375);
                        std::string lastXStep=xStep;
                        if(ImGui::InputText("Step", &xStep,ImGuiInputTextFlags_CharsScientific))
                        {
                            std::string tmp=xStep;
                            lessetB::Options evalOptions{false,
                                                            0,
                                                            0,
                                                            0,
                                                            0,
                                                            false,
                                                            0,
                                                            "",
                                                            0,
                                                            ""};
                            
                            lessetB::evaluateEquation(evalOptions,true,false,tmp,nothing,xStep,instances.at(selectedInstance).variables, instances.at(selectedInstance).functions);
                        }
                        

                        if(lessetB::isRealNumber(xStep) && xStep!="" && static_cast<boost::multiprecision::cpp_complex<lessetB::maxPrecision>>(xStep).real()>0)
                        {
                            options.xStep=static_cast<boost::multiprecision::cpp_complex<lessetB::maxPrecision>>(xStep);
                        }
                        else xStep=lastXStep;
                        if(static_cast<size_t>(abs(options.xMax-options.xMin)/abs(options.xStep))+1>100000) ImGui::Text("Maximum points for calculation is 100,000"); // It's 100,001 but who cares
                        else if(static_cast<size_t>(abs(options.xMax-options.xMin)/abs(options.xStep))+1>1000) ImGui::SetItemTooltip("You'll calculate over 1,000 points.");

                        ImGui::EndMenu();
                    }
                    ImGui::SetItemTooltip("Adjust bounds and step size for table calculations.");

                    if(ImGui::BeginMenu("Graphs"))
                    {
                        if(maxIndividualGraphPointsMultiplier>5) maxIndividualGraphPointsMultiplier=5;
                        if(maxIndividualGraphPointsMultiplier<1) maxIndividualGraphPointsMultiplier=1;
                        std::string shownResolution="Medium";
                        
                        switch(maxIndividualGraphPointsMultiplier)
                        {
                            case 1: {maxIndividualGraphPoints=MAX_GRAPH_POINTS_BASE/2; shownResolution="Low"; break;}
                            case 2: {maxIndividualGraphPoints=MAX_GRAPH_POINTS_BASE; shownResolution="Medium"; break;}
                            case 3: {maxIndividualGraphPoints=MAX_GRAPH_POINTS_BASE*2; shownResolution="High"; break;}
                            case 4: {maxIndividualGraphPoints=MAX_GRAPH_POINTS_BASE*4; shownResolution="Overkill"; break;}
                            case 5: {maxIndividualGraphPoints=MAX_GRAPH_POINTS_BASE/3; shownResolution="Too Low"; break;}

                            default: {maxIndividualGraphPoints=MAX_GRAPH_POINTS_BASE/2; shownResolution="Low"; break;}
                        }
                        ImGui::SetNextItemWidth(220.f);
                        if(ImGui::SliderInt("Resolution",&maxIndividualGraphPointsMultiplier,1,5,shownResolution.c_str()))
                        {
                            recalculateGraphs=true;
                        }
                        ImGui::SetItemTooltip("Changes how precisely graphs are calculated, how many points per graph.\nOverkill might be useful for detailed graphs.\nOnly use Too Low if you have many graphs, are creating insane functions or your computer sux.");

                        if(ImGui::Checkbox("Connect Discontinuities",&interpolateDiscontinuities))
                        {
                            recalculateGraphs=true;
                        }
                        ImGui::SetItemTooltip("This calculator is stupid and doesn't actually know where exactly a discontinuity in a function like 1/x is.\nThus, it tries to approximate it, but sometimes ends up creating visual artifacts in continuous functions.");
                        ImGui::SameLine();
                        ImGui::Checkbox("Mark Points",&markSpecialPoints);
                        ImGui::SetItemTooltip("Mark points where a function crosses 0, the point closest to the cursor, extremes.");
                        if(interpolateDiscontinuities) options.interpolateDiscontinuities=true;

                        ImGui::EndMenu();
                    }

                    if(ImGui::BeginMenu("Misc."))
                    {
                         ImGui::SetNextItemWidth(248.f);
                        // ImGui::SliderFloat("Max error for ≈",&aroundTruthinessLeniencyFloat,0,1);
                        
                        if(ImGui::InputText("Max Error for ≈", &aroundTruthinessLeniency,ImGuiInputTextFlags_CharsScientific))
                        {
                            lessetB::Options evalOptions{false,
                                                            0,
                                                            0,
                                                            0,
                                                            0,
                                                            false,
                                                            0,
                                                            "",
                                                            0,
                                                            ""};
                            std::string tmp=aroundTruthinessLeniency;
                            lessetB::evaluateEquation(evalOptions,true,false,tmp,nothing,aroundTruthinessLeniency,instances.at(selectedInstance).variables, instances.at(selectedInstance).functions);
                        }

                        if(lessetB::isRealNumber(aroundTruthinessLeniency) && aroundTruthinessLeniency!="")
                        {
                            aroundTruthinessLeniencyFloat=static_cast<boost::multiprecision::cpp_complex<lessetB::maxPrecision>>(aroundTruthinessLeniency);
                        }
                        
                        ImGui::SetItemTooltip("When 2 values are close to equal, what is the maximum difference for which ≈ returns true?");
                        if(aroundTruthinessLeniencyFloat.real()<0) 
                        {
                            aroundTruthinessLeniencyFloat=0;
                            aroundTruthinessLeniency="0";
                        }
                        if(aroundTruthinessLeniencyFloat.real()>1)
                        {
                            aroundTruthinessLeniencyFloat=1;
                            aroundTruthinessLeniency="1";
                        }
                        options.aroundTruthinessLeniency=aroundTruthinessLeniencyFloat;
                        
                        ImGui::Checkbox("Pretty Output",&showFractions);
                        options.prettyPrinting=showFractions;
                        ImGui::SetItemTooltip("Show some results as fractions or constants instead of decimal numbers.");
                        ImGui::SameLine();
                        ImGui::Checkbox("Prioritize Implicit Multiplication",&prioritizeImplicitMultiplication);
                        options.prioritizeImplicitMultiplication=prioritizeImplicitMultiplication;
                        ImGui::SetItemTooltip("Disambiguate something like 8÷2(2+2) as 8÷(2(2+2))=1 instead of (8÷2)(2+2)=16.");

                        ImGui::SetNextItemWidth(350);
                        ImGui::DragInt("##", &lessetB::globals::decimalPrecision,0.2,1,lessetB::maxPrecision,"Decimal Places in Output: %d",ImGuiSliderFlags_AlwaysClamp);
                        ImGui::SetItemTooltip("Changes amount of decimal places shown in output.");

                        ImGui::EndMenu();
                    }
                    ImGui::Separator();
                    if(ImGui::BeginMenu("Style"))
                    {
                        ImGui::Text("Save and Load styles in ini format.");
                        if(!showGayMode) ImGui::SetNextItemWidth(300);
                        if(ImGui::SliderFloat("##",&hueModifier,0,1,"Hue: %.3f"))
                        {
                            for(size_t i{6}; i<ImGuiCol_COUNT; i++)
                            {
                                if(i==ImGuiCol_MenuBarBg) continue;
                                ImVec4 col{0,0,0,1};
                                ImGui::ColorConvertRGBtoHSV(style.Colors[i].x,style.Colors[i].y,style.Colors[i].z, col.x, col.y, col.z);
                                col.x=hueModifier;
                                ImGui::ColorConvertHSVtoRGB( col.x, col.y, col.z,style.Colors[i].x,style.Colors[i].y,style.Colors[i].z);
                            }
                        }
                        
                        if(!showGayMode) ImGui::SetNextItemWidth(300);
                        if(ImGui::SliderFloat("##1",&saturationModifier,0,1, "Saturation: %.3f"))
                        {
                            for(size_t i{6}; i<ImGuiCol_COUNT; i++)
                            {
                                if(i==ImGuiCol_MenuBarBg) continue;
                                ImVec4 col{0,0,0,1};
                                ImGui::ColorConvertRGBtoHSV(style.Colors[i].x,style.Colors[i].y,style.Colors[i].z, col.x, col.y, col.z);
                                col.x=hueModifier;
                                col.y=saturationModifier;
                                
                                ImGui::ColorConvertHSVtoRGB( col.x, col.y, col.z,style.Colors[i].x,style.Colors[i].y,style.Colors[i].z);
                            }
                        }
                        
                        if(ImGui::Button("Save Style"))
                        {
                            if(ImGui::IsKeyDown(ImGuiMod_Shift))
                            {
                            nfdu8char_t *outPath{};
                            nfdsavedialogu8args_t args = {0};
                            args.defaultName="LessetStyle.ini";
                            nfdresult_t result = NFD_SaveDialogU8_With(&outPath, &args);
                            if(result == NFD_OKAY) ImGui::SaveStylesTo(std::string(outPath).c_str());
                            }
                            else ImGui::SaveStylesTo("LessetStyle.ini");
                            
                        }
                        ImGui::SetItemTooltip("Use shift to open a file dialog instead of saving to executable directory.");
                        ImGui::SameLine();
                        if(ImGui::Button("Load Style"))
                        {
                            if(std::filesystem::exists(std::string(std::string(std::filesystem::current_path())+std::string("/LessetStyle.ini"))) && !ImGui::IsKeyDown(ImGuiMod_Shift))
                                ImGui::LoadStyleFrom("LessetStyle.ini");
                            else
                            {
                                nfdu8char_t *outPath{};
                                nfdopendialogu8args_t args = {0};
                                nfdresult_t result = NFD_OpenDialogU8_With(&outPath, &args);
                                if(result == NFD_OKAY)
                                {
                                    ImGui::LoadStyleFrom(outPath);
                                }
                            }
                        }
                        if(std::filesystem::exists(std::string(std::string(std::filesystem::current_path())+std::string("/LessetStyle.ini"))))
                        {
                            ImGui::SetItemTooltip("Use shift to open a file dialog instead of loading from executable directory.");
                        }
                        else ImGui::SetItemTooltip("Save \"LessetStyle.ini\" to the executable's path to auto load it on startup.");

                        if(showGayMode)
                        {
                            ImGui::SameLine();
                            ImGui::Checkbox("Gay", &gayMode);
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(98);
                            ImGui::DragFloat("##2", &gayModeSpeed,0.00003,-0.01,0.01,"Speed: %.4f",ImGuiSliderFlags_AlwaysClamp);
                        }
                        // else ImGui::SameLine();
                        // ImGui::Checkbox("Decimal Comma", &lessetB::globals::useDecimalComma);
                        // ImGui::SetItemTooltip("Use comma for decimal places and semicolon for function argument separation.");

                        ImGui::EndMenu();
                    }
                    
                    ImGui::EndMenu();
                }
                ImGui::SetItemTooltip("Tweak some settings.");

                if(ImGui::BeginMenu("History"))
                {
                    if(resultHistory!="")
                    {
                        if(ImGui::Button("Clear"))
                        {
                            resultHistory="";
                            lessetB::globals::tokenMemory.clear();
                            lessetB::globals::ans.clear();
                            options.ans="NAN";
                        }
                    }
                    else
                    {
                        ImGui::MenuItem("You have no history.",NULL,false,false);
                    }
                    std::string line;
                    // ImGui::Text("%s",resultHistory.c_str());
                    
                    for(size_t i{1}; i<resultHistory.length();)
                    {
                        line=resultHistory.substr(i,resultHistory.find('\n',i)-i);
                        if(ImGui::Button(line.c_str()))
                        {
                            ImGui::SetClipboardText(line.c_str());
                        }
                        ImGui::SetItemTooltip("Click to copy.");
                        i+=line.length()+1;
                    }


                    ImGui::EndMenu();
                }
                ImGui::SetItemTooltip("View past calculations.");

                if (ImGui::BeginMenu("Help"))
                {
                    if(ImGui::BeginMenu("Operators"))
                    {   
                        if(ImGui::MenuItem("+")) equation.append("+");
                        ImGui::SetItemTooltip("Adds left and right or does nothing (unary plus). Is a tooltip really needed for this..?");
                        
                        if(ImGui::MenuItem("-")) equation.append("-");
                        ImGui::SetItemTooltip("Subtracts left and right or negates right.");

                        if(ImGui::MenuItem("×")) equation.append("×");
                        ImGui::SetItemTooltip("Multiplication. (*)");

                        if(ImGui::MenuItem("/")) equation.append("/");
                        ImGui::SetItemTooltip("Division... please not by 0. (÷)");

                        if(ImGui::MenuItem("//")) equation.append("//");
                        ImGui::SetItemTooltip("Floor division. Only works on real numbers.");

                        if(ImGui::MenuItem("^")) equation.append("^");
                        ImGui::SetItemTooltip("Exponentiation. (**)");

                        if(ImGui::MenuItem("!")) equation.append("!");
                        ImGui::SetItemTooltip("Factorial.");

                        if(ImGui::MenuItem("!!")) equation.append("!!");
                        ImGui::SetItemTooltip("Double-Factorial.");

                        if(ImGui::MenuItem("mod")) equation.append("mod");
                        ImGui::SetItemTooltip("Modulus, division with remainder. (%%)\nFloors the result of the division. fmod truncates it, rmod rounds it.");

                        if(ImGui::MenuItem("nPk")) equation.append("nPk");
                        ImGui::SetItemTooltip("Permutations.");

                        if(ImGui::MenuItem("nCk")) equation.append("nCk");
                        ImGui::SetItemTooltip("Binomial coefficient.");

                        if(ImGui::MenuItem("|expr|")) equation.append("||");
                        ImGui::SetItemTooltip("Absolute value.");

                        if(ImGui::MenuItem("(expr)")) equation.append("()");
                        ImGui::SetItemTooltip("Parentheses.");

                        ImGui::EndMenu();
                    }
                    if(ImGui::BeginMenu("Functions"))
                    {
                        if(ImGui::BeginMenu("Common"))
                        {
                            if(ImGui::MenuItem("√")) equation.append("√");
                            ImGui::SetItemTooltip("Square root (sqrt) function.");

                            if(ImGui::MenuItem("∛")) equation.append("∛");
                            ImGui::SetItemTooltip("Cube root (cbrt) function.");

                            if(ImGui::MenuItem("∜")) equation.append("∜");
                            ImGui::SetItemTooltip("Quartic root (qtrt) function.");

                            if(ImGui::MenuItem("root()")) equation.append("root(radicand, degree)");
                            ImGui::SetItemTooltip("Nth root function, radicand on the left, degree right.\nMay be called with one argument for sqrt.");
           
                            if(ImGui::MenuItem("ln")) equation.append("ln");
                            ImGui::SetItemTooltip("Log with base ℯ.");

                            if(ImGui::MenuItem("log()")) equation.append("log(base, value)");
                            ImGui::SetItemTooltip("Generic log function, base on the left, expression right.\nMay be called with one argument for log10(expr).");

                            if(ImGui::MenuItem("abs")) equation.append("abs");
                            ImGui::SetItemTooltip("Absolute value as a function.");

                            if(ImGui::MenuItem("sin")) equation.append("sin");
                            ImGui::SetItemTooltip("Sine function.");

                            if(ImGui::MenuItem("cos")) equation.append("cos");
                            ImGui::SetItemTooltip("Cosine function.");      
 
                            if(ImGui::MenuItem("tan")) equation.append("tan");
                            ImGui::SetItemTooltip("Tangent function.");    
      
                            if(ImGui::MenuItem("asin")) equation.append("asin");
                            ImGui::SetItemTooltip("Arcsine function.");

                            if(ImGui::MenuItem("acos")) equation.append("acos");
                            ImGui::SetItemTooltip("Arccosine function.");      
 
                            if(ImGui::MenuItem("atan")) equation.append("atan");
                            ImGui::SetItemTooltip("Arctangent function.");          
                                                     
                            ImGui::EndMenu();
                        }

                        if(ImGui::BeginMenu("Trig"))
                        {
                            if(ImGui::MenuItem("sin")) equation.append("sin");
                            ImGui::SetItemTooltip("Sine function.");

                            if(ImGui::MenuItem("cos")) equation.append("cos");
                            ImGui::SetItemTooltip("Cosine function.");      
 
                            if(ImGui::MenuItem("tan")) equation.append("tan");
                            ImGui::SetItemTooltip("Tangent function.");           
                            
                            if(ImGui::MenuItem("sec")) equation.append("sec");
                            ImGui::SetItemTooltip("sec(x) = 1/cos(x)");         

                            if(ImGui::MenuItem("csc")) equation.append("csc");
                            ImGui::SetItemTooltip("csc(x) = 1/sin(x).");   

                            if(ImGui::MenuItem("cot")) equation.append("cot");
                            ImGui::SetItemTooltip("cot(x) = 1/tan(x).");   

                            if(ImGui::MenuItem("asin")) equation.append("asin");
                            ImGui::SetItemTooltip("Arcsine function.");

                            if(ImGui::MenuItem("acos")) equation.append("acos");
                            ImGui::SetItemTooltip("Arccosine function.");      
 
                            if(ImGui::MenuItem("atan")) equation.append("atan");
                            ImGui::SetItemTooltip("Arctangent function.");           

                            if(ImGui::MenuItem("atan2()")) equation.append("atan2(y,x)");
                            ImGui::SetItemTooltip("Two-argument arctangent.");   
                            
                            if(ImGui::MenuItem("asec")) equation.append("asec");
                            ImGui::SetItemTooltip("Arcsecant function.");         

                            if(ImGui::MenuItem("acsc")) equation.append("acsc");
                            ImGui::SetItemTooltip("Arccosecant function.");   

                            if(ImGui::MenuItem("acot")) equation.append("acot");
                            ImGui::SetItemTooltip("Arccotangent function.");  

                            if(ImGui::MenuItem("sinc")) equation.append("sinc");
                            ImGui::SetItemTooltip("sinc(x) = sin(x)/x /; x ≠ 0, sinc(0) = 1.");

                            if(ImGui::MenuItem("cis")) equation.append("cis");
                            if(ImGui::IsItemHovered())
                            {
                                std::uniform_int_distribution<> intDist(0,1000);
                                if(tooltipRnd==0) tooltipRnd = intDist(lessetB::randomMt);
                                if(tooltipRnd!=1000)
                                ImGui::SetItemTooltip("cis(x) = cos(x)+isin(x) = e^(ix).");
                                else ImGui::SetItemTooltip("Something I'm not.");
                            }
                            else tooltipRnd=0;
                            if(ImGui::MenuItem("cas")) equation.append("cas");
                            ImGui::SetItemTooltip("cas(x) = sin(x) + cos(x).");
                            
                            
                            ImGui::EndMenu();
                        }
                        ImGui::SetItemTooltip("Squared variants (sin^2) exist too.");

                        if(ImGui::BeginMenu("Hyperbolic"))
                        {
                            if(ImGui::MenuItem("sinh")) equation.append("sinh");
                            ImGui::SetItemTooltip("Hyperbolic sine function.");

                            if(ImGui::MenuItem("cosh")) equation.append("cosh");
                            ImGui::SetItemTooltip("Hyperbolic cosine function.");      
 
                            if(ImGui::MenuItem("tanh")) equation.append("tanh");
                            ImGui::SetItemTooltip("Hyperbolic tangent function.");           
                            
                            if(ImGui::MenuItem("sech")) equation.append("sech");
                            ImGui::SetItemTooltip("Hyperbolic secant function.");         

                            if(ImGui::MenuItem("csch")) equation.append("csch");
                            ImGui::SetItemTooltip("Hyperbolic cosecant function.");   

                            if(ImGui::MenuItem("coth")) equation.append("coth");
                            ImGui::SetItemTooltip("Hyperbolic cotangent function.");   

                            if(ImGui::MenuItem("asinh")) equation.append("asinh");
                            ImGui::SetItemTooltip("Hyperbolic arcsine function.");

                            if(ImGui::MenuItem("acosh")) equation.append("acosh");
                            ImGui::SetItemTooltip("Hyperbolic arccosine function.");      
 
                            if(ImGui::MenuItem("atanh")) equation.append("atanh");
                            ImGui::SetItemTooltip("Hyperbolic arctangent function.");           
                            
                            if(ImGui::MenuItem("asech")) equation.append("asech");
                            ImGui::SetItemTooltip("Hyperbolic arcsecant function.");         

                            if(ImGui::MenuItem("acsch")) equation.append("acsch");
                            ImGui::SetItemTooltip("Hyperbolic arccosecant function.");   

                            if(ImGui::MenuItem("acoth")) equation.append("acoth");
                            ImGui::SetItemTooltip("Hyperbolic arccotangent function.");  
                            
                            ImGui::EndMenu();
                        }
                        ImGui::SetItemTooltip("Squared variants (sinh^2) exist too.");

                        if(ImGui::BeginMenu("Calculus"))
                        {    
                            if(ImGui::MenuItem("exp")) equation.append("exp");
                            ImGui::SetItemTooltip("exp(x) = e^x");

                            if(ImGui::MenuItem("ln")) equation.append("ln");
                            ImGui::SetItemTooltip("Log with base ℯ.");

                            if(ImGui::MenuItem("log()")) equation.append("log(base,value)");
                            ImGui::SetItemTooltip("Generic log function, base on the left, expression right.\nMay be called with one argument for log10(expr).");

                            if(ImGui::MenuItem("diff()")) equation.append("diff(expr)");
                            ImGui::SetItemTooltip("Approximates a derivative. Takes one argument.");
                              
                            // if(ImGui::MenuItem("sum()")) equation.append("sum(expr, min, max)");
                            // ImGui::SetItemTooltip("Like Σ. Argument 1 is an expression (which may contain n), Argument 2 is minimum n, Argument 3 is maximum n\nWarning: this function is comically slow.");

                            if(ImGui::MenuItem("lgam")) equation.append("lgam");
                            ImGui::SetItemTooltip("Lgamma.");

                            if(ImGui::MenuItem("gam")) equation.append("gam");
                            ImGui::SetItemTooltip("Gamma.");
                                                                                  
                            ImGui::EndMenu();
                        }

                        if(ImGui::BeginMenu("Num. Theory"))
                        {
                            if(ImGui::MenuItem("root()")) equation.append("root(denominator,enumerator)");
                            ImGui::SetItemTooltip("Nth root function, denominator on the left, enumerator right.\nMay be called with one argument for sqrt.");
              
                            if(ImGui::MenuItem("round")) equation.append("round");
                            ImGui::SetItemTooltip("Rounds number to nearest integer.");

                            if(ImGui::MenuItem("round()")) equation.append("round(expr, decimal places)");
                            ImGui::SetItemTooltip("Rounds number to however many decimal places specified.");

                            if(ImGui::MenuItem("bround")) equation.append("bround");
                            ImGui::SetItemTooltip("Rounds number to nearest even integer (Banker's Rounding).");

                            if(ImGui::MenuItem("floor")) equation.append("floor");
                            ImGui::SetItemTooltip("Lowers number to next integer.");

                            if(ImGui::MenuItem("ceil")) equation.append("ceil");
                            ImGui::SetItemTooltip("Raises number to next integer.");

                            if(ImGui::MenuItem("trunc")) equation.append("trunc");
                            ImGui::SetItemTooltip("Truncates number to nearest integer closer to 0.");

                            if(ImGui::MenuItem("trunc()")) equation.append("trunc(expr, decimal places)");
                            ImGui::SetItemTooltip("Truncates number to however many decimal places specified.");

                            if(ImGui::MenuItem("abs")) equation.append("abs");
                            ImGui::SetItemTooltip("Absolute value as a function.");

                            if(ImGui::MenuItem("sabs()")) equation.append("sabs(x,λ)");
                            ImGui::SetItemTooltip("Absolute value, but smooth.\nArgument 1 is f(x), argument 2 λ (blending)");

                            if(ImGui::MenuItem("sign")) equation.append("sign");
                            ImGui::SetItemTooltip("Returns sign of input. N<0 => -1, 0 => 0, N>0 => 1");

                            if(ImGui::MenuItem("gcf()")) equation.append("gcf(round )");
                            ImGui::SetItemTooltip("Calculates the greatest common factor, takes multiple arguments.\nWill cause extreme output for numbers with many decimal places.\nInputs will be rounded when graphing.");

                            if(ImGui::MenuItem("lcm()")) equation.append("lcm(round ");
                            ImGui::SetItemTooltip("Calculates the lowest common multiple, takes multiple arguments.\nWill cause extreme output for numbers with many decimal places.\nInputs will be rounded when graphing.");

                            if(ImGui::MenuItem("prime")) equation.append("prime");
                            ImGui::SetItemTooltip("Returns 1 if a number is prime, else return 0.\nTry graphing this one, it's cool.");

                            ImGui::EndMenu();
                        }

                        if(ImGui::BeginMenu("Statistics"))
                        {
                            if(ImGui::MenuItem("rndint()")) equation.append("rndint(min, max)");
                            ImGui::SetItemTooltip("Takes a lower bound and upper bound, returns a random integer in range.");
                            
                            if(ImGui::MenuItem("mean()")) equation.append("mean(");
                            ImGui::SetItemTooltip("Averages inputs, takes multiple arguments.");

                            if(ImGui::MenuItem("median()")) equation.append("median(");
                            ImGui::SetItemTooltip("Evaluates all inputs and returns median, takes multiple arguments.\nWorks with only the real part of arguments.");

                            if(ImGui::MenuItem("stdevp()")) equation.append("stdevp(");
                            ImGui::SetItemTooltip("Population standard deviation, takes multiple arguments.");

                            if(ImGui::MenuItem("max()")) equation.append("max(");
                            ImGui::SetItemTooltip("Evaluates all inputs and returns greatest, takes multiple arguments.");

                            if(ImGui::MenuItem("min()")) equation.append("min(");
                            ImGui::SetItemTooltip("Evaluates all inputs and returns lowest, takes multiple arguments.");

                            // if(ImGui::MenuItem("smax()")) equation.append("smax(f(x), g(x), λ)");
                            // ImGui::SetItemTooltip("Smooth Maximum for 2 functions. Takes three arguments.\nArgument 1 is f(x), argument 2 g(x), argument 3 λ (blending)");

                            // if(ImGui::MenuItem("smin()")) equation.append("smin(f(x), g(x), λ)");
                            // ImGui::SetItemTooltip("Smooth Minimum for 2 functions. Takes three arguments.\nArgument 1 is f(x), argument 2 g(x), argument 3 λ (blending)");

                            ImGui::EndMenu();
                        }

                        if(ImGui::BeginMenu("Complex"))
                        {
                            if(ImGui::MenuItem("real")) equation.append("real");
                            ImGui::SetItemTooltip("Returns real part of input.");

                            if(ImGui::MenuItem("imag")) equation.append("imag");
                            ImGui::SetItemTooltip("Returns imaginary part of input as a real number.");

                            if(ImGui::MenuItem("arg")) equation.append("arg");
                            ImGui::SetItemTooltip("Counter-clockwise angle from positive real axis to line from origin to complex number.");

                            if(ImGui::MenuItem("conj")) equation.append("conj");
                            ImGui::SetItemTooltip("Flips sign of imaginary part of a number.");

                            if(ImGui::MenuItem("norm")) equation.append("norm");
                            ImGui::SetItemTooltip("Squares real and imaginary parts.");
                            
                            ImGui::EndMenu();
                        }

                        if(ImGui::BeginMenu("Misc."))
                        {
                            if(ImGui::MenuItem("rndsel()")) equation.append("rndsel(");
                            ImGui::SetItemTooltip("Evaluates all inputs and returns a random one, takes multiple arguments.");

                            if(ImGui::MenuItem("sat")) equation.append("sat");
                            ImGui::SetItemTooltip("Linear transition between 0 and 1.");
                            
                            if(ImGui::MenuItem("sstep")) equation.append("sstep");
                            ImGui::SetItemTooltip("Smooth transition between 0 and 1.");

                            if(ImGui::MenuItem("mix()")) equation.append("mix(min, max, f(x)");
                            ImGui::SetItemTooltip("Mixes args 1, 2, by a function between 0 and 1.");

                            if(ImGui::MenuItem("ReLU")) equation.append("ReLU");
                            ImGui::SetItemTooltip("if(f(x)>0), ReLU(f(x))=f(x), else ReLU(x)=0)");

                            if(ImGui::MenuItem("fish")) equation.append("fish.");
                            ImGui::SetItemTooltip("fishifies your equation.");

                            ImGui::EndMenu();
                        }

                        if(ImGui::BeginMenu("Custom"))
                        {
                            if(instances.at(selectedInstance).functions.size()==0) ImGui::Text("You can make custom functions in the graphs menu.");
                            size_t i{};
                            for(std::pair<std::string,lessetB::Function> fn : instances.at(selectedInstance).functions)
                            {
                                std::string argNames;
                                for(size_t j{}; j<instances.at(selectedInstance).functions.at(fn.first).argNames.size(); j++)
                                {
                                    std::string argName = instances.at(selectedInstance).functions.at(fn.first).argNames.at(j);
                                    argNames+=argName;
                                    if(j<instances.at(selectedInstance).functions.at(fn.first).argNames.size()-1) argNames+=",";
                                }
                                if(ImGui::MenuItem(std::string(fn.first+"(" +argNames+ ")##"+std::to_string(i)).c_str()))
                                {
                                    equation.append(fn.second.definition);
                                }
                                ImGui::SetItemTooltip("%s",fn.second.definition.c_str());
                                i++;
                            }
                            ImGui::EndMenu();
                        }

                        ImGui::EndMenu();
                    }
                    if(ImGui::BeginMenu("Logic"))
                    {
                        ImGui::MenuItem("false = 0, true = 1, if N≠0 => true",NULL,false,false);

                        if(ImGui::MenuItem("if()")) equation.append("if(condition, true, false)");
                        ImGui::SetItemTooltip("Argument 1 is a condition, argument 2 returns if true, argument 3 returns if false (optional).\nProbably useful for piecewise equations?");

                        if(ImGui::MenuItem("<")) equation.append("<");
                        ImGui::SetItemTooltip("Returns true if left less then right.");
                       
                        if(ImGui::MenuItem(">")) equation.append(">");
                        ImGui::SetItemTooltip("Returns true if left greater then right."); 

                        if(ImGui::MenuItem("≤")) equation.append("≤");
                        ImGui::SetItemTooltip("Returns true if left less than or equal to right. (<=)");
                        
                        if(ImGui::MenuItem("≥")) equation.append("≥");
                        ImGui::SetItemTooltip("Returns true if left greater than or equal to right. (>=)");

                        if(ImGui::MenuItem("=")) equation.append("=");
                        ImGui::SetItemTooltip("Returns true if left is exactly equal to right.\nThis can often be falsified by floating point inaccuracy.");

                        if(ImGui::MenuItem("≠")) equation.append("≠");
                        ImGui::SetItemTooltip("Returns true if left not equal to right. (=!)");

                        if(ImGui::MenuItem("≈")) equation.append("≈");
                        ImGui::SetItemTooltip("Returns true if left close to equal to right. (AROUND)\nCan be configured in options.\nUseful in graphing intersections, zero points, so on.");

                        if(ImGui::MenuItem("∨")) equation.append("∨");
                        ImGui::SetItemTooltip("Logical or, returns true if either side is true. (OR)");

                        if(ImGui::MenuItem("∧")) equation.append("∧");
                        ImGui::SetItemTooltip("Logical and, returns true if both sides are true. (AND)");

                        if(ImGui::MenuItem("⊕")) equation.append("⊕");
                        ImGui::SetItemTooltip("Logical exclusive or, returns true if either side is true. (XOR)");

                        ImGui::EndMenu();
                    }
                    ImGui::Separator();
                    if(ImGui::BeginMenu("Technical"))
                    {

                        if(ImGui::BeginMenu("Evaluation"))
                        {
                            ImGui::Text("Input is evaluated in the following order:\n- Subexpressions/multi argument functions, 2 + {lcm(3,2)}\n\n- Functions, 3{sin x}\n\n- Unary Operators, 2+{3!}\n\n- Exponentiation, 3{e^2}\n\n- Negation, 3/{- 2}\n\n- Implicit Multiplication, {3(2)}*3\n\tNote, this can be merged with explicit using an option\n\n- \"Multiplication\" (many included here), 2+{3*2}, 2+{3mod2}\n\n- Addition, {3 + -2} = 1\n\tNote: Subtraction works as addition by a negative number\n\n- Comparisons, {3>2} AND true\n\n- Logical Operators, {true AND 1}\n\nBesided exponentiation, operators are left-associative.\nFunctions like round are the exception to this; if no\nsubexpression follows, like in sin 3x, one is formed on its own.\nsin 3x(2) -> sin(3x)*(2), sin -3x^2 - 2 -> sin(-3x^2) - 2, don't get\nconfused by complex numbers written as (real,imag)");
                            ImGui::EndMenu();
                        }

                        if(ImGui::BeginMenu("Instances"))
                        {
                            ImGui::Text("An instance holds its own variables, sliders and functions.\nIt is recommended to run a script in its own instance.");
                            ImGui::EndMenu();
                        }
                        if(ImGui::BeginMenu("Scripting"))
                        {
                            ImGui::Text("Scripting is a way to automate certain processes.");
                            if(ImGui::BeginMenu("Commands"))
                            {
                                if(ImGui::BeginMenu("let"))
                                {
                                    ImGui::Text("Let an expression be saved to a variable.\nMultiple assignments can be on one line.");
                                    ImGui::EndMenu();
                                }

                                if(ImGui::BeginMenu("graph"))
                                {
                                    ImGui::Text("Add an equation to be graphed.\nUse numof before a variable to use its value.");
                                    ImGui::EndMenu();
                                }

                                if(ImGui::BeginMenu("IF"))
                                {
                                    ImGui::Text("Run a block of commands only if an expression following 'IF' is true.\nEnd block with endIF, no nested IF statements are supported.\nThis is not the same as if() in the calculator.");
                                    ImGui::EndMenu();
                                }

                                if(ImGui::BeginMenu("jump"))
                                {
                                    ImGui::Text("Jump to any line in the script. Nothing bad will ever be caused by this statement.");
                                    ImGui::EndMenu();
                                }

                                ImGui::EndMenu();
                            }
                            if(ImGui::BeginMenu("Syntax"))
                            {
                                ImGui::Text("Scripts are almost like regular input line by line, except with extra commands.\nIt does not care about whitespace other than newlines.\nIdentifier syntax is as follows:\nlet/set name = value");
                                ImGui::EndMenu();
                            }
                            ImGui::EndMenu();
                        }
                        if(ImGui::BeginMenu("Notes"))
                        {
                            ImGui::Text("Variables have full precision, even if not all the digits are shown.\nFunctions and variables may share a name, using the name with parentheses following means you call the function!\nIn this case, it may be good to make your multiplication explicit.");
                            ImGui::EndMenu();
                        }
                        ImGui::EndMenu();
                    }

                    if(ImGui::BeginMenu("Credits"))
                    {   
                        ImGui::Text("Glued together by Dummigame.\nAlso responsible for the calculator behind this n stuff.\nI am not that good at math btw. If this thing tells you 2+2=5, call me.\n\nLibraries used in this project:\nImGui, ImPlot, ImStyle, Boost lib, Native File Dialog Extended \n(MIT License, https://github.com/ocornut/imgui,\nMIT License, https://github.com/epezent/implot,\nBSD 3-Clause License, https://github.com/csprite/ImStyle,\nBoost Software License, https://www.boost.org/LICENSE_1_0.txt\nzlib License, https://github.com/btzy/nativefiledialog-extended)");

                        ImGui::EndMenu();
                    }
                    if(showTrivia)
                    {
                        if(ImGui::BeginMenu("Trivia"))
                        {
                            ImGui::Text("Technically, all mutli argument functions are variadic, meaning you can supply an arbitrary number of arguments.\nWhether all of them are considered depends on the function.\n");
                            ImGui::EndMenu();
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::SetItemTooltip("Learn about the calculator.");
                if(graphsEquations.size()>=MANY_GRAPHS)
                {
                    if(ImGui::Checkbox("Draw many graphs",&drawMany_Graphs))
                    {
                        recalculateGraphs=true;
                    }
                }
                
                if(graphsEquations.size() > previousGraphsEquationsSize)
                {
                    for(size_t i{}; i<graphsEquations.size(); i++)
                    {
                        addClosingParentheses(graphsEquations.at(i).first);
                    }
                }
                previousGraphsEquationsSize=graphsEquations.size();
                if(gayMode)
                {
                    for(size_t i{6}; i<ImGuiCol_COUNT; i++)
                    {
                        if(i==ImGuiCol_MenuBarBg) continue;
                        ImVec4 col{0,0,0,1};
                        ImGui::ColorConvertRGBtoHSV(style.Colors[i].x,style.Colors[i].y,style.Colors[i].z, col.x, col.y, col.z);
                        col.x=col.x+gayModeSpeed;
                        if(col.x>1) col.x=0;
                        ImGui::ColorConvertHSVtoRGB( col.x, col.y, col.z,style.Colors[i].x,style.Colors[i].y,style.Colors[i].z);
                    }  
                }

                if(instances.size()>1)
                {
                    ImGui::Separator();
                    if(selectedInstance!=0)
                    {
                        ImGui::Text("You are using instance %s.",instances.at(selectedInstance).name.c_str());
                    }
                    else ImGui::Text("You are using the main instance.");
                }
                ImGui::EndMenuBar();
            }

            {            
                size_t i{};
                for(std::pair<std::string,std::string> var : instances.at(selectedInstance).variables)
                {
                    if(instances.at(selectedInstance).sliders.find(var.first)==instances.at(selectedInstance).sliders.end()) break;
                    if(instances.at(selectedInstance).sliders.at(var.first).animate)
                    {
                        instances.at(selectedInstance).sliders.at(var.first).nums[0]+=(instances.at(selectedInstance).sliders.at(var.first).nums[2]-instances.at(selectedInstance).sliders.at(var.first).nums[1])/500;
                        timeStationary=0;
                        lessetB::globals::tokenMemory.clear();
                        
                        if(instances.at(selectedInstance).sliders.at(var.first).nums[2]>instances.at(selectedInstance).sliders.at(var.first).nums[1])
                        {
                            if(instances.at(selectedInstance).sliders.at(var.first).nums[0]>instances.at(selectedInstance).sliders.at(var.first).nums[2]) instances.at(selectedInstance).sliders.at(var.first).nums[0]=instances.at(selectedInstance).sliders.at(var.first).nums[1];
                            if(instances.at(selectedInstance).sliders.at(var.first).nums[0]<instances.at(selectedInstance).sliders.at(var.first).nums[1]) instances.at(selectedInstance).sliders.at(var.first).nums[0]=instances.at(selectedInstance).sliders.at(var.first).nums[1];
                        }
                        else
                        {
                            if(instances.at(selectedInstance).sliders.at(var.first).nums[0]<instances.at(selectedInstance).sliders.at(var.first).nums[2]) instances.at(selectedInstance).sliders.at(var.first).nums[0]=instances.at(selectedInstance).sliders.at(var.first).nums[1];
                            if(instances.at(selectedInstance).sliders.at(var.first).nums[0]>instances.at(selectedInstance).sliders.at(var.first).nums[1]) instances.at(selectedInstance).sliders.at(var.first).nums[0]=instances.at(selectedInstance).sliders.at(var.first).nums[2];
                        }
                        lessetB::addVariable(std::pair<std::string, std::string>(var.first,std::to_string(instances.at(selectedInstance).sliders.at(var.first).nums[0])), instances.at(selectedInstance).variables);
                        recalculateGraphs=true;
                    }
                    i++;
                }
            }

            style.FontScaleDpi = main_scale*2;
            ImGui::PushFont(io.FontDefault, 42.0f/2);      
            ImGui::SetNextItemWidth(io.DisplaySize.x/3);
            ImGui::InputText(" ",&equation);
            ImGui::PopFont();
            style.FontScaleDpi = main_scale;

            int unclosedParentheses = addClosingParentheses(equation);
            if(unclosedParentheses<0) ImGui::SetItemTooltip("Found extra closing parentheses.");
            
            if(equation!="") nonEmptyEquation=equation;
            std::string resultPlusEquals;
            if(equation!="") lastNonEmptyEquation=equation;
            else resultPlusEquals="";
            

            if(result.find('\n')==std::string::npos) resultPlusEquals="  =  " + result;
            else resultPlusEquals="  =  ";

            if(equation=="fps")
            {
                resultPlusEquals = "  =  " + std::to_string(static_cast<int>(1.0/io.DeltaTime));
            }

            if(equation=="fpsavg")
            {
                resultPlusEquals = "  =  " + std::to_string(static_cast<int>(io.Framerate+.5));
            }

            else if(equation=="how to exit vim" || equation=="how do i exit vim")
            {
                resultPlusEquals = "  =  :q";
            }

            else if(equation=="trivia")
            {
                showTrivia=true;
                resultPlusEquals = "  =  Read my ramblings! (help menu)";
            }

            else if(equation=="gayMode")
            {
                resultPlusEquals = "  =  check style options";
                showGayMode=true;
            }

            else if(equation.find("fishthumbs")!=std::string::npos)
            {
                resultPlusEquals = "  =  tacky could never";
                showGayMode=true;
            }

            else if(equation=="nine plus ten")
            {
                resultPlusEquals = "  =  twenty one";
            }

            else if(equation=="enableDebug")
            {
                resultPlusEquals = "  =  Enabled debug console output";
                lessetB::globals::debugCout=true;
            }

            else if(equation=="disableDebug")
            {
                resultPlusEquals = "  =  Disabled debug console output";
                lessetB::globals::debugCout=false;
            }

            bool textTooWide{};
            if(ImGui::CalcTextSize(resultPlusEquals.c_str()).x>io.DisplaySize.x/2)
            {
                textTooWide=true;
                ImGui::PushFont(io.FontDefault, 42.0f/(ImGui::CalcTextSize(resultPlusEquals.c_str()).x/io.DisplaySize.x*4));
            }
            
            ImGui::SameLine(io.DisplaySize.x/3+11);
            if(ImGui::Button(resultPlusEquals.c_str(),ImVec2(0,48)))
            {
                resultWindow=true;
                result="";
                if(equation!="")
                {
                    lastCalculationPrecision=lessetB::globals::decimalPrecision;
                    lessetB::evaluateEquation(options,false,false,nonEmptyEquation,resultHistory,result,instances.at(selectedInstance).variables, instances.at(selectedInstance).functions);
                    options.ans=result;
                }
            } 
            if(textTooWide) ImGui::PopFont();

            if(result.find('\n')!=std::string::npos)
            {
                std::string label;
                if(lessetB::globals::error) label="Errors";
                else label="Result";

                if(resultWindow)
                {
                    if(ImGui::Begin(label.c_str(),&resultWindow,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoDocking))
                    {
                        if(label=="Result")
                        {
                            if(showGraphingMenuHint) ImGui::MenuItem("For graphing, use the Graph menu!",NULL,false,false);
                            hasShownHint=true;
                        }
                        std::string line;
                        // ImGui::Text("%s",resultHistory.c_str());
                        
                        for(size_t i{}; i<result.length();)
                        {
                            line=result.substr(i,result.find('\n',i)-i)+"##"+std::to_string(i);
                            if(ImGui::Button(line.c_str()))
                            {
                                ImGui::SetClipboardText(result.substr(i,result.find('\n',i)-i).c_str());
                            }
                            ImGui::SetItemTooltip("Click to copy.");
                            i+=line.length()-1-std::to_string(i).length();
                        }
                        

                    }
                    else if(hasShownHint) showGraphingMenuHint=false;
                    ImGui::End();
                }
            }
            // if(graphsEquations.size()<MANY_GRAPHS && instances.size()==1) ImGui::Text("");




            const ImVec2 graphsPlotSize{io.DisplaySize.x-17,io.DisplaySize.y-95};

            if(ImPlot::BeginPlot("Graphs",graphsPlotSize, ImPlotFlags_Equal)) 
            {
                // ImPlot::BustColorCache();
                // ImPlot::PushColormap(colorMap);
                ImPlot::SetupLegend(ImPlotLocation_NorthWest,ImPlotLegendFlags_Horizontal);
                ImPlotRect prevLimits {limits.X.Min, limits.X.Max, limits.Y.Min, limits.Y.Max};
                limits = ImPlot::GetPlotLimits();

                ImPlot::PlotLine("",defaultBoundsX,defaultBoundsY,2,specHidden);

                bool skipRestOfFrame{};
                if(graphsEquations.size()!=0)
                {
                    float precisionDivisor=glfwGetVideoMode(glfwGetPrimaryMonitor())->refreshRate/io.Framerate; // Lower precision to improve framerate for expensive graphs.
                    if(precisionDivisor<1) precisionDivisor=1;
                    if(precisionDivisor>1.25)precisionDivisor+=precisionDivisor;
                    size_t j{};
                    pastPrecisionDivisors[j]=precisionDivisor;
                    j++;
                    if(j>=100) j=0;
                    float averagedPrecisionDivisor{};
                    size_t precisionDivisorsCollected{};
                    for(size_t i{}; i<100 && pastPrecisionDivisors[i]!=0; i++)
                    {
                        precisionDivisorsCollected++;
                        averagedPrecisionDivisor+=pastPrecisionDivisors[i];
                    }
                    averagedPrecisionDivisor/=precisionDivisorsCollected;
                    averagedPrecisionDivisor=round(averagedPrecisionDivisor);
                    if(averagedPrecisionDivisor>20) averagedPrecisionDivisor=20;
                    if(averagedPrecisionDivisor<4) averagedPrecisionDivisor=4;
                    averagedPrecisionDivisor*=graphsEquations.size()/4.f+1;
                    averagedPrecisionDivisor*=maxIndividualGraphPointsMultiplier/4.0;

                    double limitsRatio {abs((limits.X.Max-limits.X.Min)/(limits.Y.Max-limits.Y.Min))};

                    if(!(prevLimits.X.Min == limits.X.Min && prevLimits.X.Max == limits.X.Max))
                    {
                        timeStationary=0;
                    }
                    else timeStationary++;

                    const bool zoomedIn{abs(limits.X.Max-limits.X.Min)<abs(prevLimits.X.Max-prevLimits.X.Min)-0.000000001f};
                    const bool zoomedOut{abs(limits.X.Max-limits.X.Min)-0.000000001f>abs(prevLimits.X.Max-prevLimits.X.Min)};
                    size_t downsizeGraphIndex{static_cast<size_t>(-1)};
                    for(size_t j{}; j<graphsPoints.first.size(); j++)
                    {
                        if(graphsPoints.first.at(j).size()>32000/graphsEquations.size()*maxIndividualGraphPointsMultiplier && 
                        graphsPoints.first.at(j).size()>2000/graphsEquations.size()/2*maxIndividualGraphPointsMultiplier)
                        {
                            downsizeGraphIndex=j;
                            break;
                        }
                    }

                    double minimumPrecision{(graphsEquations.size()/2.f+1)};
                    // Recalculate a graph when too many points have been saved for it... or recalculate the graph of recalculateGraphIndex. Only one graph per frame being recalculated is intentional.
                    if(
                        (
                            downsizeGraphIndex!=static_cast<size_t>(-1) && 
                            graphsEquations.size()>downsizeGraphIndex && 
                            downsizeGraphIndex<graphsPoints.first.size() && 
                            graphsPoints.first.size()<MANY_GRAPHS && 
                            timeStationary<HIGH_PRECISION_DRAW_DELAY
                        ) || recalculateGraphIndex!=static_cast<size_t>(-1)
                    )
                    {
                        if(recalculateGraphIndex!=static_cast<size_t>(-1)) downsizeGraphIndex=recalculateGraphIndex;
                        if(lessetB::globals::debugCout)
                        {
                            std::cout<<"Recalculate single graph "<< graphsEquations.at(downsizeGraphIndex).first << '\n';
                            lessetB::globals::debugCoutUsed=true;
                        }
                        nonEmptyGraphEquation=graphsEquations.at(downsizeGraphIndex).first;
                        lessetB::Options graphOptions{true,
                                                    limits.X.Min,
                                                    limits.X.Max,
                                                    abs(limits.X.Max-limits.X.Min)/(maxIndividualGraphPoints/averagedPrecisionDivisor),
                                                    (aroundTruthinessLeniencyFloat),
                                                    interpolateDiscontinuities,
                                                    prioritizeImplicitMultiplication,
                                                    graphsEquations.at(downsizeGraphIndex).second.first};
                        lessetB::evaluateEquation(graphOptions,true,false,nonEmptyGraphEquation,nothing,nothing,instances.at(selectedInstance).variables, instances.at(selectedInstance).functions);

                        graphsPoints.first.at(downsizeGraphIndex)=lessetB::globals::points.first;
                        graphsPoints.second.at(downsizeGraphIndex)=lessetB::globals::points.second;       
                    }
                    // Prepend/Append to graph
                    else if(timeStationary==0 && graphsPoints.first.size()<MANY_GRAPHS && !zoomedIn) 
                    {
                        const double dXMin{limits.X.Min-prevLimits.X.Min};
                        const double dXMinScreenProportion{abs(dXMin)/(limits.X.Max-limits.X.Min)};

                        const double dXMax{limits.X.Max-prevLimits.X.Max};
                        const double dXMaxScreenProportion{abs(dXMax)/(limits.X.Max-limits.X.Min)};
                        if(dXMin<0)
                            for(size_t j{}; j<graphsEquations.size() && graphsPoints.first.at(j).at(0)>limits.X.Min; j++)
                            {
                                if(lessetB::globals::debugCout)
                                {
                                    std::cout<<"Prepending to graph "<< graphsEquations.at(j).first << '\n';
                                    lessetB::globals::debugCoutUsed=true;
                                }
                                nonEmptyGraphEquation=graphsEquations.at(j).first;

                                lessetB::Options graphOptions{true,
                                                            limits.X.Min,
                                                            prevLimits.X.Min,
                                                            abs(dXMin)/(maxIndividualGraphPoints*dXMinScreenProportion/averagedPrecisionDivisor),
                                                            (aroundTruthinessLeniencyFloat),
                                                            interpolateDiscontinuities,
                                                            prioritizeImplicitMultiplication,
                                                            graphsEquations.at(j).second.first};
                                lessetB::evaluateEquation(graphOptions,true,false,nonEmptyGraphEquation,nothing,nothing,instances.at(selectedInstance).variables, instances.at(selectedInstance).functions);

                                graphsPoints.first.at(j).insert_range(graphsPoints.first.at(j).begin(),lessetB::globals::points.first);
                                graphsPoints.second.at(j).insert_range(graphsPoints.second.at(j).begin(),lessetB::globals::points.second);
                            }
                        if(dXMax>0)
                            for(size_t j{}; j<graphsEquations.size() && graphsPoints.first.at(j).at(graphsPoints.first.at(j).size()-1)<limits.X.Max; j++)
                            {
                                if(lessetB::globals::debugCout)
                                {
                                    std::cout<<"Appending to graph "<< graphsEquations.at(j).first << '\n';
                                    lessetB::globals::debugCoutUsed=true;
                                }
                                nonEmptyGraphEquation=graphsEquations.at(j).first;

                                lessetB::Options graphOptions{true,
                                                            prevLimits.X.Max,
                                                            limits.X.Max,
                                                            abs(dXMax)/(maxIndividualGraphPoints/averagedPrecisionDivisor*dXMaxScreenProportion),
                                                            (aroundTruthinessLeniencyFloat),
                                                            interpolateDiscontinuities,
                                                            prioritizeImplicitMultiplication,
                                                            graphsEquations.at(j).second.first};
                                lessetB::evaluateEquation(graphOptions,true,false,nonEmptyGraphEquation,nothing,nothing,instances.at(selectedInstance).variables, instances.at(selectedInstance).functions);

                                graphsPoints.first.at(j).append_range(lessetB::globals::points.first);
                                graphsPoints.second.at(j).append_range(lessetB::globals::points.second);
                            }

                    }
                    // Recalculate when zooming in
                    else if(timeStationary==0 && graphsPoints.first.size()<MANY_GRAPHS && zoomedIn)
                    {
                        if(lessetB::globals::debugCout)
                        {
                            std::cout<<"Recalculating from zooming in\n";
                            lessetB::globals::debugCoutUsed=true;
                        }
                        graphsPoints.first.clear();
                        graphsPoints.second.clear();
                        for(size_t j{}; 
                            j<graphsEquations.size() && 
                            !(prevLimits.X.Min == limits.X.Min && prevLimits.X.Max == limits.X.Max); 
                            j++)
                        {
                            nonEmptyGraphEquation=graphsEquations.at(j).first;

                            lessetB::Options graphOptions{true,
                                                        limits.X.Min,
                                                        limits.X.Max,
                                                        abs(limits.X.Max-limits.X.Min)/(maxIndividualGraphPoints/averagedPrecisionDivisor),
                                                        (aroundTruthinessLeniencyFloat),
                                                        interpolateDiscontinuities,
                                                        prioritizeImplicitMultiplication,
                                                        graphsEquations.at(j).second.first};
                            lessetB::evaluateEquation(graphOptions,true,false,nonEmptyGraphEquation,nothing,nothing,instances.at(selectedInstance).variables, instances.at(selectedInstance).functions);

                            graphsPoints.first.emplace_back(lessetB::globals::points.first);
                            graphsPoints.second.emplace_back(lessetB::globals::points.second);
                            graphsPoints.first.at(graphsPoints.first.size()-1).reserve(maxIndividualGraphPoints);
                            graphsPoints.second.at(graphsPoints.second.size()-1).reserve(maxIndividualGraphPoints); 
                        }
                    }
                    // Recalculate at a high precision
                    if(timeStationary==HIGH_PRECISION_DRAW_DELAY || recalculateGraphs)
                    {
                        if(lessetB::globals::debugCout)
                        {
                            std::cout<<"Recalculating at high precision\n";
                            lessetB::globals::debugCoutUsed=true;
                        }
                        recalculateGraphs=false;
                        if(graphsEquations.size()>=MANY_GRAPHS && drawMany_Graphs || graphsEquations.size()<MANY_GRAPHS)
                        {
                            graphsPoints.first.clear();
                            graphsPoints.second.clear();
                            for(size_t j{}; j<graphsEquations.size(); j++)
                            {
                                nonEmptyGraphEquation=graphsEquations.at(j).first;
                                lessetB::Options graphOptions{true,
                                                              limits.X.Min,
                                                              limits.X.Max,
                                                              abs(limits.X.Max-limits.X.Min)/maxIndividualGraphPoints*minimumPrecision,
                                                              (aroundTruthinessLeniencyFloat),
                                                              interpolateDiscontinuities,
                                                              prioritizeImplicitMultiplication,
                                                              graphsEquations.at(j).second.first};
                                lessetB::evaluateEquation(graphOptions,true,false,nonEmptyGraphEquation,nothing,nothing,instances.at(selectedInstance).variables, instances.at(selectedInstance).functions);

                                graphsPoints.first.emplace_back(lessetB::globals::points.first);
                                graphsPoints.second.emplace_back(lessetB::globals::points.second);
                            }
                        }
                    } 
                    bool hasShownMousePointText{};
                    for(size_t j{}; j<graphsPoints.first.size(); j++) // Disconnect discontinuities and hand points to ImPlot
                    {

                        if(graphsEquations.size()>=MANY_GRAPHS && timeStationary<HIGH_PRECISION_DRAW_DELAY || graphsEquations.size()>=MANY_GRAPHS && !drawMany_Graphs) break;
                        
                        if(graphsEquations.size()<MANY_GRAPHS && !interpolateDiscontinuities)
                        {
                            for(size_t i{}; i<graphsPoints.first.at(j).size(); i++)
                            {
                                if(i<graphsPoints.first.at(j).size()-1)
                                {
                                    if(graphsPoints.first.at(j).at(i)>graphsPoints.first.at(j).at(i+1))
                                    {
                                        graphsPoints.second.at(j).erase(graphsPoints.second.at(j).begin()+i);
                                        graphsPoints.first.at(j).erase(graphsPoints.first.at(j).begin()+i);
                                    }
                                }
                                if(i>0 && i<graphsPoints.first.at(j).size()-2)
                                {
                                    const float previousDifference = (graphsPoints.second.at(j).at(i)-graphsPoints.second.at(j).at(i-1)) / (graphsPoints.first.at(j).at(i)-graphsPoints.first.at(j).at(i-1));
                                    const float difference = (graphsPoints.second.at(j).at(i+1)-graphsPoints.second.at(j).at(i)) / (graphsPoints.first.at(j).at(i+1)-graphsPoints.first.at(j).at(i));
                                    const float nextDifference = (graphsPoints.second.at(j).at(i+2)-graphsPoints.second.at(j).at(i+1)) / (graphsPoints.first.at(j).at(i+2)-graphsPoints.first.at(j).at(i+1));

                                    // Since this is merely an approximation of the derivative, it'll go crazy at a discontinuity, which is why the following conditions work.
                                    if(!interpolateDiscontinuities && 
                                        (
                                            difference!=0 && previousDifference==0 ||
                                            // get rid of the next line if able
                                            abs(difference)>abs(previousDifference)*MAX_CHANGE_FACTOR_SECOND && abs(difference)>abs(nextDifference)*MAX_CHANGE_FACTOR_SECOND || 
                                            (   
                                                (difference>previousDifference*MAX_CHANGE_FACTOR_FIRST && difference>=0 && previousDifference>=0) ||  
                                                (difference<previousDifference*MAX_CHANGE_FACTOR_FIRST && difference<=0 && previousDifference<=0) ||  
                                                (difference>-previousDifference*MAX_CHANGE_FACTOR_FIRST && difference<=0 && previousDifference>=0) || 
                                                (-difference>previousDifference*MAX_CHANGE_FACTOR_FIRST && difference>=0 && previousDifference<=0)    
                                            )
                                        ) && abs(difference)/abs(limits.Y.Max-limits.Y.Min)>0.03 // Don't see differences near 0 as discontinuities
                                      )
                                    {
                                        if(!isNoisy(graphsPoints.first.at(j),graphsPoints.second.at(j),i,maxIndividualGraphPointsMultiplier))
                                        {
                                            // Basically prevent ImPlot from interpolating between points
                                            if(graphsEquations.size()>20)
                                            {
                                                graphsPoints.second.at(j).at(i)=NAN;
                                            }
                                            else
                                            {
                                                
                                                graphsPoints.second.at(j).insert(graphsPoints.second.at(j).cbegin()+i,NAN);
                                                graphsPoints.first.at(j).insert(graphsPoints.first.at(j).cbegin()+i,NAN);
                                                i++;
                                                graphsPoints.second.at(j).insert(graphsPoints.second.at(j).cbegin()+i+1,NAN);
                                                graphsPoints.first.at(j).insert(graphsPoints.first.at(j).cbegin()+i+1,NAN);
                                                i++;
                                            }
                                        }
                                    }
                                }                    
                            }
                        }

                        ImPlotSpec spec{};
                        spec.Flags=ImPlotItemFlags_NoFit;
                        spec.LineWeight=2.f;

                        if(timeStationary<HIGH_PRECISION_DRAW_DELAY) ImPlot::PlotLine(graphsEquations.at(j).first.c_str(), &(*graphsPoints.first.at(j).cbegin()), &(*graphsPoints.second.at(j).cbegin()), graphsPoints.second.at(j).size(),spec);
                        else if((drawMany_Graphs && graphsEquations.size()>=MANY_GRAPHS) || graphsEquations.size()<MANY_GRAPHS) ImPlot::PlotLine(graphsEquations.at(j).first.c_str(), &(*graphsPoints.first.at(j).cbegin()), &(*graphsPoints.second.at(j).cbegin()), graphsPoints.second.at(j).size(),spec);
                        
                        bool textAbove{};
                        bool hasShownPoint{false};
                        double xPreviousPointMarked{-INFINITY};
                        size_t increment = 3;
                        
                        if(timeStationary>=100) increment=1;
                        if(markSpecialPoints && graphsEquations.size()<25 && !IsPlotHidden() && ImPlot::IsPlotHovered())
                            for(size_t i{increment}; i<graphsPoints.first.at(j).size()-20; i+=increment)
                            {
                                // Mouse point
                                if(abs(ImPlot::GetPlotMousePos().x-graphsPoints.first.at(j).at(i)) < abs(ImPlot::GetPlotMousePos().x-graphsPoints.first.at(j).at(i+increment)) &&
                                abs(ImPlot::GetPlotMousePos().x-graphsPoints.first.at(j).at(i)) < abs(ImPlot::GetPlotMousePos().x-graphsPoints.first.at(j).at(i-increment)) &&
                                !hasShownPoint) 
                                {
                                    hasShownPoint=true;
                                    ImPlotSpec spec{};
                                    spec.Flags=ImPlotItemFlags_NoFit;
                                    spec.MarkerLineColor=ImVec4{0,0,0,1};
                                    spec.MarkerFillColor=ImVec4(1,1,1,1);
                                    if(abs(ImPlot::GetPlotMousePos().y-graphsPoints.second.at(j).at(i))<(limits.Y.Max-limits.Y.Min)/5)
                                    {
                                        ImPlot::PlotScatter("##", &graphsPoints.second.at(j).at(i), 1, 0,graphsPoints.first.at(j).at(i),spec);
                                        if(abs(ImPlot::GetPlotMousePos().y-graphsPoints.second.at(j).at(i))<(limits.Y.Max-limits.Y.Min)/20 && !hasShownMousePointText)
                                        {
                                            hasShownMousePointText=true;
                                            std::string coordsFormatted= graphsEquations.at(j).first+"\n(" +  std::to_string(graphsPoints.first.at(j).at(i))+ "; " + std::to_string(graphsPoints.second.at(j).at(i)) + ")";
                                            ImPlot::PlotText(coordsFormatted.c_str(),graphsPoints.first.at(j).at(i),graphsPoints.second.at(j).at(i),ImVec2(0,30));
                                        }
                                    }
                                }

                                // Extremes and zeroes
                                if(((graphsPoints.first.at(j).at(i)<0 && graphsPoints.first.at(j).at(i+increment)>0) ||
                                    (graphsPoints.second.at(j).at(i)<graphsPoints.second.at(j).at(i+increment) && graphsPoints.second.at(j).at(i)<graphsPoints.second.at(j).at(i-increment)) ||
                                    (graphsPoints.second.at(j).at(i)>graphsPoints.second.at(j).at(i+increment) && graphsPoints.second.at(j).at(i)>graphsPoints.second.at(j).at(i-increment))) &&
                                    (abs(graphsPoints.first.at(j).at(i)-xPreviousPointMarked)>abs(limits.X.Max-limits.X.Min)/50)
                                  )
                                {
                                    bool hasNAN{};
                                    for(size_t k{i}; k<i+increment; k++)
                                    {
                                        if(graphsPoints.second.at(j).at(k)!=graphsPoints.second.at(j).at(k))
                                        {
                                            hasNAN=true;
                                            break;
                                        }
                                    }
                                    if(!hasNAN)
                                    {
                                        xPreviousPointMarked=graphsPoints.first.at(j).at(i);
                                        ImPlotSpec spec{};
                                        spec.Flags=ImPlotItemFlags_NoFit;
                                        spec.MarkerLineColor=ImVec4{0,0,0,1};
                                        spec.MarkerFillColor=ImVec4(1,1,1,1);
                                        ImPlot::PlotScatter("##", &graphsPoints.second.at(j).at(i), 1, 0,graphsPoints.first.at(j).at(i),spec);
                                        
                                        if(abs(ImPlot::GetPlotMousePos().x-graphsPoints.first.at(j).at(i))<(limits.X.Max-limits.X.Min)/10 &&
                                        abs(ImPlot::GetPlotMousePos().y-graphsPoints.second.at(j).at(i))<(limits.Y.Max-limits.Y.Min)/10)
                                        {
                                            if(textAbove) textAbove=false;
                                            else textAbove=true;
                                            std::string coordsFormatted = std::to_string(graphsPoints.first.at(j).at(i)).substr(0,std::to_string(graphsPoints.first.at(j).at(i)).size()-TRIMMEDDECIMALPLACES)+ '\n' + std::to_string(graphsPoints.second.at(j).at(i)).substr(0,std::to_string(graphsPoints.first.at(j).at(i)).size()-TRIMMEDDECIMALPLACES);

                                            ImPlot::PlotText(coordsFormatted.c_str(),graphsPoints.first.at(j).at(i),graphsPoints.second.at(j).at(i),ImVec2(0,60-textAbove*120));
                                        }
                                    }
                                }
                                else if(((graphsPoints.second.at(j).at(i)>0 && graphsPoints.second.at(j).at(i+increment)<0) ||
                                        (graphsPoints.second.at(j).at(i)<0 && graphsPoints.second.at(j).at(i+increment)>0)) &&
                                        (abs(graphsPoints.first.at(j).at(i)-xPreviousPointMarked)>abs(limits.X.Max-limits.X.Min)/50))
                                {
                                    bool hasNAN{};
                                    for(size_t k{i}; k<i+increment; k++)
                                    {
                                        if(graphsPoints.second.at(j).at(k)!=graphsPoints.second.at(j).at(k))
                                        {
                                            hasNAN=true;
                                            break;
                                        }
                                    }
                                    if(!hasNAN)
                                    {
                                        xPreviousPointMarked=graphsPoints.first.at(j).at(i);
                                        ImPlotSpec spec{};
                                        spec.Flags=ImPlotItemFlags_NoFit;
                                        spec.MarkerLineColor=ImVec4{0,0,0,1};
                                        spec.MarkerFillColor=ImVec4(1,1,1,1);
                                        const float zero=0;
                                        ImPlot::PlotScatter("##", &zero, 1, 0,graphsPoints.first.at(j).at(i),spec);
                                        
                                        if(abs(ImPlot::GetPlotMousePos().x-graphsPoints.first.at(j).at(i))<(limits.X.Max-limits.X.Min)/10 &&
                                        abs(ImPlot::GetPlotMousePos().y-graphsPoints.second.at(j).at(i))<(limits.Y.Max-limits.Y.Min)/10)
                                        {
                                            if(textAbove) textAbove=false;
                                            else textAbove=true;
                                            std::string coordsFormatted = std::to_string(graphsPoints.first.at(j).at(i)).substr(0,std::to_string(graphsPoints.first.at(j).at(i)).size()-TRIMMEDDECIMALPLACES)+ "\n0";
                                            ImPlot::PlotText(coordsFormatted.c_str(),graphsPoints.first.at(j).at(i),0,ImVec2(0,60-textAbove*120));
                                        }             
                                    }                           
                                }

                        }
                    
                    
                    }
                }
                
                
                ImPlot::EndPlot();
                // ImGui::Text("%f",io.Framerate);
            }
            ImGui::End();
        }
        if(lessetB::globals::debugCoutUsed) std::cout<<'\n';

        // Rendering
        ImGui::Render();        ImDrawData* main_draw_data = ImGui::GetDrawData();        const bool main_is_minimized = (main_draw_data->DisplaySize.x <= 0.0f || main_draw_data->DisplaySize.y <= 0.0f);        wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;        wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;        wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;        wd->ClearValue.color.float32[3] = clear_color.w;        if (!main_is_minimized)FrameRender(wd, main_draw_data);
        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable && false)        {            ImGui::UpdatePlatformWindows();            ImGui::RenderPlatformWindowsDefault();        }
        // Present Main Platform Window
        if (!main_is_minimized) FramePresent(wd);
        if(!main_is_minimized && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Q)) break;
    }
    // Cleanup
    std::cout<<"\nQuitting...\n";
    err = vkDeviceWaitIdle(g_Device);    check_vk_result(err);    ImGui_ImplVulkan_Shutdown();    ImGui_ImplGlfw_Shutdown();    ImPlot::DestroyContext();    ImGui::DestroyContext();CleanupVulkanWindow(&g_MainWindowData);CleanupVulkan();glfwDestroyWindow(window);glfwTerminate();

    return 0;
}

bool isNoisy(const std::vector<double> &pointsX, const std::vector<double> &pointsY, size_t i, int maxIndividualGraphPointsMultiplier)
{
    int switches{};
    bool rising{};
    bool prevRising{};
    int j=i-10;
    if(j<0) return false;
    {
        for(; j<i+10 && j<pointsX.size()-1; j++)
        {
            if(pointsX.at(j)==0) return true;
            // if(j>0 && pointsX.at(j)-pointsX.at(j+1) != pointsX.at(j-1)-pointsX.at(j)) return true;
            if(pointsY.at(j)<pointsY.at(j+1))
            {
                rising=true;
                if(prevRising!=rising) switches++;
                if(j==i-10) switches--;
            }
            else if(pointsY.at(j)>pointsY.at(j+1))
            {
                rising=false;
                if(prevRising!=rising) switches++;
            }
            prevRising=rising;
        } 
    }

    if(switches<6) return false;

    return true; 
}

int addClosingParentheses(std::string &equation)
{                
    int unclosedParentheses{};
    for(size_t i{}; i<equation.length(); i++)
    {
        if(equation.at(i)=='(') unclosedParentheses++;
        else if(equation.at(i)==')') unclosedParentheses--;
    }
    if(unclosedParentheses>0)
    {
        while(unclosedParentheses>0)
        {
            equation.push_back(')');
            unclosedParentheses--;
        }
    }
    return unclosedParentheses;
}

bool IsPlotHidden()
{
    ImPlotContext &imPlotContext = *GImPlot;
    if(imPlotContext.PreviousItem)
    {
        return !imPlotContext.PreviousItem->Show;
    }
    else return false;
}
