#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

//#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstdio>
#include <optional>
#include <set>
#include <limits>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <array>
#include "ansi.h"

#include <vulkan/vk_enum_string_helper.h>

#include "worldgenerator.h"
#include "fontrenderer.h"

static bool platformIsLittleEndian() {
    unsigned int t = 1;
    return (reinterpret_cast<char*>(&t))[0] == 1;
}

struct Camera {
    alignas(16) glm::vec3 position = glm::vec3(0, 0, 10.0);
    alignas(16) glm::vec3 forward = glm::vec3(0, 1, 0);
    alignas(16) glm::vec3 up = glm::vec3(0, 0, 1);
    alignas(16) glm::vec3 right = glm::vec3(1, 0, 0);
    alignas(4) float cur_time = 0.0f;
    alignas(16) glm::vec3 sunDirection = glm::vec3(-1, -1, -1);
};

static VkVertexInputBindingDescription getVertexBindingDescription() {
    VkVertexInputBindingDescription bindingDescription {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

static std::array<VkVertexInputAttributeDescription, 2> getVertexAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions {};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, texCoord);

    return attributeDescriptions;
}

/* Vertex arrays for framebuffer quad */
const std::vector<Vertex> vertices = {
    {{-1.0f, -1.0f}, {0.0f, 0.0f}},
    {{1.0f, -1.0f}, {1.0f, 0.0f}},
    {{1.0f, 1.0f}, {1.0f, 1.0f}},
    {{-1.0f, 1.0f}, {0.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0
};

const uint32_t WIDTH = 1920;
const uint32_t HEIGHT = 1080;
const uint32_t RENDER_SCALE = 2;

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct DistancesPushConstants {
    alignas(16) glm::ivec3 curVoxelOffset;
};

const int MAX_FRAMES_IN_FLIGHT = 2;
uint32_t currentFrame = 0;
uint64_t frameCounter = 0;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
    VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME
};
#define NDEBUG
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif // NDEBUG

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                      const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                      const VkAllocationCallbacks* pAllocator,
                                      VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

class Game {
public:
    void run() {
        littleEndian = platformIsLittleEndian();
        console.setGameInstance(this);
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    /** Console class **/
    static constexpr int MAX_LINE = 256;
    static constexpr int MAX_OUTPUT_LINES = 16;

    class Console
    {
    public:
        Console() {
            clearInput();
            clearOutput();
        }
        ~Console() {}

        void setGameInstance(Game* g) {
            instance = g;
        }

        void enterCharacter(char c) {
            if (commandCursor < (MAX_LINE - 2)) {
                commandBuf[commandCursor] = c;
                commandCursor++;
                _latestTextRendered = false;
            }
        }

        void deleteCharacter() {
            if (commandCursor > 1) {
                commandCursor--;
                commandBuf[commandCursor] = '\0';
                _latestTextRendered = false;
            }
        }

        void runCommand() {
            if (hasInput()) {
                if (strcmp(commandBuf + 1, "help") == 0) {
                    snprintf(scratch, sizeof(scratch),  "Valid commands:\n"
                                                        "%s: shows this message.\n"
                                                        "%s: print a message.\n"
                                                        "%s: quit the game.\n"
                                                        "%s: get current position\n"
                                                        "%s: set position",
                                                        "help", "echo <message>", "exit/quit", "getpos", "setpos x,y,z");
                    strcpy(output, scratch);
                } else if (strncmp(commandBuf + 1, "echo ", 5) == 0) {
                    strcpy(output, commandBuf + 6);
                } else if (strcmp(commandBuf + 1, "exit") == 0 || strcmp(commandBuf + 1, "quit") == 0) {
                    instance->shouldQuit = true;
                } else if (strcmp(commandBuf + 1, "getpos") == 0) {
                    snprintf(scratch, sizeof(scratch), "(%f, %f, %f)", instance->camera.position.x, instance->camera.position.y, instance->camera.position.z);
                    strcpy(output, scratch);
                } else if (strncmp(commandBuf + 1, "setpos ", 6) == 0) {
                    float readPositions[3] = {0.0f, 0.0f, 0.0f};
                    int matched = sscanf(commandBuf + 8, "%f,%f,%f", &readPositions[0], &readPositions[1], &readPositions[2]);
                    if (matched == 3) {
                        instance->camera.position = glm::vec3(readPositions[0], readPositions[1], readPositions[2]);
                    } else {
                        strcpy(output, "Invalid position.");
                    }
                } else {
                    strcpy(output, "Invalid command.");
                }

                clearInput();
                _latestTextRendered = false;
            }
        }

        void toggle() { enabled = !enabled; }
        bool isEnabled() { return enabled; }
        bool hasOutput() { return output[0] != '\0'; }
        bool hasInput() { return commandBuf[1] != '\0'; }

        const char* getCommandBuf() { return commandBuf; }
        const char* getOutputBuf() { return output; }

        bool latestTextRendered() { return _latestTextRendered; }
        void setRenderedText() { _latestTextRendered = true; }

    private:
        void clearOutput() {
            output[0] = '\0';
        }

        void clearInput() {
            memset(commandBuf, '\0', MAX_LINE);
            commandBuf[0] = '>';
            commandCursor = 1;
        }

        bool enabled = false;
        bool _latestTextRendered = false;
        char commandBuf[MAX_LINE];
        int commandCursor = 0;
        char output[MAX_OUTPUT_LINES * MAX_LINE];
        char scratch[MAX_OUTPUT_LINES * MAX_LINE];
        Game* instance;
    };

    /** Variables **/
    bool littleEndian;

    GLFWwindow* window;

    VkInstance instance;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;

    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkQueue computeQueue;

    VkSurfaceKHR surface;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;

    VkRenderPass renderPass;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkCommandBuffer> computeCommandBuffers;
    std::vector<VkCommandBuffer> computeDistancesCommandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> computeFinishedSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> computeFinishedFences;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    std::vector<VkImage> renderImages;
    std::vector<VkDeviceMemory> renderImagesMemory;
    std::vector<VkImageView> renderImageViews;

    VkSampler textureSampler;

    /* Font rendering */
    FontRenderer fontRenderer;

    FontMesh renderedFont;
    FontMesh renderedFpsCounter;

    VkImage fontImage;
    VkDeviceMemory fontImageMemory;
    VkImageView fontImageView;
    VkSampler fontSampler;

    VkBuffer textBuffer;
    VkDeviceMemory textBufferMemory;
    VkBuffer textIndexBuffer;
    VkDeviceMemory textIndexBufferMemory;

    VkBuffer fpsCounterBuffer;
    VkDeviceMemory fpsCounterMemory;
    VkBuffer fpsCounterIndexBuffer;
    VkDeviceMemory fpsCounterIndexMemory;

    std::vector<VkDescriptorSet> fontDescriptorSets;

    /* Console */
    Console console;

    /* Compute pipeline */
    VkDescriptorSetLayout computeDescriptorSetLayout;
    VkDescriptorPool computeDescriptorPool;
    std::vector<VkDescriptorSet> computeDescriptorSets;
    VkPipeline computePipeline;
    VkPipelineLayout computePipelineLayout;
    /*
    std::vector<VkBuffer> computeUniformBuffers;
    std::vector<VkDeviceMemory> computeUniformsMemory;
    std::vector<void*> computeUniformsMapped;
    */

    std::vector<VkBuffer> voxelBuffers;
    std::vector<VkDeviceMemory> voxelBuffersMemory;

    /* Compute - calculating distance field */
    VkDescriptorSetLayout computeDistancesSetLayout;
    VkDescriptorPool computeDistancesPool;
    std::vector<VkDescriptorSet> computeDistancesDescriptorSets;
    VkPipeline computeDistancesPipeline;
    VkPipelineLayout computeDistancesPipelineLayout;

    /* Debug messenger */
    VkDebugUtilsMessengerEXT debugMessenger;

    /* Voxels */
    LoadedChunks* chunks = nullptr;

    /* Camera / player */
    Camera camera;
    glm::ivec2 lastUpdatePlayerChunk;

    bool keyPressed[GLFW_KEY_LAST+1];
    double cursorX;
    double cursorY;
    double cursorLastX;
    double cursorLastY;
    double cameraRotY = 0;
    double cameraRotZ = 0;
    static constexpr double minPitch = -1.5;
    static constexpr double maxPitch = 1.5;
    static constexpr double sensitivity = 2.0;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastFrameStart;

    bool framebufferResized = false;
    bool shouldQuit = false;

    double lastFrameFps = 0.0;
    bool fpsCounterEnabled = false;

    FontMesh getMeshForFpsCounter(double fps) {
        FontMesh result;
        char scratch[64];
        snprintf(scratch, 64, "%.0f @ %dx%d", fps, swapChainExtent.width / RENDER_SCALE, swapChainExtent.height / RENDER_SCALE);
        scratch[63] = '\0';

        glm::vec2 cursor(-1.0, -1.0);
        /* Title text */
        const char* titleText = "voxel renderer";
        double labelWidth = fontRenderer.getGlyphWidthScreen() * strlen(titleText) + CONSOLE_MARGIN * 2.0;
        double labelHeight = fontRenderer.getGlyphHeightScreen() + CONSOLE_MARGIN * 2.0;
        fontRenderer.addMeshForBG(result, FONT_BG2_UV, cursor, {-1.0 + labelWidth, cursor.y + labelHeight});
        fontRenderer.addMeshForLabel(result, titleText, {cursor.x + CONSOLE_MARGIN, cursor.y + CONSOLE_MARGIN});
        cursor.y += labelHeight;

        /* FPS counter */
        labelWidth = fontRenderer.getGlyphWidthScreen() * strlen(scratch) + CONSOLE_MARGIN * 2.0;
        fontRenderer.addMeshForBG(result, FONT_BG2_UV, cursor, {-1.0 + labelWidth, cursor.y + labelHeight});
        fontRenderer.addMeshForLabel(result, scratch, {cursor.x + CONSOLE_MARGIN, cursor.y + CONSOLE_MARGIN});
        cursor.y += labelHeight;

        /* Position display */
        snprintf(scratch, 64, "(%.2f, %.2f, %.2f)", camera.position.x, camera.position.y, camera.position.z);
        labelWidth = fontRenderer.getGlyphWidthScreen() * strlen(scratch) + CONSOLE_MARGIN * 2.0;
        fontRenderer.addMeshForBG(result, FONT_BG2_UV, cursor, {-1.0 + labelWidth, cursor.y + labelHeight});
        fontRenderer.addMeshForLabel(result, scratch, {cursor.x + CONSOLE_MARGIN, cursor.y + CONSOLE_MARGIN});
        cursor.y += labelHeight;

        /* Chunk display */
        snprintf(scratch, 64, "Chunk: (%d, %d)", lastUpdatePlayerChunk.x, lastUpdatePlayerChunk.y);
        labelWidth = fontRenderer.getGlyphWidthScreen() * strlen(scratch) + CONSOLE_MARGIN * 2.0;
        fontRenderer.addMeshForBG(result, FONT_BG2_UV, cursor, {-1.0 + labelWidth, cursor.y + labelHeight});
        fontRenderer.addMeshForLabel(result, scratch, {cursor.x + CONSOLE_MARGIN, cursor.y + CONSOLE_MARGIN});

        return result;
    }

    FontMesh getMeshForConsole() {
        FontMesh result;

        /* Command buffer */
        glm::vec2 cursor(-1.0 + CONSOLE_MARGIN, -1.0 + (fontRenderer.getGlyphHeightScreen() + 2.0 * CONSOLE_MARGIN) * 4.0);
        fontRenderer.addMeshForBG(result, FONT_BG2_UV, {-1.0, cursor.y + fontRenderer.getGlyphHeightScreen()},
                                                       {1.0, cursor.y});
        fontRenderer.addMeshForLabel(result, console.getCommandBuf(), cursor);
        cursor.y += fontRenderer.getGlyphHeightScreen();

        /* Output buffer */
        if (console.hasOutput()) {
            int numLines = 1;
            int outputLength = strlen(console.getOutputBuf());
            for (int i = 0; i < outputLength; i++) {
                if (console.getOutputBuf()[i] == '\n') {
                    numLines++;
                }
            }
            double outputHeight = fontRenderer.getGlyphHeightScreen() * double(numLines);
            fontRenderer.addMeshForBG(result, FONT_BG2_UV, {-1.0, cursor.y},
                                                          {1.0, cursor.y + outputHeight});
            fontRenderer.addMeshForLabel(result, console.getOutputBuf(), cursor);
        }

        return result;
    }

    /*
    UTF-32 text input
    Just used for ASCII input to console
    */
    static void charCallback(GLFWwindow* window, unsigned int codepoint) {
        Game* app = reinterpret_cast<Game*>(glfwGetWindowUserPointer(window));
        if (app->console.isEnabled()) {
            char c;
            if (app->littleEndian) {
                c = (reinterpret_cast<char *>(&codepoint))[0];
            } else {
                c = (reinterpret_cast<char *>(&codepoint))[3];
            }
            if (isPrintableAscii(c) && c != '`') {
                app->console.enterCharacter(c);
            }
        }
    }

    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        Game* app = reinterpret_cast<Game*>(glfwGetWindowUserPointer(window));
        if (key <= GLFW_KEY_LAST && key >= 0) {
            switch(action) {
            case GLFW_PRESS:
                if (key == GLFW_KEY_GRAVE_ACCENT) {
                    app->console.toggle();
                } else if (!app->console.isEnabled() && (key == GLFW_KEY_Q)) {
                    app->shouldQuit = true;
                } else if (app->console.isEnabled()) {
                    if (key == GLFW_KEY_ENTER) {
                        app->console.runCommand();
                    } else if (key == GLFW_KEY_BACKSPACE) {
                        app->console.deleteCharacter();
                    }
                } else if (key == GLFW_KEY_F1) {
                    app->fpsCounterEnabled = !app->fpsCounterEnabled;
                }
                app->keyPressed[key] = true;
                break;
            case GLFW_RELEASE:
                app->keyPressed[key] = false;
                break;
            case GLFW_REPEAT:
            default:
                break;
            }
        }
    }

    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
        Game* app = reinterpret_cast<Game*>(glfwGetWindowUserPointer(window));

        app->cursorLastX = app->cursorX;
        app->cursorLastY = app->cursorY;
        app->cursorX = xpos;
        app->cursorY = ypos;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void *pUserData) {

        std::stringstream ss;

        switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            ss << "[Verbose] " << ANSI::escape(pCallbackData->pMessage, FAINT, FG_DEFAULT);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            ss << "[Info] " << ANSI::escape(pCallbackData->pMessage, FAINT, FG_DEFAULT);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            ss << "[Warning] " << ANSI::escape(pCallbackData->pMessage, FG_DEFAULT, FG_YELLOW);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            ss << ANSI::escape("[ERROR] ", BOLD, FG_RED) << ANSI::escape(pCallbackData->pMessage, BOLD, FG_RED);
            break;
        default:
            break;
        }
        std::cout << ss.str() << std::endl;

        return VK_FALSE;
    }

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(WIDTH, HEIGHT, "renderer", glfwGetPrimaryMonitor(), nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
        for (int i = 0; i < GLFW_KEY_LAST; i++) {
            keyPressed[i] = false;
        }
        glfwSetCharCallback(window, charCallback);
        glfwSetKeyCallback(window, keyCallback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (glfwRawMouseMotionSupported())
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        glfwGetCursorPos(window, &cursorX, &cursorY);
        cursorLastX = cursorX;
        cursorLastY = cursorY;
        //glfwSetCursorPosCallback(window, cursorPosCallback);
    }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<Game*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createTextureSampler();
        loadFont();
        createFontImageView();
        createFontSampler();
        createVertexBuffer();
        createIndexBuffer();
        createTextBuffers();
        createFpsCounterBuffers();
        createUniformBuffers();
        createVoxelBuffers();
        createRenderImages();
        createRenderImageViews();
        createDescriptorPool();
        createDescriptorSets();
        createFontDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
        createComputeDescriptorSetLayout();
        createComputePipeline();
        createComputeDescriptorPool();
        createComputeDescriptorSets();
        // Compute distances
        createComputeDistancesLayout();
        createComputeDistancesPipeline();
        createComputeDistancesPool();
        createComputeDistancesDescriptorSets();
        // Compute actual distances...
        //computeVoxelDistances();
    }

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
        createInfo.pUserData = nullptr;
    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo {};
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    void computeVoxelDistances() {
        VkFence computeDistanceFence;

        VkFenceCreateInfo computeFenceInfo {};
        computeFenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        computeFenceInfo.flags = 0;

        if (vkCreateFence(device, &computeFenceInfo, nullptr, &computeDistanceFence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute distance fences!");
        }

        const int workgroupSizeX = 8;
        const int workgroupSizeY = 8;
        const int workgroupSizeZ = 16;

        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(computeDistancesCommandBuffers[0], &beginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to begin recording compute command buffer!");
        }

        vkCmdBindPipeline(computeDistancesCommandBuffers[0], VK_PIPELINE_BIND_POINT_COMPUTE, computeDistancesPipeline);
        vkCmdBindDescriptorSets(computeDistancesCommandBuffers[0], VK_PIPELINE_BIND_POINT_COMPUTE, computeDistancesPipelineLayout,
                                0, 1, &computeDistancesDescriptorSets[0], 0, nullptr);

        vkCmdDispatch(computeDistancesCommandBuffers[0], CHUNK_WIDTH_VOXELS / workgroupSizeX, CHUNK_WIDTH_VOXELS / workgroupSizeY, CHUNK_HEIGHT_VOXELS / workgroupSizeZ);

        if (vkEndCommandBuffer(computeDistancesCommandBuffers[0]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to record compute distances command buffer!");
        }

        VkSubmitInfo computeDistancesSubmitInfo {};
        computeDistancesSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        computeDistancesSubmitInfo.waitSemaphoreCount = 0;
        computeDistancesSubmitInfo.pWaitSemaphores = nullptr;
        computeDistancesSubmitInfo.pWaitDstStageMask = nullptr;

        computeDistancesSubmitInfo.commandBufferCount = 1;
        computeDistancesSubmitInfo.pCommandBuffers = &computeDistancesCommandBuffers[0];

        computeDistancesSubmitInfo.signalSemaphoreCount = 0;
        computeDistancesSubmitInfo.pSignalSemaphores = nullptr;

        VkResult result;
        if ((result = vkQueueSubmit(computeQueue, 1, &computeDistancesSubmitInfo, computeDistanceFence)) != VK_SUCCESS)
        {
            std::cerr << string_VkResult(result) << std::endl;
            throw std::runtime_error("Failed to submit compute distances command buffer");
        }
        if ((result = vkWaitForFences(device, 1, &computeDistanceFence, VK_TRUE, UINT64_MAX)) != VK_SUCCESS)
        {
            std::cerr << "Error waiting for fence: " << string_VkResult(result) << std::endl;
            throw std::runtime_error("Failed to wait for compute distances fence!");
        }

        vkDestroyFence(device, computeDistanceFence, nullptr);

        for (int i = 1; i < MAX_FRAMES_IN_FLIGHT; i++) {
            copyBuffer(voxelBuffers[0], voxelBuffers[i], sizeof(LoadedChunks));
        }
    }

    void createComputeDistancesLayout() {
        std::array<VkDescriptorSetLayoutBinding, 1> layoutBindings {};
        layoutBindings[0].binding = 0;
        layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        layoutBindings[0].descriptorCount = 1;
        layoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        layoutBindings[0].pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
        layoutInfo.pBindings = layoutBindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &computeDistancesSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute distance descriptor set layout!");
        }
    }

    void createComputeDistancesPipeline() {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &computeDistancesSetLayout;

        // Push constant
        /*
        VkPushConstantRange pushConstantRange {};
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(DistancesPushConstants);
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        */

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &computeDistancesPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute distances pipeline layout!");
        }

        auto computeDistancesShaderCode = readFile("shaders/compute_distances.spv");

        VkShaderModule computeDistancesShaderModule = createShaderModule(computeDistancesShaderCode);

        VkPipelineShaderStageCreateInfo computeDistancesShaderStageInfo {};
        computeDistancesShaderStageInfo.sType =  VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeDistancesShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeDistancesShaderStageInfo.module = computeDistancesShaderModule;
        computeDistancesShaderStageInfo.pName = "main";

        VkComputePipelineCreateInfo pipelineInfo {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = computeDistancesPipelineLayout;
        pipelineInfo.stage = computeDistancesShaderStageInfo;

        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                     &computeDistancesPipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute distances pipeline!");
        }

        vkDestroyShaderModule(device, computeDistancesShaderModule, nullptr);
    }

    void createComputeDistancesPool() {
        std::array<VkDescriptorPoolSize, 1> poolSizes {};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        poolInfo.flags = 0;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &computeDistancesPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute distances descriptor pool!");
        }
    }

    void createComputeDistancesDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, computeDistancesSetLayout);
        VkDescriptorSetAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = computeDistancesPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        computeDistancesDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        VkResult result = vkAllocateDescriptorSets(device, &allocInfo, computeDistancesDescriptorSets.data());

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate compute distances descriptor sets!");
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufferInfo {};
            bufferInfo.buffer = voxelBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = VK_WHOLE_SIZE;

            std::array<VkWriteDescriptorSet, 1> descriptorWrites {};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = computeDistancesDescriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()),
                                   descriptorWrites.data(), 0, nullptr);
        }
    }

    /*
    void createComputeUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(Camera);

        computeUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        computeUniformsMemory.resize(MAX_FRAMES_IN_FLIGHT);
        computeUniformsMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         computeUniformBuffers[i], computeUniformsMemory[i]);
            vkMapMemory(device, computeUniformsMemory[i], 0, bufferSize, 0, &computeUniformsMapped[i]);
        }
    }
    */

    void createComputeDescriptorPool() {
        std::array<VkDescriptorPoolSize, 2> poolSizes {};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        poolInfo.flags = 0;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &computeDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute descriptor pool!");
        }
    }

    void createComputeDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, computeDescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = computeDescriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        computeDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        VkResult result = vkAllocateDescriptorSets(device, &allocInfo, computeDescriptorSets.data());

        if (result != VK_SUCCESS) {
            std::cout << string_VkResult(result) << std::endl;
            throw std::runtime_error("failed to allocate compute descriptor sets!");
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorImageInfo outputImageInfo {};
            outputImageInfo.sampler = textureSampler;
            outputImageInfo.imageView = renderImageViews[i];
            outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkDescriptorBufferInfo voxelBufferInfo {};
            voxelBufferInfo.buffer = voxelBuffers[i];
            voxelBufferInfo.offset = 0;
            voxelBufferInfo.range = VK_WHOLE_SIZE;

            std::array<VkWriteDescriptorSet, 2> descriptorWrites {};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = computeDescriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo = &outputImageInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = computeDescriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pBufferInfo = &voxelBufferInfo;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()),
                                   descriptorWrites.data(), 0, nullptr);
        }
    }

    void updateComputeDescriptorSets() {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorImageInfo outputImageInfo {};
            outputImageInfo.sampler = textureSampler;
            outputImageInfo.imageView = renderImageViews[i];
            outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkWriteDescriptorSet descriptorWrite {};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = computeDescriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &outputImageInfo;

            vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
        }
    }

    void updateDescriptorSets() {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorImageInfo inputImageInfo {};
            inputImageInfo.sampler = textureSampler;
            inputImageInfo.imageView = renderImageViews[i];
            inputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkWriteDescriptorSet descriptorWrite {};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSets[i];
            descriptorWrite.dstBinding = 1;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &inputImageInfo;

            vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
        }
    }

    void createComputeDescriptorSetLayout() {
        std::array<VkDescriptorSetLayoutBinding, 2> layoutBindings {};

        layoutBindings[0].binding = 0;
        layoutBindings[0].descriptorCount = 1;
        layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        layoutBindings[0].pImmutableSamplers = nullptr;
        layoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        layoutBindings[1].binding = 1;
        layoutBindings[1].descriptorCount = 1;
        layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        layoutBindings[1].pImmutableSamplers = nullptr;
        layoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
        layoutInfo.pBindings = layoutBindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute descriptor set layout!");
        }
    }

    void createComputePipeline() {
        /* Pipeline layout */
        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &computeDescriptorSetLayout;

        // Push constant
        VkPushConstantRange pushConstantRange {};
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(Camera);
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &computePipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute pipeline layout!");
        }

        /* Shader stage */
        auto computeShaderCode = readFile("shaders/compute.spv");

        VkShaderModule computeShaderModule = createShaderModule(computeShaderCode);

        VkPipelineShaderStageCreateInfo computeShaderStageInfo {};
        computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeShaderStageInfo.module = computeShaderModule;
        computeShaderStageInfo.pName = "main";

        /* Pipeline */
        VkComputePipelineCreateInfo pipelineInfo {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = computePipelineLayout;
        pipelineInfo.stage = computeShaderStageInfo;

        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                     &computePipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute pipeline!");
        }

        vkDestroyShaderModule(device, computeShaderModule, nullptr);
    }

    void createTextureSampler() {
        VkSamplerCreateInfo samplerInfo {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;

        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;

        VkPhysicalDeviceProperties properties {};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);

        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    void createFontSampler() {
        VkSamplerCreateInfo samplerInfo {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;

        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;

        VkPhysicalDeviceProperties properties {};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);

        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &fontSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferImageCopy region {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {
            width,
            height,
            1
        };

        vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        endSingleTimeCommands(commandBuffer);
    }

    void copyBufferToBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize sz) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferCopy bufferCopy {};
        bufferCopy.srcOffset = 0;
        bufferCopy.dstOffset = 0;
        bufferCopy.size = sz;

        vkCmdCopyBuffer(commandBuffer, src, dst, 1, &bufferCopy);
        endSingleTimeCommands(commandBuffer);
    }

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
                               VkImageLayout newLayout) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;

        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        /*
        // Which types of operations involving this resource must happen before this
        // barrier?
        barrier.srcAccessMask = 0;
        // Which types of operations must wait on this barrier?
        barrier.dstAccessMask = 0;
        */
        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            // No operations we need to wait on
            barrier.srcAccessMask = 0;
            // We want to transfer to the image
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                   newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {

            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT; // (a pseudo-stage)
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        } else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(commandBuffer,
                             sourceStage,
                             destinationStage,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);

        endSingleTimeCommands(commandBuffer);
    }

    VkCommandBuffer beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                     VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image,
                     VkDeviceMemory& imageMemory) {
        // Create the image
        VkImageCreateInfo imageInfo {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;

        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags = 0;

        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        // Create the backing device memory
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                    properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        vkBindImageMemory(device, image, imageMemory, 0);
    }
    /*
    void createTextureImage() {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load("textures/texture.png", &texWidth, &texHeight,
                                    &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(imageSize, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
            memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);

        stbi_image_free(pixels);

        createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth),
                          static_cast<uint32_t>(texHeight));
        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }
    */

    /*
    Load a given file to a VkImage and VkDeviceMemory
    calls stbi_load with given desired_channels
    */
    void loadVkImage(const char* path, int desired_channels, VkImage& image, VkDeviceMemory& memory) {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight,
                                    &texChannels, desired_channels);
        VkDeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(imageSize, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
            memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);

        stbi_image_free(pixels);

        createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory);

        transitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer, image, static_cast<uint32_t>(texWidth),
                          static_cast<uint32_t>(texHeight));
        transitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void loadVkImage(stbi_uc* pixels, int texWidth, int texHeight, VkImage& image, VkDeviceMemory& memory) {
        VkDeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels) {
            throw std::runtime_error("NULL texture image!");
        }

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(imageSize, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
            memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);

        createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory);

        transitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer, image, static_cast<uint32_t>(texWidth),
                          static_cast<uint32_t>(texHeight));
        transitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void loadFont() {
        int texWidth, texHeight, texChannels;
        int desired_channels = STBI_rgb_alpha;
        stbi_uc* pixels = stbi_load("textures/simple_8x9.png", &texWidth, &texHeight,
                                    &texChannels, desired_channels);

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        loadVkImage(pixels, texWidth, texHeight, fontImage, fontImageMemory);
        stbi_image_free(pixels);
    }

    VkImageView createImageView(VkImage image, VkFormat format) {
        VkImageViewCreateInfo createInfo {};
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
        if (vkCreateImageView(device, &createInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }

        return imageView;
    }

    void createFontImageView() {
        fontImageView = createImageView(fontImage, VK_FORMAT_R8G8B8A8_SRGB);
    }

    void createRenderImages() {
        renderImages.resize(MAX_FRAMES_IN_FLIGHT);
        renderImagesMemory.resize(MAX_FRAMES_IN_FLIGHT);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            createImage(swapChainExtent.width / RENDER_SCALE, swapChainExtent.height / RENDER_SCALE, VK_FORMAT_R8G8B8A8_UNORM,
                        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, renderImages[i], renderImagesMemory[i]);
            transitionImageLayout(renderImages[i], VK_FORMAT_R8G8B8A8_UNORM,
                                  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        }
    }

    void createRenderImageViews() {
        renderImageViews.resize(MAX_FRAMES_IN_FLIGHT);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            renderImageViews[i] = createImageView(renderImages[i], VK_FORMAT_R8G8B8A8_UNORM);
        }
    }

    void createVoxelBuffers() {
        lastUpdatePlayerChunk = glm::ivec2(int(camera.position.x) / CHUNK_WIDTH_VOXELS,
                                           int(camera.position.y) / CHUNK_WIDTH_VOXELS);
        size_t voxelBufferSize = sizeof(LoadedChunks);
        std::cout << "Creating a voxel buffer of size " << voxelBufferSize << std::endl;
        chunks = new LoadedChunks;
        std::cout << "Generating chunks: 0 / " << TOTAL_CHUNKS_LOADED;
        std::cout.flush();
        for (int x = 0; x < LOADED_CHUNKS_AXIS; x++) {
            for (int y = 0; y < LOADED_CHUNKS_AXIS; y++) {
                VoxelChunk v(chunks, x, y);
                WorldGenerator::generateChunk(&v, lastUpdatePlayerChunk.x + x - DRAW_DISTANCE, lastUpdatePlayerChunk.y + y - DRAW_DISTANCE);
                std::cout << "\rGenerating chunks: " << x * LOADED_CHUNKS_AXIS + y + 1 << " / " << TOTAL_CHUNKS_LOADED;
                std::cout.flush();
            }
        }
        std::cout << std::endl;

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(voxelBufferSize, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, voxelBufferSize, 0, &data);
            memcpy(data, &chunks->voxels, voxelBufferSize);
        vkUnmapMemory(device, stagingBufferMemory);
        //delete chunks;

        voxelBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        voxelBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            createBuffer(voxelBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, voxelBuffers[i],
                                    voxelBuffersMemory[i]);
            copyBuffer(stagingBuffer, voxelBuffers[i], voxelBufferSize);
        }

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void updateChunks() {
        glm::ivec2 playerChunk = glm::ivec2(int(camera.position.x) / CHUNK_WIDTH_METERS,
                                             int(camera.position.y) / CHUNK_WIDTH_METERS);
        if (playerChunk.x != lastUpdatePlayerChunk.x ||
            playerChunk.y != lastUpdatePlayerChunk.y) {
            loadNewChunks(playerChunk.x - lastUpdatePlayerChunk.x, playerChunk.y - lastUpdatePlayerChunk.y);
        }
    }

    /*
    Load the new chunks into memory when the player moves.
    directionX = -1 if player moved in the negative x direction
                  0 if player did not change x coord
                  1 if player moved in positive x direction
                  etc.

    if either directionX or directionY is not one of these values,
    reload all chunks
    */
    void loadNewChunks(int directionX, int directionY) {
        std::cout << "Player moved in the " << directionX << ", " << directionY << " direction." << std::endl;
        lastUpdatePlayerChunk = glm::ivec2(int(camera.position.x) / CHUNK_WIDTH_METERS,
                                           int(camera.position.y) / CHUNK_WIDTH_METERS);
        glm::ivec2 minChunk(lastUpdatePlayerChunk.x - DRAW_DISTANCE,
                            lastUpdatePlayerChunk.y - DRAW_DISTANCE);
        if (directionX == 0 && directionY == 1) {
            /* Issue CHUNK_WIDTH_VOXELS * LOADED_CHUNKS_AXIS buffer copies */
        } else if (directionX == 1 && directionY == 0) {

        } else if (directionX == 1 && directionY == 1) {

        } else if (directionX == 0 && directionY == -1) {

        } else if (directionX == -1 && directionY == 0) {

        } else if (directionX == -1 && directionY == -1) {

        } else {
            return;
        }
    }

    void createDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        VkResult result;
        if ((result = vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data())) != VK_SUCCESS) {
            std::cout << string_VkResult(result) << std::endl;
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufferInfo {};
            bufferInfo.buffer = uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            VkDescriptorImageInfo imageInfo {};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfo.imageView = renderImageViews[i];
            imageInfo.sampler = textureSampler;

            std::array<VkWriteDescriptorSet, 2> descriptorWrites {};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = descriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = descriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()),
                                    descriptorWrites.data(), 0, nullptr);
        }
    }

    void createFontDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        fontDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        VkResult result;
        if ((result = vkAllocateDescriptorSets(device, &allocInfo, fontDescriptorSets.data())) != VK_SUCCESS) {
            std::cout << string_VkResult(result) << std::endl;
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufferInfo {};
            bufferInfo.buffer = uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            VkDescriptorImageInfo imageInfo {};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = fontImageView;
            imageInfo.sampler = fontSampler;

            std::array<VkWriteDescriptorSet, 2> descriptorWrites {};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = fontDescriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = fontDescriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()),
                                    descriptorWrites.data(), 0, nullptr);
        }
    }

    void createDescriptorPool() {
        std::array<VkDescriptorPoolSize, 2> poolSizes {};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2;

        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2;
        poolInfo.flags = 0;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void createUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i],
                         uniformBuffersMemory[i]);
            vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0,
                        &uniformBuffersMapped[i]);
        }
    }

    void createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboLayoutBinding {};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding samplerLayoutBinding {};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};

        VkDescriptorSetLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                        &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties, VkBuffer& buffer,
                      VkDeviceMemory& bufferMemory) {

        // Create the buffer
        VkBufferCreateInfo bufferInfo {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vertex buffer!");
        }

        // Assign memory to it
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                                   properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate vertex buffer memory!");
        }

        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }

    // Type filter - bit field of suitable memory types
    // properties - bit field of required properties
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        // memoryHeaps - different memory resources (ex. dedicated VRAM, swap space
        // in RAM)
        // TODO this affects performance, but we don't consider it here

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    // Optimization - use a separate command pool for temporary command buffers
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferCopy copyRegion {};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    }

    void createVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        // Fill the buffer with data
        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, vertices.data(), (size_t) bufferSize);
        vkUnmapMemory(device, stagingBufferMemory);

        createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
        copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void createFpsCounterBuffers() {
        /* Font mesh */
        renderedFpsCounter = getMeshForFpsCounter(lastFrameFps);
        /* Vertex buffer */
        VkDeviceSize bufferSize = sizeof(renderedFpsCounter.vert[0]) * renderedFpsCounter.vert.size();

        VkBuffer vertexStagingBuffer;
        VkDeviceMemory vertexStagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexStagingBuffer, vertexStagingBufferMemory);

        // Fill the buffer with data
        void* data;
        vkMapMemory(device, vertexStagingBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, renderedFpsCounter.vert.data(), (size_t) bufferSize);
        vkUnmapMemory(device, vertexStagingBufferMemory);

        createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, fpsCounterBuffer, fpsCounterMemory);
        copyBuffer(vertexStagingBuffer, fpsCounterBuffer, bufferSize);

        vkDestroyBuffer(device, vertexStagingBuffer, nullptr);
        vkFreeMemory(device, vertexStagingBufferMemory, nullptr);
        /* Index buffer */
        VkDeviceSize indexBufferSize = sizeof(renderedFpsCounter.ind[0]) * renderedFpsCounter.ind.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        // Fill the buffer with data
        //void* data;
        vkMapMemory(device, stagingBufferMemory, 0, indexBufferSize, 0, &data);
            memcpy(data, renderedFpsCounter.ind.data(), (size_t) indexBufferSize);
        vkUnmapMemory(device, stagingBufferMemory);

        createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, fpsCounterIndexBuffer, fpsCounterIndexMemory);
        copyBuffer(stagingBuffer, fpsCounterIndexBuffer, indexBufferSize);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void recreateFpsBuffers() {
        vkDeviceWaitIdle(device);

        vkDestroyBuffer(device, fpsCounterBuffer, nullptr);
        vkFreeMemory(device, fpsCounterMemory, nullptr);

        vkDestroyBuffer(device, fpsCounterIndexBuffer, nullptr);
        vkFreeMemory(device, fpsCounterIndexMemory, nullptr);

        createFpsCounterBuffers();
    }

    void createTextBuffers() {
        /* Font mesh */
        renderedFont = getMeshForConsole();
        /* Vertex buffer */
        VkDeviceSize bufferSize = sizeof(renderedFont.vert[0]) * renderedFont.vert.size();

        VkBuffer vertexStagingBuffer;
        VkDeviceMemory vertexStagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexStagingBuffer, vertexStagingBufferMemory);

        // Fill the buffer with data
        void* data;
        vkMapMemory(device, vertexStagingBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, renderedFont.vert.data(), (size_t) bufferSize);
        vkUnmapMemory(device, vertexStagingBufferMemory);

        createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textBuffer, textBufferMemory);
        copyBuffer(vertexStagingBuffer, textBuffer, bufferSize);

        vkDestroyBuffer(device, vertexStagingBuffer, nullptr);
        vkFreeMemory(device, vertexStagingBufferMemory, nullptr);
        /* Index buffer */
        VkDeviceSize indexBufferSize = sizeof(renderedFont.ind[0]) * renderedFont.ind.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        // Fill the buffer with data
        //void* data;
        vkMapMemory(device, stagingBufferMemory, 0, indexBufferSize, 0, &data);
            memcpy(data, renderedFont.ind.data(), (size_t) indexBufferSize);
        vkUnmapMemory(device, stagingBufferMemory);

        createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textIndexBuffer, textIndexBufferMemory);
        copyBuffer(stagingBuffer, textIndexBuffer, indexBufferSize);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void recreateTextBuffers() {
        vkDeviceWaitIdle(device);

        vkDestroyBuffer(device, textBuffer, nullptr);
        vkFreeMemory(device, textBufferMemory, nullptr);

        vkDestroyBuffer(device, textIndexBuffer, nullptr);
        vkFreeMemory(device, textIndexBufferMemory, nullptr);

        createTextBuffers();
    }

    void createIndexBuffer() {
        VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        // Fill the buffer with data
        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, indices.data(), (size_t) bufferSize);
        vkUnmapMemory(device, stagingBufferMemory);

        createBuffer(bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
        copyBuffer(stagingBuffer, indexBuffer, bufferSize);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void cleanupSwapChain() {
        for (size_t i = 0; i < renderImageViews.size(); i++) {
            vkDestroyImageView(device, renderImageViews[i], nullptr);
        }
        for (size_t i = 0; i < renderImages.size(); i++) {
            vkDestroyImage(device, renderImages[i], nullptr);
        }
        for (size_t i = 0; i < renderImagesMemory.size(); i++) {
            vkFreeMemory(device, renderImagesMemory[i], nullptr);
        }
        for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
            vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
        }
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            vkDestroyImageView(device, swapChainImageViews[i], nullptr);
        }
        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }

    void recreateSwapChain() {
        /*
        Pause while window is minimized (size is 0)
        Probably not necessary?
        */
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        std::cout << "Recreating swapchain." << std::endl;
        VkResult result;
        if ((result = vkDeviceWaitIdle(device)) != VK_SUCCESS) {
            std::cerr << string_VkResult(result) << std::endl;
            throw std::runtime_error("vkDeviceWaitIdle failed!");
        }
        std::cout << "Device is idle." << std::endl;
        cleanupSwapChain();
        std::cout << "Finished swapchain cleanup" << std::endl;
        createSwapChain();
        createImageViews();
        createFramebuffers();
        createRenderImages();
        createRenderImageViews();
        updateDescriptorSets();
        updateComputeDescriptorSets();
    }

    void createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        computeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        computeFinishedFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkFenceCreateInfo computeFenceInfo {};
        computeFenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        computeFenceInfo.flags = 0;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &computeFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &computeFenceInfo, nullptr, &computeFinishedFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create sync objects!");
            }
        }
    }

    void recordComputeCommandBuffer(VkCommandBuffer commandBuffer, uint32_t renderImageIndex) {
        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording compute command buffer!");
        }

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout,
                                0, 1, &computeDescriptorSets[currentFrame], 0, nullptr);
        vkCmdPushConstants(commandBuffer, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(Camera), &camera);
        vkCmdDispatch(commandBuffer, ((swapChainExtent.width / RENDER_SCALE) + 31) / 32, ((swapChainExtent.height / RENDER_SCALE) + 31) / 32, 1);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        // Begin render pass
        VkRenderPassBeginInfo renderPassInfo {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];

        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;

        VkClearValue clearColor = {{{0.1f, 0.15f, 0.15f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

        VkViewport viewport {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapChainExtent.width);
        viewport.height = static_cast<float>(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor {};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // Bind descriptor set
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0,
                                nullptr);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

        /* Font rendering */
        if (console.isEnabled()) {
            VkBuffer textVertexBuffers[] = {textBuffer};
            VkDeviceSize textOffsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, textVertexBuffers, textOffsets);

            vkCmdBindIndexBuffer(commandBuffer, textIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayout, 0, 1, &fontDescriptorSets[currentFrame], 0,
                                    nullptr);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(renderedFont.ind.size()), 1, 0, 0, 0);
        }

        if (fpsCounterEnabled) {
            VkBuffer textVertexBuffers[] = {fpsCounterBuffer};
            VkDeviceSize textOffsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, textVertexBuffers, textOffsets);

            vkCmdBindIndexBuffer(commandBuffer, fpsCounterIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayout, 0, 1, &fontDescriptorSets[currentFrame], 0,
                                    nullptr);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(renderedFpsCounter.ind.size()), 1, 0, 0, 0);
        }

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void createCommandBuffers() {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        // Level:
        // - primary buffers can be submitted to a queue and can call
        // secondary command buffers
        // - secondary buffers can only be called from primary buffers
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }

        /* Compute command buffers */
        computeCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        allocInfo.commandBufferCount = (uint32_t) computeCommandBuffers.size();

        if (vkAllocateCommandBuffers(device, &allocInfo, computeCommandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate compute command buffers!");
        }

        /* Compute distances command buffers */
        computeDistancesCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        allocInfo.commandBufferCount = (uint32_t) computeDistancesCommandBuffers.size();

        if (vkAllocateCommandBuffers(device, &allocInfo, computeDistancesCommandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate compute distances command buffers!");
        }
    }

    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

        VkCommandPoolCreateInfo poolInfo {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsAndComputeFamily.value();

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool");
        }
    }

    void createFramebuffers() {
        swapChainFramebuffers.resize(swapChainImageViews.size());

        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            VkImageView attachments[] = {
                swapChainImageViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void createRenderPass() {
        VkAttachmentDescription colorAttachment {};
        colorAttachment.format = swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        // Load and store ops - what to do with the attachment after rendering
        // (color and depth data)
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        // (stencil data)
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // Subpasses
        VkAttachmentReference colorAttachmentRef {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        // Subpass dependency to make sure the image is ready when we use it
        VkSubpassDependency dependency {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;

        // Wait for the swap chan to finish reading from the image before
        // we access it for rendering
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;

        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // Render pass
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }
        return shaderModule;
    }

    void createGraphicsPipeline() {
        auto vertShaderCode = readFile("shaders/vert.spv");
        auto fragShaderCode = readFile("shaders/frag.spv");

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        // Assign shaders to pipeline stage
        VkPipelineShaderStageCreateInfo vertShaderStageInfo {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";
        // Optional - allows specifying values for constants
        // to configure shader at pipeline creation
        // The compiler can then perform optimizations with
        // those values known
        vertShaderStageInfo.pSpecializationInfo = nullptr;

        VkPipelineShaderStageCreateInfo fragShaderStageInfo {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        // Configure fixed function pipeline stages
        // Format of vertex data passed to vertex shader
        VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        auto bindingDescription = getVertexBindingDescription();
        auto attributeDescriptions = getVertexAttributeDescriptions();
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // Viewport and scissors
        /*
        VkViewport viewport {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor {};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        */

        // Dynamic state - viewport size and scissor state -
        // we will specify these at draw time
        // (no performance penalty)
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineViewportStateCreateInfo viewportState {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        // Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        // Depth clamp enable - fragments that fall outside of the near/far planes
        // are clamped to them instead of discarding them
        // (useful for shadow maps)
        rasterizer.depthClampEnable = VK_FALSE;
        // If true, geometry never passes through rasterizer stage
        // basically disables output to framebuffer?
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        // Culling mode
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        // Depth value adjustments
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

        // MSAA
        VkPipelineMultisampleStateCreateInfo multisampling {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional

        // Color blending
        // Per-framebuffer
        VkPipelineColorBlendAttachmentState colorBlendAttachment {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                              VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT |
                                              VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        // Should these be switched (alpha of framebuffer will be alpha of last written component?)
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        // Per-pipeline
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        // Pipeline layout (uniforms)
        VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        // Create the pipeline!
        VkGraphicsPipelineCreateInfo pipelineInfo {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;

        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;

        pipelineInfo.layout = pipelineLayout;

        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;

        // Base pipeline to derive/inherit from
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional

        // Create the pipeline
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                      &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

    void createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());

        for (size_t i = 0; i < swapChainImages.size(); i++) {
            swapChainImageViews[i] = createImageView(swapChainImages[i],
                                                     swapChainImageFormat);
        }
    }

    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 &&
            imageCount > swapChainSupport.capabilities.maxImageCount) {

            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {indices.graphicsAndComputeFamily.value(),
                                         indices.presentFamily.value()};
        // Set up image sharing if the graphics queue family happens
        // to be different from the presentation queue family
        if (indices.graphicsAndComputeFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        // Transformations on swap chain images
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        // Alpha for transparent windows
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        // We don't care about the value of pixels that are obscured by another window
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
        fontRenderer.updateResolution(swapChainExtent.width, swapChainExtent.height);
    }

    void createSurface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsAndComputeFamily.value(), indices.presentFamily.value()};

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures {};
        deviceFeatures.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        /* Enable shader 8 bit access */

        VkPhysicalDeviceVulkan12Features enable8BitStorage {};
        enable8BitStorage.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        enable8BitStorage.uniformAndStorageBuffer8BitAccess = VK_TRUE;
        enable8BitStorage.shaderInt8 = VK_TRUE;

        createInfo.pNext = &enable8BitStorage;

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device");
        }

        vkGetDeviceQueue(device, indices.graphicsAndComputeFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
        vkGetDeviceQueue(device, indices.graphicsAndComputeFamily.value(), 0, &computeQueue);
    }

    void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }

        VkPhysicalDeviceProperties usedDeviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &usedDeviceProperties);
        std::cout << "Using GPU: " << usedDeviceProperties.deviceName << std::endl;
        std::cout << "Vulkan version: " << usedDeviceProperties.apiVersion << std::endl;
    }

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsAndComputeFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsAndComputeFamily.has_value() && presentFamily.has_value();
        }
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                indices.graphicsAndComputeFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        return indices.isComplete() && extensionsSupported && swapChainAdequate &&
                supportedFeatures.samplerAnisotropy;
        /*
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures deviceFeatures;

        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
               deviceFeatures.geometryShader;
        */
    }

    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName: validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                std::cout << layerName << " : validation layer not found" << std::endl;
                return false;
            }
        }

        return true;
    }

    /* Swap chain */
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        VkSurfaceFormatKHR returned = availableFormats[0];
        for (const auto& availableFormat : availableFormats) {
            //std::cout << "format: " << availableFormat.format << std::endl;
            //std::cout << "color space: " << availableFormat.colorSpace << std::endl;
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    returned = availableFormat;
                }
        }

        return returned;
    }

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        // For certain displays (high DPI), the device coordinates passed to GLFW at
        // window creation are not the same as pixels, so make sure we choose the
        // right swap chain image size here
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };
            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                                            capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                                             capabilities.maxImageExtent.height);
            return actualExtent;
        }
    }

    void createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "renderer";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // Enable validation layers
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo {};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = nullptr;//(VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        VkResult result;
        if ((result = vkCreateInstance(&createInfo, nullptr, &instance)) != VK_SUCCESS) {
            std::cout << string_VkResult(result) << std::endl;
            throw std::runtime_error("failed to create instance!");
        }
    }

    void mainLoop() {
        lastFrameStart = std::chrono::high_resolution_clock::now();
        double timeSpentRendering = 0.0;
        while (!glfwWindowShouldClose(window) && !shouldQuit) {
            glfwPollEvents();
            /** Avoid putting syscalls here - they seem to break the timer **/
            const auto start = std::chrono::high_resolution_clock::now();
            drawFrame();
            updateChunks();
            const auto end_time = std::chrono::high_resolution_clock::now();
            const float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(end_time - start).count();
            lastFrameFps = 1 / frameTime;
        }
        std::cout << "Frames rendered: " << frameCounter << std::endl;
        std::cout << "Time taken: " << timeSpentRendering << std::endl;
        std::cout << "Avg FPS: " << 1 / (timeSpentRendering / double(frameCounter)) << std::endl;

        vkDeviceWaitIdle(device);
    }

    void drawFrame() {
        if (!console.latestTextRendered()) {
            recreateTextBuffers();
            console.setRenderedText();
        }
        if (fpsCounterEnabled) {
            recreateFpsBuffers();
        }

        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastFrameStart).count();
        camera.cur_time += deltaTime;
        lastFrameStart = std::chrono::high_resolution_clock::now();
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX,
                              imageAvailableSemaphores[currentFrame],
                              VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        if (vkResetFences(device, 1, &inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to reset fence!");
        }

        /* Update camera */
        cursorLastX = cursorX;
        cursorLastY = cursorY;
        glfwGetCursorPos(window, &cursorX, &cursorY);
        double cursorDeltaX = cursorX - cursorLastX;
        double cursorDeltaY = cursorY - cursorLastY;
        // Normalize for DPI ?
        cursorDeltaX /= double(swapChainExtent.width);
        cursorDeltaY /= double(swapChainExtent.height);
        cursorDeltaX *= sensitivity;
        cursorDeltaY *= sensitivity;

        /* Gravity */
        /*
        if (camera.position.z > 1.8f) {
            camera.position.z -= 9.8f * deltaTime;
        }
        if (camera.position.z < 1.8f) {
            camera.position.z = 1.8f;
        }
        */

        /* Camera rotation */
        if (!console.isEnabled()) {
            cameraRotY -= cursorDeltaY;
            cameraRotZ += cursorDeltaX;

            cameraRotY = glm::clamp(cameraRotY, minPitch, maxPitch);

            camera.forward = glm::normalize(glm::vec3(glm::cos(cameraRotZ) * glm::cos(cameraRotY), glm::sin(cameraRotZ) * glm::cos(cameraRotY), glm::sin(cameraRotY)));
            camera.up = glm::normalize(glm::vec3(glm::cos(cameraRotZ) * glm::cos(cameraRotY + glm::radians(90.0f)), glm::sin(cameraRotZ) * glm::cos(cameraRotY + glm::radians(90.0f)), glm::sin(cameraRotY + glm::radians(90.0f))));
            camera.right = -glm::cross(camera.forward, camera.up);

            //printf("Fwd: (%f, %f, %f)\n", camera.forward.x, camera.forward.y, camera.forward.z);
            //printf("Up: (%f, %f, %f)\n", camera.up.x, camera.up.y, camera.up.z);
            //printf("Right: (%f, %f, %f)\n", camera.right.x, camera.right.y, camera.right.z);

            /* Camera position */
            glm::vec3 moveDirection(0, 0, 0);
            float moveSpeed = 1.0;
            if (keyPressed[GLFW_KEY_W])
                moveDirection += camera.forward;
            if (keyPressed[GLFW_KEY_S])
                moveDirection -= camera.forward;
            if (keyPressed[GLFW_KEY_A])
                moveDirection -= camera.right;
            if (keyPressed[GLFW_KEY_D])
                moveDirection += camera.right;
            if (keyPressed[GLFW_KEY_LEFT_SHIFT])
                moveSpeed = 10.0f;
            //moveDirection.z = 0.0f;
            camera.position += moveDirection * deltaTime * moveSpeed;
            //std::cout << "Camera: " << camera.position.x << ", " << camera.position.y << ", " << camera.position.z << std::endl;
            //std::cout << "Rot Z: " << cameraRotZ << " Rot Y: " << cameraRotY << std::endl;
            //std::cout << "x: " << cursorDeltaX << " y: " << cursorDeltaY << std::endl;
        }
        camera.sunDirection = glm::normalize(glm::vec3(0.1 * glm::sin(camera.cur_time) + 1, 0.1 * glm::cos(camera.cur_time) + 1, 0.1 * glm::sin(camera.cur_time) - 1));

        /* Compute shader block */
        vkResetCommandBuffer(computeCommandBuffers[currentFrame], 0);
        recordComputeCommandBuffer(computeCommandBuffers[currentFrame], imageIndex);

        VkSubmitInfo computeSubmitInfo {};
        computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore computeWaitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags computeWaitStages[] = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
        computeSubmitInfo.waitSemaphoreCount = 1;
        computeSubmitInfo.pWaitSemaphores = computeWaitSemaphores;
        computeSubmitInfo.pWaitDstStageMask = computeWaitStages;

        computeSubmitInfo.commandBufferCount = 1;
        computeSubmitInfo.pCommandBuffers = &computeCommandBuffers[currentFrame];

        VkSemaphore computeSignalSemaphores[] = {computeFinishedSemaphores[currentFrame]};
        computeSubmitInfo.signalSemaphoreCount = 1;
        computeSubmitInfo.pSignalSemaphores = computeSignalSemaphores;

        if ((result = vkQueueSubmit(computeQueue, 1, &computeSubmitInfo, VK_NULL_HANDLE)) != VK_SUCCESS) {
            std::cerr << string_VkResult(result) << std::endl;
            throw std::runtime_error("failed to submit compute command buffer!");
        }

        /*
        vkWaitForFences(device, 1, &computeFinishedFences[currentFrame], VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &computeFinishedFences[currentFrame]);
        */

        /* Graphics pipeline block */
        // Update uniform buffer
        //updateUniformBuffer(currentFrame);

        // Record command buffer with the image we just got
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {computeFinishedSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        // Present the image
        VkPresentInfoKHR presentInfo {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        // Array of VkResults from presenting to the swapchain (optional)
        presentInfo.pResults = nullptr;
        result = vkQueuePresentKHR(presentQueue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }
        //std::cout << "End frame " << currentFrame << std::endl;
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo {};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                               glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float) swapChainExtent.height,
                                    0.1f, 10.0f);
        // Vulkan uses clip coordinates with the Y inverted compared to OpenGL
        // Fix by inverting Y scale in the projection matrix
        ubo.proj[1][1] *= -1;

        memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
        // Optimization - push constants
    }

    void cleanup() {
        cleanupSwapChain();

        /* Clean up compute distances pipeline */
        vkDestroyPipelineLayout(device, computeDistancesPipelineLayout, nullptr);
        vkDestroyPipeline(device, computeDistancesPipeline, nullptr);

        vkDestroyDescriptorPool(device, computeDistancesPool, nullptr);
        vkDestroyDescriptorSetLayout(device, computeDistancesSetLayout, nullptr);

        /* Clean up compute pipeline and related structures */
        /*
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyBuffer(device, computeUniformBuffers[i], nullptr);
            vkFreeMemory(device, computeUniformsMemory[i], nullptr);
        }
        */

        vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
        vkDestroyPipeline(device, computePipeline, nullptr);

        vkDestroyDescriptorPool(device, computeDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, computeDescriptorSetLayout, nullptr);

        vkDestroySampler(device, fontSampler, nullptr);
        vkDestroyImageView(device, fontImageView, nullptr);

        vkDestroyImage(device, fontImage, nullptr);
        vkFreeMemory(device, fontImageMemory, nullptr);

        vkDestroySampler(device, textureSampler, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyBuffer(device, uniformBuffers[i], nullptr);
            vkFreeMemory(device, uniformBuffersMemory[i], nullptr);

            vkDestroyBuffer(device, voxelBuffers[i], nullptr);
            vkFreeMemory(device, voxelBuffersMemory[i], nullptr);
        }

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);

        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        vkDestroyBuffer(device, fpsCounterBuffer, nullptr);
        vkFreeMemory(device, fpsCounterMemory, nullptr);

        vkDestroyBuffer(device, fpsCounterIndexBuffer, nullptr);
        vkFreeMemory(device, fpsCounterIndexMemory, nullptr);

        vkDestroyBuffer(device, textBuffer, nullptr);
        vkFreeMemory(device, textBufferMemory, nullptr);

        vkDestroyBuffer(device, textIndexBuffer, nullptr);
        vkFreeMemory(device, textIndexBufferMemory, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);

        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(device, computeFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
            vkDestroyFence(device, computeFinishedFences[i], nullptr);
        }

        vkDestroyCommandPool(device, commandPool, nullptr);

        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);

        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main() {
    std::cout << ANSI::escape("--------------------------------------------------------------------------------", BOLD, FG_DEFAULT) << std::endl;
    Game app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
