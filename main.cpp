#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>
#include <vector>
#include <map>


const std::vector<const char*> gValidationLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

#ifdef NDEBUG
const bool bEnableValidationLayer = false;
#else 
const bool bEnableValidationLayer = true;
#endif


VkResult CreateDebugResultCallbackEXT(VkInstance Instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocators, VkDebugReportCallbackEXT* pCallback) {
	auto Func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(Instance, "vkCreateDebugReportCallbackEXT");
	if (Func) {
		return Func(Instance, pCreateInfo, pAllocators, pCallback);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugReportCallbackEXT(VkInstance Instance, VkDebugReportCallbackEXT Callback, const VkAllocationCallbacks* pAllocator) {
	auto Func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(Instance, "vkDestroyDebugReportCallbackEXT");
	if (Func) {
		Func(Instance, Callback, pAllocator);
	}
}

struct QueueFamilyIndices {
	int GraphicsFamily = -1;
	bool IsComplete() {
		return GraphicsFamily >= 0;
	}
};

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
		SetupDebugCallback();
		PickPhysicalDevice();
		CreateLogicalDevice();
	}

	void CreateLogicalDevice() {
		// Setup the queues
		QueueFamilyIndices QueueIndex = FindQueueFamilies(PhysicalDevice);
		VkDeviceQueueCreateInfo QueueCreateInfo = {};
		QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		QueueCreateInfo.queueFamilyIndex = QueueIndex.GraphicsFamily;
		QueueCreateInfo.queueCount = 1;

		float QueuePriority = 1.0f;
		QueueCreateInfo.pQueuePriorities = &QueuePriority;

		VkPhysicalDeviceFeatures DeviceFeatures = {};

		// Create the logical device
		VkDeviceCreateInfo DeviceCreateInfo = {};
		DeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		DeviceCreateInfo.pQueueCreateInfos = &QueueCreateInfo;
		DeviceCreateInfo.queueCreateInfoCount = 1;
		DeviceCreateInfo.pEnabledFeatures = &DeviceFeatures;

		DeviceCreateInfo.enabledExtensionCount = 0;

		if (bEnableValidationLayer) {
			DeviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(gValidationLayers.size());
			DeviceCreateInfo.ppEnabledLayerNames = gValidationLayers.data();
		}
		else {
			DeviceCreateInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(PhysicalDevice, &DeviceCreateInfo, nullptr, &LogicalDevice) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create Logical Device");
		}

		vkGetDeviceQueue(LogicalDevice, QueueIndex.GraphicsFamily, 0, &graphicsQueue);
	}

	void PickPhysicalDevice() {
		uint32_t DeviceCount;
		vkEnumeratePhysicalDevices(Instance, &DeviceCount, nullptr);

		if (DeviceCount == 0) {
			throw std::runtime_error("Failed to find GPUs with Vulkan support");
		}

		std::vector<VkPhysicalDevice> Devices(DeviceCount);
		vkEnumeratePhysicalDevices(Instance, &DeviceCount, Devices.data());

		PhysicalDevice = VK_NULL_HANDLE;

		std::multimap<int, VkPhysicalDevice> Candidates;
		for (auto& Device : Devices) {
			int Score = RateDeviceSuitablility(Device);
			Candidates.insert(std::make_pair(Score, Device));
		}

		if (Candidates.rbegin()->first > 0) {
			PhysicalDevice = Candidates.rbegin()->second;
		}
		else {
			throw std::runtime_error("Failed to find a suitable GPU");
		}
	}

	int RateDeviceSuitablility(VkPhysicalDevice Device) {
		VkPhysicalDeviceProperties Properties;
		VkPhysicalDeviceFeatures Features;
		vkGetPhysicalDeviceProperties(Device, &Properties);
		vkGetPhysicalDeviceFeatures(Device, &Features);

		std::cout << "Device Found: " << Properties.deviceName << std::endl;

		int Score = 0;
		if (Properties.deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			Score += 1000;
		}

		Score += Properties.limits.maxImageDimension2D;

		QueueFamilyIndices QueueIndex = FindQueueFamilies(Device);
		if (!QueueIndex.IsComplete()) {
			return 0;
		}

		if (!Features.geometryShader) {
			return 0;
		}
		return Score;
	}

	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice Device) {
		QueueFamilyIndices Indices;

		uint32_t QueueFamilyCount;
		vkGetPhysicalDeviceQueueFamilyProperties(Device, &QueueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> QueueFamilies(QueueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(Device, &QueueFamilyCount, QueueFamilies.data());

		int Index = 0;
		for (auto& QueueFamily : QueueFamilies) {
			if (QueueFamily.queueCount > 0 && QueueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				Indices.GraphicsFamily = Index;
			}

			if (Indices.IsComplete()) {
				break;
			}
			Index++;
		}

		return Indices;
	}

	void SetupDebugCallback() {
		if (!bEnableValidationLayer) return;

		VkDebugReportCallbackCreateInfoEXT CreateInfo = {};
		CreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		CreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		CreateInfo.pfnCallback = DebugReport;
		CreateInfo.pUserData = this;

		if (CreateDebugResultCallbackEXT(Instance, &CreateInfo, nullptr, &DebugReportCallback) != VK_SUCCESS) {
			throw std::runtime_error("Failed to setup debug report callback");
		}
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
		
		auto Extensions = GetRequiredExtensions();
		CreateInfo.enabledExtensionCount = static_cast<uint32_t>(Extensions.size());
		CreateInfo.ppEnabledExtensionNames = Extensions.data();

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

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugReport(
		VkDebugReportFlagsEXT flags,
		VkDebugReportObjectTypeEXT objType,
		uint64_t obj,
		size_t location,
		int32_t code,
		const char* layerPrefix,
		const char* msg,
		void* userData) {

		std::cerr << "Validation Layer: " << msg << std::endl;

		return VK_FALSE;
	}

	std::vector<const char*> GetRequiredExtensions() {
		uint32_t ExtensionCount = 0;
		const char** RawExtensions = glfwGetRequiredInstanceExtensions(&ExtensionCount);
		std::vector<const char*> Extensions(RawExtensions, RawExtensions + ExtensionCount);

		if (bEnableValidationLayer) {
			Extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}
		return Extensions;
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
		vkDestroyDevice(LogicalDevice, nullptr);

		if (bEnableValidationLayer) {
			DestroyDebugReportCallbackEXT(Instance, DebugReportCallback, nullptr);
		}

		vkDestroyInstance(Instance, nullptr);

		glfwDestroyWindow(Window);
		Window = nullptr;

		glfwTerminate();
	}

private:
	VkInstance Instance;
	VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
	VkDevice LogicalDevice;
	VkQueue graphicsQueue;
	VkDebugReportCallbackEXT DebugReportCallback;
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
