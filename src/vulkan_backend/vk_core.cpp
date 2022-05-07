#define VK_BACKEND
#include "vk_core.h"
#include <string>
#include <vector>
using namespace backend_vk_private;

struct core_private {
public:
	bool isAnyWindowResized = false; //Instead of checking each window each frame, just check this
	bool isActive_SurfaceKHR = false, isSupported_PhysicalDeviceProperties2 = true;
};
core_private* hidden = nullptr;

void GFX_Error_Callback(int error_code, const char* description) {
	printer(result_tgfx_FAIL, (std::string("GLFW error: ") + description).c_str());
}
void create_window();
void Setup_Debugging();
void Create_Instance();
void Check_ComputerSpecs();
void second_initialization();
void core_vk::initialize() {
	//Set error callback to handle all glfw errors (including initialization error)!
	glfwSetErrorCallback(GFX_Error_Callback);

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	Create_Instance();
	Setup_Debugging();
	Check_ComputerSpecs();

	second_initialization();

	create_window();
}

extern void renderer_Run();
void core_vk::run() {
	renderer_Run();

	glfwSwapBuffers(window_glfwhandle);
	glfwPollEvents();
}


std::vector<const char*> Active_InstanceExtensionNames;
std::vector<VkExtensionProperties> Supported_InstanceExtensionList;
std::vector<const char*> Active_DeviceExtensions;
bool IsExtensionSupported(const char* ExtensionName, VkExtensionProperties* SupportedExtensions, unsigned int SupportedExtensionsCount);

bool Check_InstanceExtensions(); 

void Create_Instance() {
	VkApplicationInfo Application_Info = {};
	//APPLICATION INFO
	Application_Info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	Application_Info.pApplicationName = "Vulkan DLL";
	Application_Info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	Application_Info.pEngineName = "GFX API";
	Application_Info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	Application_Info.apiVersion = VK_API_VERSION_1_2;

	//CHECK SUPPORTED EXTENSIONs
	uint32_t extension_count;
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
	//Doesn't construct VkExtensionProperties object, so we have to use resize!
	Supported_InstanceExtensionList.resize(extension_count);
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, Supported_InstanceExtensionList.data());
	if (!Check_InstanceExtensions()) {
		return;
	}

	//CHECK SUPPORTED LAYERS
	unsigned int Supported_LayerNumber = 0;
	vkEnumerateInstanceLayerProperties(&Supported_LayerNumber, nullptr);
	VkLayerProperties* Supported_LayerList = new VkLayerProperties[Supported_LayerNumber];
	vkEnumerateInstanceLayerProperties(&Supported_LayerNumber, Supported_LayerList);

	//INSTANCE CREATION INFO
	VkInstanceCreateInfo InstCreation_Info = {};
	InstCreation_Info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	InstCreation_Info.pApplicationInfo = &Application_Info;
	//Extensions
	InstCreation_Info.enabledExtensionCount = Active_InstanceExtensionNames.size();
	InstCreation_Info.ppEnabledExtensionNames = Active_InstanceExtensionNames.data();

	//Validation Layers
	const char* Validation_Layers[1] = {
		"VK_LAYER_KHRONOS_validation"
};
	InstCreation_Info.enabledLayerCount = 1;
	InstCreation_Info.ppEnabledLayerNames = Validation_Layers;

	if (vkCreateInstance(&InstCreation_Info, nullptr, &vkinst) != VK_SUCCESS) {
		printer(result_tgfx_FAIL, "Failed to create a Vulkan Instance!");
	}
}

extern void analize_gpumemory();
extern void analize_queues();
void Describe_SupportedExtensions();
void CheckDeviceExtensionProperties();
void Check_ComputerSpecs() {
	//CHECK GPUs
	uint32_t GPU_NUMBER = 0;
	vkEnumeratePhysicalDevices(vkinst, &GPU_NUMBER, nullptr);
	std::vector<VkPhysicalDevice> Physical_GPU_LIST(GPU_NUMBER, VK_NULL_HANDLE);
	vkEnumeratePhysicalDevices(vkinst, &GPU_NUMBER, Physical_GPU_LIST.data());

	if (GPU_NUMBER == 0) {
		printer(result_tgfx_FAIL, "There is no GPU that has Vulkan support! Updating your drivers or Upgrading the OS may help");
	}


	//GET GPU INFORMATIONs, QUEUE FAMILIES etc
	for (unsigned int i = 0; i < GPU_NUMBER; i++) {
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(Physical_GPU_LIST[i], &props);
		if (props.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			physicaldevice = Physical_GPU_LIST[i];

			vkGetPhysicalDeviceFeatures(physicaldevice, &physicaldevice_features);

			unsigned int queuefamcount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(physicaldevice, &queuefamcount, nullptr);
			queuefam_props.resize(queuefamcount);
			vkGetPhysicalDeviceQueueFamilyProperties(physicaldevice, &queuefamcount, queuefam_props.data());

			vkGetPhysicalDeviceMemoryProperties(physicaldevice, &memory_props);


			analize_gpumemory();
			analize_queues();
			Describe_SupportedExtensions();
			CheckDeviceExtensionProperties();

		}
		else { printer(result_tgfx_WARNING, "Non-Discrete GPUs are not available in vulkan backend!"); }
	}
}


//While enabling features, some struct should be chained. This struct is to keep data object lifetimes optimal
struct device_features_chainedstructs {
	VkPhysicalDeviceDescriptorIndexingFeatures DescIndexingFeatures = {};
	VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures seperatedepthstencillayouts = {};
	VkPhysicalDeviceBufferDeviceAddressFeatures bufferdeviceaddress = {};
};
bool IsExtensionSupported(const char* ExtensionName, VkExtensionProperties* SupportedExtensions, unsigned int SupportedExtensionsCount) {
	bool Is_Found = false;
	for (unsigned int supported_extension_index = 0; supported_extension_index < SupportedExtensionsCount; supported_extension_index++) {
		if (strcmp(ExtensionName, SupportedExtensions[supported_extension_index].extensionName)) {
			return true;
		}
	}
	printer(result_tgfx_WARNING, ("Extension: " + std::string(ExtensionName) + " is not supported by the GPU!").c_str());
	return false;
}
bool Check_InstanceExtensions() {
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	for (unsigned int i = 0; i < glfwExtensionCount; i++) {
		if (!IsExtensionSupported(glfwExtensions[i], Supported_InstanceExtensionList.data(), Supported_InstanceExtensionList.size())) {
			printer(result_tgfx_INVALIDARGUMENT, "Your vulkan instance doesn't support extensions that're required by GLFW. This situation is not tested, so report your device to the author!");
			return false;
		}
		Active_InstanceExtensionNames.push_back(glfwExtensions[i]);
	}


	if (IsExtensionSupported(VK_KHR_SURFACE_EXTENSION_NAME, Supported_InstanceExtensionList.data(), Supported_InstanceExtensionList.size())) {
		Active_InstanceExtensionNames.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
	}
	else {
		printer(result_tgfx_WARNING, "Your Vulkan instance doesn't support to display a window, so you shouldn't use any window related functionality such as: GFXRENDERER->Create_WindowPass, GFX->Create_Window, GFXRENDERER->Swap_Buffers ...");
	}

	//Check PhysicalDeviceProperties2KHR
	if (Application_Info.apiVersion == VK_API_VERSION_1_0) {
		if (!IsExtensionSupported(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, Supported_InstanceExtensionList.data(), Supported_InstanceExtensionList.size())) {
			hidden->isSupported_PhysicalDeviceProperties2 = false;
			printer(result_tgfx_FAIL, "Your OS doesn't support Physical Device Properties 2 extension which should be supported, so Vulkan device creation has failed!");
			return false;
		}
		else { Active_InstanceExtensionNames.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME); }
	}

#ifdef VULKAN_DEBUGGING
	if (IsExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, Supported_InstanceExtensionList.data(), Supported_InstanceExtensionList.size())) {
		Active_InstanceExtensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
#endif
}
void Describe_SupportedExtensions() {
	//GET SUPPORTED DEVICE EXTENSIONS
	VkExtensionProperties* Exts = nullptr;	unsigned int ExtsCount = 0;
	vkEnumerateDeviceExtensionProperties(physicaldevice, nullptr, &ExtsCount, nullptr);
	Exts = new VkExtensionProperties[ExtsCount];
	vkEnumerateDeviceExtensionProperties(physicaldevice, nullptr, &ExtsCount, Exts);

	//GET SUPPORTED FEATURES OF THE DEVICE EXTENSIONS
	VkPhysicalDeviceFeatures2 features2;
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = nullptr;
	device_features_chainedstructs chainer_check;
	{	//Fill pNext to check all necessary features you'll need from the extensions below
		chainer_check.DescIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
		chainer_check.seperatedepthstencillayouts.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES;
		chainer_check.bufferdeviceaddress.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_ADDRESS_FEATURES_EXT;

		features2.pNext = &chainer_check.DescIndexingFeatures;
		chainer_check.DescIndexingFeatures.pNext = &chainer_check.seperatedepthstencillayouts;
		chainer_check.seperatedepthstencillayouts.pNext = &chainer_check.bufferdeviceaddress;
	}
	vkGetPhysicalDeviceFeatures2(physicaldevice, &features2);


	if (IsExtensionSupported(VK_KHR_SWAPCHAIN_EXTENSION_NAME, Exts, ExtsCount)) {
		Active_DeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	}
	else {
		printer(result_tgfx_WARNING, "Current GPU doesn't support to display a swapchain, so you shouldn't use any window related functionality such as: GFXRENDERER->Create_WindowPass, GFX->Create_Window, GFXRENDERER->Swap_Buffers ...");
	}

	if (chainer_check.bufferdeviceaddress.bufferDeviceAddress) {
		if (Application_Info.apiVersion == VK_API_VERSION_1_0 || Application_Info.apiVersion == VK_API_VERSION_1_1) {
			VkPhysicalDeviceBufferDeviceAddressFeatures x;
		}
		else {}
	}
	//Check Descriptor Indexing
	if (chainer_check.DescIndexingFeatures.descriptorBindingVariableDescriptorCount) {
		//First check Maintenance3 and PhysicalDeviceProperties2 for Vulkan 1.0
		if (Application_Info.apiVersion == VK_API_VERSION_1_0) {
			if (IsExtensionSupported(VK_KHR_MAINTENANCE3_EXTENSION_NAME, Exts, ExtsCount) && hidden->isSupported_PhysicalDeviceProperties2) {
				Active_DeviceExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
				if (IsExtensionSupported(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, Exts, ExtsCount)) {
					Active_DeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
				}
			}
		}
		//Maintenance3 and PhysicalDeviceProperties2 is core in 1.1, so check only Descriptor Indexing
		else {
			//If Vulkan is 1.1, check extension
			if (Application_Info.apiVersion == VK_API_VERSION_1_1) {
				if (IsExtensionSupported(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, Exts, ExtsCount)) {
					Active_DeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
				}
			}
			//1.2+ Vulkan supports DescriptorIndexing by default.
			else {
			}
		}
	}
}
void CheckDeviceExtensionProperties() {
	VkPhysicalDeviceProperties2 devprops2;


	devprops2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	descindexinglimits.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
	uniformblocklimits.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES_EXT;
	devprops2.pNext = &descindexinglimits;
	descindexinglimits.pNext = &uniformblocklimits;
	vkGetPhysicalDeviceProperties2(physicaldevice, &devprops2);
}

extern void Create_IMGUI();
extern void Create_Renderer();
extern void Create_GPUContentManager();
void ActivateDeviceExtensionFeatures();
extern void do_allocations();
VkDeviceCreateInfo Logical_Device_CreationInfo{};
device_features_chainedstructs chainer;
void second_initialization() {
	Create_IMGUI();

	//Create Logical Device
	{
		std::vector<VkDeviceQueueCreateInfo> queues_ci;
		//Queue Creation Processes
		for (unsigned int QueueIndex = 0; QueueIndex < queuefam_props.size(); QueueIndex++) {
			queuefam_vk* queuefam = queuefams[QueueIndex];
			VkDeviceQueueCreateInfo QueueInfo = {};
			QueueInfo.flags = 0;
			QueueInfo.pNext = nullptr;
			QueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			QueueInfo.queueFamilyIndex = queuefam->queueFamIndex;
			float QueuePriority = 1.0f;
			float* priorities = new float[queuefam->queuecount];
			QueueInfo.pQueuePriorities = priorities;
			QueueInfo.queueCount = queuefam->queuecount;
			for (unsigned int i = 0; i < queuefam->queuecount; i++) {
				priorities[i] = 1.0f - (float(i) / float(queuefam->queuecount));
			}
			queues_ci.push_back(QueueInfo);
		}

		Logical_Device_CreationInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		Logical_Device_CreationInfo.flags = 0;
		Logical_Device_CreationInfo.pQueueCreateInfos = queues_ci.data();
		Logical_Device_CreationInfo.queueCreateInfoCount = static_cast<uint32_t>(queues_ci.size());
		//This is to destroy datas of extending features
		device_features_chainedstructs chainer_activate;
		ActivateDeviceExtensionFeatures();

		Logical_Device_CreationInfo.enabledExtensionCount = static_cast<uint32_t>(Active_DeviceExtensions.size());
		Logical_Device_CreationInfo.ppEnabledExtensionNames = Active_DeviceExtensions.data();
		Logical_Device_CreationInfo.pEnabledFeatures = &physicaldevice_features;
		Logical_Device_CreationInfo.enabledLayerCount = 0;
		if (vkCreateDevice(physicaldevice, &Logical_Device_CreationInfo, nullptr, &logicaldevice) != VK_SUCCESS) {
			printer(result_tgfx_FAIL, "Vulkan failed to create a Logical Device!");
			return;
		}

		//Get VkQueue Objects
		for (unsigned int i = 0; i < queuefam_props.size(); i++) {
			printer(result_tgfx_SUCCESS, ("Queue Feature Score: " + std::to_string(queuefams[i]->featurescore)).c_str());
			for (unsigned int queueindex = 0; queueindex < queuefams[i]->queuecount; queueindex++) {
				vkGetDeviceQueue(logicaldevice, queuefams[i]->queueFamIndex, queueindex, &(queuefams[i]->queues[queueindex].Queue));
			}
			printer(result_tgfx_SUCCESS, ("After vkGetDeviceQueue() " + std::to_string(i)).c_str());
		}
		//Create Command Pool for each Queue Family
		for (unsigned int queuefamindex = 0; queuefamindex < queuefams.size(); queuefamindex++) {
			if (!(queuefams[queuefamindex]->supportflag.is_COMPUTEsupported || queuefams[queuefamindex]->supportflag.is_GRAPHICSsupported ||
				queuefams[queuefamindex]->supportflag.is_TRANSFERsupported)
				) {
				printer(result_tgfx_SUCCESS, "VulkanRenderer:Command pool creation for a queue has failed because one of the VkQueues doesn't support neither Graphics, Compute or Transfer. So GFX API didn't create a command pool for it!");
				continue;
			}
			for (unsigned char i = 0; i < 2; i++) {
				VkCommandPoolCreateInfo cp_ci_g = {};
				cp_ci_g.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
				cp_ci_g.queueFamilyIndex = queuefams[queuefamindex]->queueFamIndex;
				cp_ci_g.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
				cp_ci_g.pNext = nullptr;

				if (vkCreateCommandPool(logicaldevice, &cp_ci_g, nullptr, &queuefams[queuefamindex]->CommandPools[i].CPHandle) != VK_SUCCESS) {
					printer(result_tgfx_FAIL, "VulkanRenderer: Command pool creation for a queue has failed at vkCreateCommandPool()!");
				}
			}
		}

		do_allocations();
	}


	Create_Renderer();
	Create_GPUContentManager();
}

void ActivateDeviceExtensionFeatures() {
	const void*& dev_ci_Next = Logical_Device_CreationInfo.pNext;
	void** extending_Next = nullptr;


	//Activate Descriptor Indexing Features
	{
		chainer.DescIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
		chainer.DescIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
		chainer.DescIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
		chainer.DescIndexingFeatures.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
		chainer.DescIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
		chainer.DescIndexingFeatures.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
		chainer.DescIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
		chainer.DescIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
		chainer.DescIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
		chainer.DescIndexingFeatures.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
		chainer.DescIndexingFeatures.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE;
		chainer.DescIndexingFeatures.pNext = nullptr;
		chainer.DescIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

		if (extending_Next) {
			*extending_Next = &chainer.DescIndexingFeatures;
			extending_Next = &chainer.DescIndexingFeatures.pNext;
		}
		else {
			dev_ci_Next = &chainer.DescIndexingFeatures;
			extending_Next = &chainer.DescIndexingFeatures.pNext;
		}
	}
}

void create_window() {
	//Window can't be resizable
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window_glfwhandle = glfwCreateWindow(window_size.x, window_size.y, window_name, NULL, nullptr);

	//Check and Report if GLFW fails
	if (window_glfwhandle == NULL) {
		printer(result_tgfx_FAIL, "Failed to create the window because of GLFW!");
		return;
	}

	//Window VulkanSurface Creation
	if (glfwCreateWindowSurface(vkinst, window_glfwhandle, nullptr, &window_surface) != VK_SUCCESS) {
		printer(result_tgfx_FAIL, "GLFW failed to create a window surface");
		return;
	}

	//Finding GPU_TO_RENDER's Surface Capabilities
	for (unsigned int queuefam_i = 0; queuefam_i < queuefam_props.size(); queuefam_i++) {
		VkBool32 supported;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicaldevice, queuefam_i, window_surface, &supported);
		if (supported) {
			if (!queuefams[queuefam_i]->supportflag.is_PRESENTATIONsupported) { queuefams[queuefam_i]->supportflag.is_PRESENTATIONsupported = true; }
			presentationqueue = &queuefams[queuefam_i]->queues[0];
			break;
		}
	}
	if (!presentationqueue) {
		printer(result_tgfx_FAIL, "Vulkan backend supports windows that your GPU supports but your GPU doesn't support current window. So window creation has failed!");
		return;
	}

	VkSurfaceCapabilitiesKHR cap;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicaldevice, window_surface, &cap);
	uint32_t FormatCount = 0;
	std::vector<VkSurfaceFormatKHR> formats;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicaldevice, window_surface, &FormatCount, nullptr);
	formats.resize(FormatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicaldevice, window_surface, &FormatCount, formats.data());
	if (!FormatCount) {
		printer(result_tgfx_FAIL, "This GPU doesn't support this type of windows, please try again with a different window configuration!");
		return;
	}
	if (!cap.maxImageCount > cap.minImageCount) {
		printer(result_tgfx_FAIL, "VulkanCore: Window Surface Capabilities have issues, maxImageCount <= minImageCount!");
		return;
	}


	uint32_t PresentationModesCount = 0;
	VkPresentModeKHR Presentations[10];		//10 is enough, considering future modes
	uint32_t PresentationMode_i = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicaldevice, window_surface, &PresentationModesCount, Presentations);


	//Create Swapchain
	{
		texture_vk* textures = new texture_vk[2];
		//Create VkSwapchainKHR Object
		{

			VkSwapchainCreateInfoKHR swpchn_ci = {};
			swpchn_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			swpchn_ci.flags = 0;
			swpchn_ci.pNext = nullptr;
			swpchn_ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
			swpchn_ci.surface = window_surface;
			swpchn_ci.minImageCount = 2;
			swpchn_ci.imageFormat = window_texture_format;
			swpchn_ci.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
			swpchn_ci.imageExtent = { window_size.x, window_size.y };
			swpchn_ci.imageArrayLayers = 1;
			//Swapchain texture can be used as framebuffer, but we should set its bit!
			swpchn_ci.imageUsage = window_texture_usage;

			swpchn_ci.clipped = VK_TRUE;
			swpchn_ci.preTransform = cap.currentTransform;
			swpchn_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
			swpchn_ci.oldSwapchain = nullptr;

			if (queuefams.size() > 1) { swpchn_ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT; }
			else { swpchn_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; }
			std::vector<uint32_t> queuefamindices(queuefams.size());
			for (unsigned int i = 0; i < queuefamindices.size(); i++) { queuefamindices[i] = i; }
			swpchn_ci.pQueueFamilyIndices = queuefamindices.data();
			swpchn_ci.queueFamilyIndexCount = queuefamindices.size();

			if (vkCreateSwapchainKHR(logicaldevice, &swpchn_ci, nullptr, &window_swapchain) != VK_SUCCESS) {
				printer(result_tgfx_FAIL, "VulkanCore: Failed to create a SwapChain for a Window");
			}
		}

		//Get Swapchain images
		{
			uint32_t created_imagecount = 0;
			vkGetSwapchainImagesKHR(logicaldevice, window_swapchain, &created_imagecount, nullptr);
			if (created_imagecount != 2) {
				printer(result_tgfx_FAIL, "VK backend asked for swapchain textures but Vulkan driver gave less number of textures than intended!");
			}
			VkImage SWPCHN_IMGs[2];
			vkGetSwapchainImagesKHR(logicaldevice, window_swapchain, &created_imagecount, SWPCHN_IMGs);
			for (unsigned int vkim_index = 0; vkim_index < 2; vkim_index++) {
				texture_vk* SWAPCHAINTEXTURE = &textures[vkim_index];
				SWAPCHAINTEXTURE->CHANNELs = window_texture_format;
				SWAPCHAINTEXTURE->WIDTH = window_size.x;
				SWAPCHAINTEXTURE->HEIGHT = window_size.y;
				SWAPCHAINTEXTURE->DATA_SIZE = SWAPCHAINTEXTURE->WIDTH * SWAPCHAINTEXTURE->HEIGHT * 4;
				SWAPCHAINTEXTURE->Image = SWPCHN_IMGs[vkim_index];
				SWAPCHAINTEXTURE->MIPCOUNT = 1;
				SWAPCHAINTEXTURE->USAGE = window_texture_usage;
			}

			for (unsigned int i = 0; i < 2; i++) {
				VkImageViewCreateInfo ImageView_ci = {};
				ImageView_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				texture_vk* SwapchainTexture = &textures[i];
				ImageView_ci.image = SwapchainTexture->Image;
				ImageView_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
				ImageView_ci.format = window_texture_format;
				ImageView_ci.flags = 0;
				ImageView_ci.pNext = nullptr;
				ImageView_ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				ImageView_ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				ImageView_ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				ImageView_ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
				ImageView_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				ImageView_ci.subresourceRange.baseArrayLayer = 0;
				ImageView_ci.subresourceRange.baseMipLevel = 0;
				ImageView_ci.subresourceRange.layerCount = 1;
				ImageView_ci.subresourceRange.levelCount = 1;

				if (vkCreateImageView(logicaldevice, &ImageView_ci, nullptr, &SwapchainTexture->ImageView) != VK_SUCCESS) {
					printer(result_tgfx_FAIL, "VulkanCore: Image View creation has failed!");
				}
			}
		}

		swapchaintextures[0] = &textures[0];
		swapchaintextures[1] = &textures[1];
	}
}




VKAPI_ATTR VkBool32 VKAPI_CALL VK_DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT Message_Severity, VkDebugUtilsMessageTypeFlagsEXT Message_Type, const VkDebugUtilsMessengerCallbackDataEXT* pCallback_Data, void* pUserData);
PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT();
PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT();
VkDebugUtilsMessengerEXT Debug_Messenger = VK_NULL_HANDLE;
void Setup_Debugging() {
	VkDebugUtilsMessengerCreateInfoEXT DebugMessenger_CreationInfo = {};
	DebugMessenger_CreationInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	DebugMessenger_CreationInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	DebugMessenger_CreationInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
	DebugMessenger_CreationInfo.pfnUserCallback = VK_DebugCallback;
	DebugMessenger_CreationInfo.pNext = nullptr;
	DebugMessenger_CreationInfo.pUserData = nullptr;

	auto func = vkCreateDebugUtilsMessengerEXT();
	if (func(backend_vk_private::vkinst, &DebugMessenger_CreationInfo, nullptr, &Debug_Messenger) != VK_SUCCESS) {
		printer(result_tgfx_FAIL, "Vulkan's Debug Callback system failed to start!");
	}
}

VKAPI_ATTR VkBool32 VKAPI_CALL VK_DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT Message_Severity, VkDebugUtilsMessageTypeFlagsEXT Message_Type, const VkDebugUtilsMessengerCallbackDataEXT* pCallback_Data, void* pUserData) {
	std::string Callback_Type = "";
	switch (Message_Type) {
	case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
		Callback_Type = "VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT : Some event has happened that is unrelated to the specification or performance\n";
		break;
	case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
		Callback_Type = "VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: Something has happened that violates the specification or indicates a possible mistake\n";
		break;
	case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
		Callback_Type = "VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: Potential non-optimal use of Vulkan\n";
		break;
	default:
		printer(result_tgfx_FAIL, "Vulkan Callback has returned a unsupported Message_Type");
		return true;
		break;
	}

	switch (Message_Severity)
	{
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		printer(result_tgfx_SUCCESS, pCallback_Data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		printer(result_tgfx_WARNING, pCallback_Data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		printer(result_tgfx_FAIL, pCallback_Data->pMessage);
		break;
	default:
		printer(result_tgfx_FAIL, "Vulkan Callback has returned a unsupported debug message type!");
		return true;
	}
	return false;
}
PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT() {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkinst, "vkCreateDebugUtilsMessengerEXT");
	if (func == nullptr) {
		printer(result_tgfx_FAIL, "(Vulkan failed to load vkCreateDebugUtilsMessengerEXT function!");
		return nullptr;
	}
	return func;
}
PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT() {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkinst, "vkDestroyDebugUtilsMessengerEXT");
	if (func == nullptr) {
		printer(result_tgfx_FAIL, "(Vulkan failed to load vkDestroyDebugUtilsMessengerEXT function!");
		return nullptr;
	}
	return func;
}