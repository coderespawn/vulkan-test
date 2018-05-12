#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <fstream>


const std::vector<const char*> gValidationLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

const std::vector<const char*> gDeviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

template<typename T>
inline T Clamp(T x, T min, T max) {
	return std::max(min, std::min(max, x));
}


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
	int PresentFamily = -1;
	bool IsComplete() {
		return GraphicsFamily >= 0
			&& PresentFamily >= 0;
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR Capabilities;
	std::vector<VkSurfaceFormatKHR> Formats;
	std::vector<VkPresentModeKHR> PresentModes;
};

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

std::vector<char> ReadFile(const std::string& Filename) {
	std::ifstream File(Filename, std::ios::ate | std::ios::binary);

	if (!File.is_open()) {
		throw std::runtime_error("Failed to open file: " + Filename);
	}

	size_t FileSize = (size_t)File.tellg();
	std::vector<char> Buffer(FileSize);
	File.seekg(0);
	File.read(Buffer.data(), FileSize);
	File.close();

	return Buffer;
}

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
		CreateSurface();
		PickPhysicalDevice();
		CreateLogicalDevice();
		CreateSwapChain();
		CreateImageViews();
		CreateRenderPass();
		CreateGraphicsPipeline();
		CreateFrameBuffers();
	}

	void CreateFrameBuffers() {
		SwapChainFrameBuffers.resize(SwapChainImageViews.size());

		for (size_t i = 0; i < SwapChainImageViews.size(); i++) {
			VkImageView Attachments[] = {
				SwapChainImageViews[i]
			};

			VkFramebufferCreateInfo FrameBufferInfo = {};
			FrameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			FrameBufferInfo.renderPass = RenderPass;
			FrameBufferInfo.attachmentCount = 1;
			FrameBufferInfo.pAttachments = Attachments;
			FrameBufferInfo.width = SwapChainExtent.width;
			FrameBufferInfo.height = SwapChainExtent.height;
			FrameBufferInfo.layers = 1;

			if (vkCreateFramebuffer(LogicalDevice, &FrameBufferInfo, nullptr, &SwapChainFrameBuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create frame buffer");
			}
		}
	}

	void CreateRenderPass() {
		VkAttachmentDescription ColorAttachment = {};
		ColorAttachment.format = SwapChainImageFormat;
		ColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		ColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		ColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		ColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		ColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference ColorAttachmentRef = {};
		ColorAttachmentRef.attachment = 0;
		ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription Subpass = {};
		Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		Subpass.colorAttachmentCount = 1;
		Subpass.pColorAttachments = &ColorAttachmentRef;

		VkRenderPassCreateInfo RenderPassInfo = {};
		RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		RenderPassInfo.attachmentCount = 1;
		RenderPassInfo.pAttachments = &ColorAttachment;
		RenderPassInfo.subpassCount = 1;
		RenderPassInfo.pSubpasses = &Subpass;

		if (vkCreateRenderPass(LogicalDevice, &RenderPassInfo, nullptr, &RenderPass) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create render pass");
		}
	}

	void CreateGraphicsPipeline() {
		auto VertShaderCode = ReadFile("../Shaders/vert.spv");
		auto FragShaderCode = ReadFile("../Shaders/frag.spv");

		VertShaderModule = CreateShaderModule(VertShaderCode);
		FragShaderModule = CreateShaderModule(FragShaderCode);

		VkPipelineShaderStageCreateInfo VertexStageInfo = {};
		VertexStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		VertexStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		VertexStageInfo.module = VertShaderModule;
		VertexStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo FragStageInfo = {};
		FragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		FragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		FragStageInfo.module = FragShaderModule;
		FragStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo ShaderStages[] = { VertexStageInfo, FragStageInfo };
		VkPipelineVertexInputStateCreateInfo VertexInputInfo = {};
		VertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		VertexInputInfo.vertexBindingDescriptionCount = 0;
		VertexInputInfo.pVertexBindingDescriptions = nullptr;
		VertexInputInfo.vertexAttributeDescriptionCount = 0;
		VertexInputInfo.pVertexAttributeDescriptions = nullptr;

		VkPipelineInputAssemblyStateCreateInfo InputAssemblyInfo = {};
		InputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		InputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
		
		VkViewport Viewport = {};
		Viewport.x = 0;
		Viewport.y = 0;
		Viewport.width = (float)SwapChainExtent.width;
		Viewport.height = (float)SwapChainExtent.height;
		Viewport.minDepth = 0.0f;
		Viewport.maxDepth = 1.0f;

		VkRect2D Scissor = {};
		Scissor.offset = { 0, 0 };
		Scissor.extent = SwapChainExtent;
		
		VkPipelineViewportStateCreateInfo ViewportStateInfo = {};
		ViewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		ViewportStateInfo.viewportCount = 1;
		ViewportStateInfo.pViewports = &Viewport;
		ViewportStateInfo.scissorCount = 1;
		ViewportStateInfo.pScissors = &Scissor;

		VkPipelineRasterizationStateCreateInfo RasterizationInfo = {};
		RasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		RasterizationInfo.depthClampEnable = VK_FALSE;
		RasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
		RasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
		RasterizationInfo.lineWidth = 1.0f;
		RasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		RasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		RasterizationInfo.depthBiasEnable = VK_FALSE;
		RasterizationInfo.depthBiasConstantFactor = 0.0f;
		RasterizationInfo.depthBiasClamp = 0.0f;
		RasterizationInfo.depthBiasSlopeFactor = 0.0f;

		VkPipelineMultisampleStateCreateInfo MultiSamplingInfo = {};
		MultiSamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		MultiSamplingInfo.sampleShadingEnable = VK_FALSE;
		MultiSamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		MultiSamplingInfo.minSampleShading = 1.0f;
		MultiSamplingInfo.pSampleMask = nullptr;
		MultiSamplingInfo.alphaToCoverageEnable = VK_FALSE;
		MultiSamplingInfo.alphaToOneEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState ColorBlendAttachment = {};
		ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT 
			| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		ColorBlendAttachment.blendEnable = VK_TRUE;
		ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo ColorBlendInfo = {};
		ColorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		ColorBlendInfo.logicOpEnable = VK_FALSE;
		ColorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
		ColorBlendInfo.attachmentCount = 1;
		ColorBlendInfo.pAttachments = &ColorBlendAttachment;
		ColorBlendInfo.blendConstants[0] = 0.0f;
		ColorBlendInfo.blendConstants[1] = 0.0f;
		ColorBlendInfo.blendConstants[2] = 0.0f;
		ColorBlendInfo.blendConstants[3] = 0.0f;

		VkDynamicState DynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};

		VkPipelineDynamicStateCreateInfo DynamicStateInfo = {};
		DynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		DynamicStateInfo.dynamicStateCount = 2;
		DynamicStateInfo.pDynamicStates = DynamicStates;

		VkPipelineLayoutCreateInfo PipelineLayoutInfo = {};
		PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		PipelineLayoutInfo.setLayoutCount = 0;
		PipelineLayoutInfo.pSetLayouts = nullptr;
		PipelineLayoutInfo.pushConstantRangeCount = 0;
		PipelineLayoutInfo.pPushConstantRanges = nullptr;

		if (vkCreatePipelineLayout(LogicalDevice, &PipelineLayoutInfo, nullptr, &PipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create Pipeline Layout");
		}

		VkGraphicsPipelineCreateInfo PipelineInfo = {};
		PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		PipelineInfo.stageCount = 2;
		PipelineInfo.pStages = ShaderStages;
		PipelineInfo.pVertexInputState = &VertexInputInfo;
		PipelineInfo.pInputAssemblyState = &InputAssemblyInfo;
		PipelineInfo.pViewportState = &ViewportStateInfo;
		PipelineInfo.pRasterizationState = &RasterizationInfo;
		PipelineInfo.pMultisampleState = &MultiSamplingInfo;
		PipelineInfo.pDepthStencilState = nullptr;
		PipelineInfo.pColorBlendState = &ColorBlendInfo;
		PipelineInfo.pDynamicState = nullptr;
		PipelineInfo.layout = PipelineLayout;
		PipelineInfo.renderPass = RenderPass;
		PipelineInfo.subpass = 0;
		PipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		PipelineInfo.basePipelineIndex = -1;

		if (vkCreateGraphicsPipelines(LogicalDevice, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &GraphicsPipeline) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create graphics pipeline");
		}
	}

	VkShaderModule CreateShaderModule(const std::vector<char>& Code) {
		VkShaderModuleCreateInfo CreateInfo = {};
		CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		CreateInfo.codeSize = Code.size();
		CreateInfo.pCode = reinterpret_cast<const uint32_t*>(Code.data());

		VkShaderModule ShaderModule;
		if (vkCreateShaderModule(LogicalDevice, &CreateInfo, nullptr, &ShaderModule) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create shader module");
		}

		return ShaderModule;
	}

	void CreateSurface() {
		if (glfwCreateWindowSurface(Instance, Window, nullptr, &Surface) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create window surface");
		}
	}

	void CreateLogicalDevice() {
		// Setup the queues
		QueueFamilyIndices QueueIndex = FindQueueFamilies(PhysicalDevice);
		std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;
		std::set<int> UniqueQueueFamilies = { QueueIndex.GraphicsFamily, QueueIndex.PresentFamily };
		float QueuePriority = 1.0f;

		for (int QueueFamily : UniqueQueueFamilies) {
			VkDeviceQueueCreateInfo QueueCreateInfo = {};
			QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			QueueCreateInfo.queueFamilyIndex = QueueFamily;
			QueueCreateInfo.queueCount = 1;
			QueueCreateInfo.pQueuePriorities = &QueuePriority;
			QueueCreateInfos.push_back(QueueCreateInfo);
		}

		VkPhysicalDeviceFeatures DeviceFeatures = {};

		// Create the logical device
		VkDeviceCreateInfo DeviceCreateInfo = {};
		DeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		DeviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(QueueCreateInfos.size());
		DeviceCreateInfo.pQueueCreateInfos = QueueCreateInfos.data();
		DeviceCreateInfo.pEnabledFeatures = &DeviceFeatures;

		DeviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(gDeviceExtensions.size());
		DeviceCreateInfo.ppEnabledExtensionNames = gDeviceExtensions.data();

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

		vkGetDeviceQueue(LogicalDevice, QueueIndex.GraphicsFamily, 0, &GraphicsQueue);
		vkGetDeviceQueue(LogicalDevice, QueueIndex.PresentFamily, 0, &PresentQueue);
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

		QueueFamilyIndices QueueIndex = FindQueueFamilies(Device);
		if (!QueueIndex.IsComplete()) {
			return 0;
		}

		if (!CheckDeviceExtensionSupport(Device)) {
			return 0;
		}

		// Check if the swap chains are supported
		SwapChainSupportDetails SwapChainSupport = QuerySwapChainSupport(Device);
		if (SwapChainSupport.Formats.empty() || SwapChainSupport.PresentModes.empty()) 
		{
			return 0;
		}


		if (!Features.geometryShader) {
			return 0;
		}

		int Score = 0;
		if (Properties.deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			Score += 1000;
		}

		Score += Properties.limits.maxImageDimension2D;

		return Score;
	}

	void CreateSwapChain() {
		SwapChainSupportDetails Details = QuerySwapChainSupport(PhysicalDevice);

		VkSurfaceFormatKHR SurfaceFormat = ChooseSwapSurfaceFormat(Details.Formats);
		VkPresentModeKHR PresentMode = ChooseSwapPresentMode(Details.PresentModes);
		VkExtent2D Extent = ChooseSwapExtent(Details.Capabilities);

		uint32_t ImageCount = Details.Capabilities.minImageCount + 1;
		if (Details.Capabilities.maxImageCount > 0 && ImageCount > Details.Capabilities.maxImageCount) {
			ImageCount = Details.Capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR CreateInfo = {};
		CreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		CreateInfo.surface = Surface;
		CreateInfo.minImageCount = ImageCount;
		CreateInfo.imageFormat = SurfaceFormat.format;
		CreateInfo.imageColorSpace = SurfaceFormat.colorSpace;
		CreateInfo.imageExtent = Extent;
		CreateInfo.imageArrayLayers = 1;
		CreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		// Setup queue family configuration
		QueueFamilyIndices Indices = FindQueueFamilies(PhysicalDevice);
		uint32_t IndicesArray[] = { (uint32_t)Indices.GraphicsFamily, (uint32_t)Indices.PresentFamily };
		if (Indices.GraphicsFamily != Indices.PresentFamily) {
			CreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			CreateInfo.queueFamilyIndexCount = 2;
			CreateInfo.pQueueFamilyIndices = IndicesArray;
		}
		else {
			CreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			CreateInfo.queueFamilyIndexCount = 0;
			CreateInfo.pQueueFamilyIndices = nullptr;
		}

		CreateInfo.preTransform = Details.Capabilities.currentTransform;
		CreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		CreateInfo.presentMode = PresentMode;
		CreateInfo.clipped = VK_TRUE;
		CreateInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(LogicalDevice, &CreateInfo, nullptr, &SwapChain) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create swap chain");
		}

		// Retrieve the swap chain images
		uint32_t SwapChainImageCount = 0;
		vkGetSwapchainImagesKHR(LogicalDevice, SwapChain, &SwapChainImageCount, nullptr);
		SwapChainImages.resize(SwapChainImageCount);
		vkGetSwapchainImagesKHR(LogicalDevice, SwapChain, &SwapChainImageCount, SwapChainImages.data());

		SwapChainImageFormat = SurfaceFormat.format;
		SwapChainExtent = Extent;
	}

	void CreateImageViews() {
		SwapChainImageViews.resize(SwapChainImages.size());

		for (size_t i = 0; i < SwapChainImageViews.size(); i++) {
			VkImageViewCreateInfo CreateInfo = {};
			CreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			CreateInfo.image = SwapChainImages[i];
			CreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			CreateInfo.format = SwapChainImageFormat;
			CreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			CreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			CreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			CreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			CreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			CreateInfo.subresourceRange.baseMipLevel = 0;
			CreateInfo.subresourceRange.levelCount = 1;
			CreateInfo.subresourceRange.baseArrayLayer = 0;
			CreateInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(LogicalDevice, &CreateInfo, nullptr, &SwapChainImageViews[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create swap image view");
			}
		}
	}

	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& Capabilities) {
		if (Capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return Capabilities.currentExtent;
		}
		else {
			VkExtent2D ActualExtent = { 
				Clamp(ScreenWidth, Capabilities.minImageExtent.width, Capabilities.maxImageExtent.width),
				Clamp(ScreenHeight, Capabilities.minImageExtent.height, Capabilities.maxImageExtent.height)
			};

			return ActualExtent;
		}
	}

	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& AvailableModes) {
		VkPresentModeKHR BestMode = VK_PRESENT_MODE_FIFO_KHR;

		for (const auto& AvailableMode : AvailableModes) {
			if (AvailableMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				// Use triple buffering since it is available
				return AvailableMode;
			}
			else if (AvailableMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
				BestMode = AvailableMode;
			}
		}
		// Use v-sync
		return BestMode;
	}

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& AvailableFormats) {
		if (AvailableFormats.size() == 1 && AvailableFormats[0].format == VK_FORMAT_UNDEFINED) {
			return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		}

		for (const auto& Format : AvailableFormats) {
			if (Format.format == VK_FORMAT_B8G8R8A8_UNORM && Format.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT) {
				return Format;
			}
		}
		return AvailableFormats[0];
	}

	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice Device) {
		SwapChainSupportDetails Details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Device, Surface, &Details.Capabilities);

		uint32_t FormatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(Device, Surface, &FormatCount, nullptr);
		if (FormatCount > 0) {
			Details.Formats.resize(FormatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(Device, Surface, &FormatCount, Details.Formats.data());
		}

		uint32_t PresentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(Device, Surface, &PresentModeCount, nullptr);
		if (PresentModeCount > 0) {
			Details.PresentModes.resize(PresentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(Device, Surface, &PresentModeCount, Details.PresentModes.data());
		}

		return Details;
	}

	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice Device) {
		QueueFamilyIndices Indices;

		uint32_t QueueFamilyCount;
		vkGetPhysicalDeviceQueueFamilyProperties(Device, &QueueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> QueueFamilies(QueueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(Device, &QueueFamilyCount, QueueFamilies.data());

		int Index = 0;
		for (auto& QueueFamily : QueueFamilies) {
			if (QueueFamily.queueCount > 0) {
				if (QueueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
					Indices.GraphicsFamily = Index;
				}

				VkBool32 PresentSupport = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(Device, Index, Surface, &PresentSupport);
				if (PresentSupport) {
					Indices.PresentFamily = Index;
				}
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

	std::vector<const char*> GetRequiredExtensions() {
		uint32_t ExtensionCount = 0;
		const char** RawExtensions = glfwGetRequiredInstanceExtensions(&ExtensionCount);
		std::vector<const char*> Extensions(RawExtensions, RawExtensions + ExtensionCount);

		if (bEnableValidationLayer) {
			Extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}
		return Extensions;
	}

	bool CheckDeviceExtensionSupport(VkPhysicalDevice Device) {
		uint32_t ExtensionCount;
		vkEnumerateDeviceExtensionProperties(Device, nullptr, &ExtensionCount, nullptr);

		std::vector<VkExtensionProperties> AvailableExtensions(ExtensionCount);
		vkEnumerateDeviceExtensionProperties(Device, nullptr, &ExtensionCount, AvailableExtensions.data());

		std::set<std::string> RequiredExtensions(gDeviceExtensions.begin(), gDeviceExtensions.end());
		for (const auto& Extension : AvailableExtensions) {
			RequiredExtensions.erase(Extension.extensionName);
		}

		return RequiredExtensions.empty();
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

		Window = glfwCreateWindow(ScreenWidth, ScreenHeight, "My Vulkan Window", nullptr, nullptr);

	}

	void MainLoop() {
		while (!glfwWindowShouldClose(Window)) {
			glfwPollEvents();
		}
	}

	void CleanUp() {
		for (VkFramebuffer FrameBuffer : SwapChainFrameBuffers) {
			vkDestroyFramebuffer(LogicalDevice, FrameBuffer, nullptr);
		}
		SwapChainFrameBuffers.clear();

		vkDestroyPipeline(LogicalDevice, GraphicsPipeline, nullptr);
		vkDestroyRenderPass(LogicalDevice, RenderPass, nullptr);
		vkDestroyPipelineLayout(LogicalDevice, PipelineLayout, nullptr);
		vkDestroyShaderModule(LogicalDevice, VertShaderModule, nullptr);
		vkDestroyShaderModule(LogicalDevice, FragShaderModule, nullptr);

		for (auto ImageView : SwapChainImageViews) {
			vkDestroyImageView(LogicalDevice, ImageView, nullptr);
		}
		SwapChainImageViews.clear();

		vkDestroySwapchainKHR(LogicalDevice, SwapChain, nullptr);
		vkDestroyDevice(LogicalDevice, nullptr);

		if (bEnableValidationLayer) {
			DestroyDebugReportCallbackEXT(Instance, DebugReportCallback, nullptr);
		}

		vkDestroySurfaceKHR(Instance, Surface, nullptr);
		vkDestroyInstance(Instance, nullptr);

		glfwDestroyWindow(Window);
		Window = nullptr;

		glfwTerminate();
	}

private:
	VkInstance Instance;
	VkDebugReportCallbackEXT DebugReportCallback;
	VkSurfaceKHR Surface;
	VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
	VkDevice LogicalDevice;
	VkQueue GraphicsQueue;
	VkQueue PresentQueue;
	GLFWwindow* Window;

	VkRenderPass RenderPass;
	VkPipelineLayout PipelineLayout;
	VkPipeline GraphicsPipeline;

	VkSwapchainKHR SwapChain;
	VkFormat SwapChainImageFormat;
	VkExtent2D SwapChainExtent;
	std::vector<VkImage> SwapChainImages;
	std::vector<VkImageView> SwapChainImageViews;
	std::vector<VkFramebuffer> SwapChainFrameBuffers; 

	VkShaderModule VertShaderModule;
	VkShaderModule FragShaderModule;

	uint32_t ScreenWidth = 800;
	uint32_t ScreenHeight = 600;
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
