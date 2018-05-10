#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>
#include <vector>


const std::vector<const char*> gValidationLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

#ifdef NDEBUG
const bool bEnableValidationLayer = false;
#else 
const bool bEnableValidationLayer = true;
#endif


class HelloTriangleApp {
public:
	void Run() {
		InitWindow();
		InitVulkan();
		MainLoop();
		CleanUp();
	}

private:
	void InitVulkan() {
		CreateInstance();
	}

	void CreateInstance() {
		if (bEnableValidationLayer && !CheckValidationLayersSupport()) {
			throw new std::runtime_error("Validation layers requested, but not supported");
		}

		VkApplicationInfo AppInfo = {};
		AppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		AppInfo.pApplicationName = "Hello Triangle";
		AppInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		AppInfo.pEngineName = "Respawn Engine";
		AppInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		AppInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo CreateInfo = {};
		CreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		CreateInfo.pApplicationInfo = &AppInfo;
		
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		
		CreateInfo.enabledExtensionCount = glfwExtensionCount;
		CreateInfo.ppEnabledExtensionNames = glfwExtensions;

		if (bEnableValidationLayer) {
			CreateInfo.enabledLayerCount = static_cast<uint32_t>(gValidationLayers.size());
			CreateInfo.ppEnabledLayerNames = gValidationLayers.data();
		}
		else {
			CreateInfo.enabledLayerCount = 0;
		}

		if (vkCreateInstance(&CreateInfo, nullptr, &Instance) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create Instance");
		}

		EnumerateExtensions();
	}

	bool CheckValidationLayersSupport() {
		uint32_t LayerCount;
		vkEnumerateInstanceLayerProperties(&LayerCount, nullptr);

		std::vector<VkLayerProperties> AvailableLayers(LayerCount);
		vkEnumerateInstanceLayerProperties(&LayerCount, AvailableLayers.data());

		for (const char* LayerName : gValidationLayers) {
			bool bLayerFound = false;
			for (const auto& LayerProperty : AvailableLayers) {
				if (strcmp(LayerProperty.layerName, LayerName) == 0) {
					bLayerFound = true;
					break;
				}
			}

			if (!bLayerFound) {
				return false;
			}
		}
		return true;
	}

	void EnumerateExtensions() {
		uint32_t ExtensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, nullptr);

		std::vector<VkExtensionProperties> Extensions(ExtensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, Extensions.data());

		std::cout << "Available Extensions: " << std::endl;
		for (const auto& Extension : Extensions) {
			std::cout << "\t" << Extension.extensionName << std::endl;
		}
	}

	void InitWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		const int ScreenWidth = 800;
		const int ScreenHeight = 600;
		Window = glfwCreateWindow(ScreenWidth, ScreenHeight, "My Vulkan Window", nullptr, nullptr);

	}

	void MainLoop() {
		while (!glfwWindowShouldClose(Window)) {
			glfwPollEvents();
		}
	}

	void CleanUp() {
		vkDestroyInstance(Instance, nullptr);

		glfwDestroyWindow(Window);
		Window = nullptr;

		glfwTerminate();
	}

private:
	VkInstance Instance;
	GLFWwindow* Window;
};


int main() {
	HelloTriangleApp app;

	try {
		app.Run();
	}
	catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
