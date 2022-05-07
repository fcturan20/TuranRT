#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <mutex>
//This preprocessor is only used while build VK backend, don't use in other projects
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>




//Systems 

enum result_tgfx : unsigned char {
    result_tgfx_SUCCESS = 0,
    result_tgfx_FAIL = 1,
    result_tgfx_NOTCODED = 2,
    result_tgfx_INVALIDARGUMENT = 3,
    result_tgfx_WRONGTIMING = 4,		//This means the operation is called at the wrong time!
    result_tgfx_WARNING = 5
};


typedef enum memoryallocationtype_tgfx {
    memoryallocationtype_DEVICELOCAL = 0,
    memoryallocationtype_HOSTVISIBLE = 1,
    memoryallocationtype_FASTHOSTVISIBLE = 2,
    memoryallocationtype_READBACK = 3
} memoryallocationtype_tgfx;
typedef struct memory_description_tgfx {
    unsigned int memorytype_id;
    memoryallocationtype_tgfx allocationtype;
    unsigned long max_allocationsize;
} memory_description_tgfx;
extern std::vector<memory_description_tgfx> memdescs;


typedef struct texture_vk* texture_tgfx_handle;
typedef struct fence_vk* fence_id;
typedef struct queue_vk* queue_id;
typedef struct queuefam_vk* queuefam_id;
typedef struct renderpass_vk* renderpass_id;
typedef struct framebuffer_vk* framebuffer_id;
extern queue_id presentationqueue;
extern queue_id allgraphicsqueue;
extern texture_tgfx_handle swapchaintextures[2];
typedef struct commandbuffer_vk* commandbufer_id;

#ifdef VK_BACKEND

#include <turanapi/threadingsys_tapi.h>



//These are used only in vk backend
//Don't access these outside of the project
void printer(result_tgfx result, const char* text);
namespace backend_vk_private {
    extern VkInstance vkinst;
    extern VkApplicationInfo Application_Info;
	extern VkPhysicalDeviceDescriptorIndexingProperties descindexinglimits;
	extern VkPhysicalDeviceInlineUniformBlockPropertiesEXT uniformblocklimits;

    //GPU data
    extern VkDevice logicaldevice;
    extern VkPhysicalDevice physicaldevice;
    extern VkPhysicalDeviceFeatures physicaldevice_features;
    extern std::vector<VkQueueFamilyProperties> queuefam_props;
    extern VkPhysicalDeviceMemoryProperties memory_props;
	extern std::vector<queuefam_vk*> queuefams;

    //Window data
    static constexpr glm::uvec2 window_size(1280, 720);
    static constexpr const char* window_name = "Turan VKRT";
    extern GLFWwindow* window_glfwhandle;
    extern VkSurfaceKHR window_surface;
	static constexpr VkFormat window_texture_format = VK_FORMAT_R8G8B8A8_UNORM;
	extern VkSwapchainKHR window_swapchain;
	static constexpr VkImageUsageFlags window_texture_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT || VK_IMAGE_USAGE_SAMPLED_BIT ||
		VK_IMAGE_USAGE_STORAGE_BIT || VK_IMAGE_USAGE_TRANSFER_SRC_BIT || VK_IMAGE_USAGE_TRANSFER_DST_BIT;;



	//Some algorithms and data structures to help in C++ (like threadlocalvector)

	extern unsigned int thread_count;
	
	template<class T, unsigned int initialsizes = 1024>
	class threadlocal_vector {
		T** lists;
		std::mutex Sync;
		//Element order: thread0-size, thread0-capacity, thread1-size, thread1-capacity...
		unsigned long* sizes_and_capacities;
		inline void expand_if_necessary(unsigned int thread_i) {
			if (sizes_and_capacities[(thread_i * 2)] == sizes_and_capacities[(thread_i * 2) + 1]) {
				T* newlist = nullptr;
				if (sizes_and_capacities[thread_i * 2] > 0) { newlist = new T[sizes_and_capacities[(thread_i * 2) + 1] * 2]; }
				else {
					newlist = new T;
					memcpy(newlist, lists[thread_i], sizeof(T) * sizes_and_capacities[thread_i * 2]);
					delete[] lists[thread_i];
				}

				lists[thread_i] = newlist;
				if (sizes_and_capacities[(thread_i * 2) + 1] == 0) { sizes_and_capacities[(thread_i * 2) + 1] = 1; }
				else { sizes_and_capacities[(thread_i * 2) + 1] *= 2; }
			}
		}
	public:
		threadlocal_vector(const threadlocal_vector& copy) {
			unsigned int threadcount = threadingsys.thread_count();
			lists = new T * [threadcount];
			sizes_and_capacities = new unsigned long[threadingsys.thread_count() * 2];
			for (unsigned int thread_i = 0; thread_i < (threadingsys.thread_count()); thread_i++) {
				lists[thread_i] = new T[copy.sizes_and_capacities[(thread_i * 2) + 1]];
				for (unsigned int element_i = 0; element_i < copy.sizes_and_capacities[thread_i * 2]; element_i++) {
					lists[thread_i][element_i] = copy.lists[thread_i][element_i];
				}
				sizes_and_capacities[thread_i * 2] = copy.sizes_and_capacities[thread_i * 2];
				sizes_and_capacities[(thread_i * 2) + 1] = copy.sizes_and_capacities[(thread_i * 2) + 1];
			}
		}
		//This constructor allocates initial_sizes memory but doesn't add any element to it, so you should use push_back()
		threadlocal_vector() {
			unsigned int threadcount = threadingsys.thread_count();
			lists = new T * [threadcount];
			sizes_and_capacities = new unsigned long[threadingsys.thread_count() * 2];
			if (initialsizes) {
				for (unsigned int thread_i = 0; thread_i < threadingsys.thread_count(); thread_i++) {
					lists[thread_i] = new T[initialsizes];
					sizes_and_capacities[thread_i * 2] = 0;
					sizes_and_capacities[(thread_i * 2) + 1] = initialsizes;
				}
			}
		}
		//This constructor allocates initial_sizes * 2 memory and fills it with initial_sizes number of ref objects
		threadlocal_vector(unsigned long initial_sizes, const T& ref) {
			unsigned int threadcount = threadingsys.thread_count();
			lists = new T * [threadcount];
			sizes_and_capacities = new unsigned long[threadingsys.thread_count() * 2];
			if (initial_sizes) {
				for (unsigned int thread_i = 0; thread_i < threadingsys.thread_count(); thread_i++) {
					lists[thread_i] = new T[initial_sizes * 2];
					for (unsigned long element_i = 0; element_i < initial_sizes; element_i++) {
						lists[thread_i][element_i] = ref;
					}
					sizes_and_capacities[thread_i * 2] = initial_sizes;
					sizes_and_capacities[(thread_i * 2) + 1] = initial_sizes * 2;
				}
			}
		}
		unsigned long size(unsigned int ThreadIndex = UINT32_MAX) {
			return sizes_and_capacities[(ThreadIndex != UINT32_MAX) ? (ThreadIndex) : (threadingsys.this_thread_index() * 2)];
		}
		void push_back(const T& ref, unsigned int ThreadIndex = UINT32_MAX) {
			const unsigned int thread_i = ((ThreadIndex != UINT32_MAX) ? (ThreadIndex) : (threadingsys.this_thread_index()));
			expand_if_necessary(thread_i);
			lists[thread_i][sizes_and_capacities[thread_i * 2]] = ref;
			sizes_and_capacities[thread_i * 2] += 1;
		}
		T* data(unsigned int ThreadIndex = UINT32_MAX) {
			return lists[(ThreadIndex != UINT32_MAX) ? (ThreadIndex) : (threadingsys.this_thread_index())];
		}
		void clear(unsigned int ThreadIndex = UINT32_MAX) {
			sizes_and_capacities[(ThreadIndex != UINT32_MAX) ? (ThreadIndex * 2) : (threadingsys.this_thread_index() * 2)] = 0;
		}
		T& get(unsigned int ThreadIndex, unsigned int ElementIndex) {
			return lists[ThreadIndex][ElementIndex];
		}
		T& operator[](unsigned int ElementIndex) {
			return lists[threadingsys.this_thread_index()][ElementIndex];
		}
		void PauseAllOperations(std::unique_lock<std::mutex>& Locker) {
			assert(0 && "vk_backend::threadlocal_vector::PauseAllOperations() isn't coded");
		}
	};
}

struct commandbuffer_vk {
	VkCommandBuffer CB;
	bool is_Used = false;
};
struct commandpool_vk {
	commandpool_vk() = default;
	commandpool_vk(const commandpool_vk& RefCP) { CPHandle = RefCP.CPHandle; CBs = RefCP.CBs; }
	void operator= (const commandpool_vk& RefCP) { CPHandle = RefCP.CPHandle; CBs = RefCP.CBs; }
	commandbuffer_vk& CreateCommandBuffer();
	void DestroyCommandBuffer();
	VkCommandPool CPHandle = VK_NULL_HANDLE;
	std::vector<commandbuffer_vk*> CBs;
private:
	std::mutex Sync;
};

//Initializes as everything is false (same as CreateInvalidNullFlag)
struct queueflag_vk {
	bool is_GRAPHICSsupported : 1;
	bool is_PRESENTATIONsupported : 1;
	bool is_COMPUTEsupported : 1;
	bool is_TRANSFERsupported : 1;
	bool doesntNeedAnything : 1;	//This is a special flag to be used as "Don't care other parameters, this is a special operation"
	//bool is_VTMEMsupported : 1;	Not supported for now!
	inline queueflag_vk() {
		doesntNeedAnything = false; is_GRAPHICSsupported = false; is_PRESENTATIONsupported = false; is_COMPUTEsupported = false; is_TRANSFERsupported = false;
	}
	inline queueflag_vk(const queueflag_vk& copy) {
		doesntNeedAnything = copy.doesntNeedAnything; is_GRAPHICSsupported = copy.is_GRAPHICSsupported; is_PRESENTATIONsupported = copy.is_PRESENTATIONsupported; is_COMPUTEsupported = copy.is_COMPUTEsupported; is_TRANSFERsupported = copy.is_TRANSFERsupported;
	}
	inline static queueflag_vk CreateInvalidNullFlag() {	//Returned flag's every bit is false. You should set at least one of them as true.
		return queueflag_vk();
	}
	inline bool isFlagValid() const {
		if (doesntNeedAnything && (is_GRAPHICSsupported || is_COMPUTEsupported || is_PRESENTATIONsupported || is_TRANSFERsupported)) {
			printer(result_tgfx_FAIL, "(This flag doesn't need anything but it also needs something, this shouldn't happen!");
			return false;
		}
		if (!doesntNeedAnything && !is_GRAPHICSsupported && !is_COMPUTEsupported && !is_PRESENTATIONsupported && !is_TRANSFERsupported) {
			printer(result_tgfx_FAIL, "(This flag needs something but it doesn't support anything");
			return false;
		}
		return true;
	}
};
struct queue_vk {
	fence_vk* RenderGraphFences[2];
	VkQueue Queue = VK_NULL_HANDLE;
};
struct queuefam_vk {
	uint32_t queueFamIndex = 0;
	unsigned int queuecount = 0, featurescore = 0;
	queueflag_vk supportflag;
	queue_vk* queues = nullptr;
	commandpool_vk CommandPools[2];
};
struct queuesys_data {
	std::vector<queuefam_vk*> queuefams;
};

struct memoryblock_vk {
	unsigned int MemAllocIndex = UINT32_MAX;
	VkDeviceSize Offset;
};
struct texture_vk {
	unsigned int WIDTH, HEIGHT, DATA_SIZE;
	unsigned char MIPCOUNT;
	VkFormat CHANNELs;
	VkImageUsageFlags USAGE;
	memoryblock_vk Block;

	VkImage Image = {};
	VkImageView ImageView = {};
};
struct framebuffer_vk {

};

void ThrowIfFailed(VkResult result, const char* text = "UNDEF THROW by VK");

#endif