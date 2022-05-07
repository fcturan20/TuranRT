#define VK_BACKEND
#include "predefinitions_vk.h"
#include <atomic>
#include <numeric>
#include <string>
using namespace backend_vk_private;

extern VkBuffer Create_VkBuffer(unsigned int size, VkBufferUsageFlags usage) {
	VkBuffer buffer;

	VkBufferCreateInfo ci{};
	ci.usage = usage;
	if (queuefams.size() > 1) {
		ci.sharingMode = VK_SHARING_MODE_CONCURRENT;
	}
	else {
		ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}
	std::vector<uint32_t> indices(queuefams.size());
	for (unsigned int i = 0; i < queuefams.size(); i++) { indices[i] = i; }
	ci.queueFamilyIndexCount = indices.size();
	ci.pQueueFamilyIndices = indices.data();
	ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	ci.size = size;

	if (vkCreateBuffer(logicaldevice, &ci, nullptr, &buffer) != VK_SUCCESS) {
		printer(result_tgfx_FAIL, "Create_VkBuffer has failed!");
	}
	return buffer;
}
struct suballocation_vk {
	VkDeviceSize Size = 0, Offset = 0;
	std::atomic<bool> isEmpty;
	suballocation_vk() : isEmpty(true) {}
	suballocation_vk(const suballocation_vk& copyblock) : isEmpty(copyblock.isEmpty.load()), Size(copyblock.Size), Offset(copyblock.Offset) {}
	suballocation_vk operator=(const suballocation_vk& copyblock) {
		Size = copyblock.Size;
		Offset = copyblock.Offset;
		isEmpty.store(copyblock.isEmpty.load());
		return *this;
	}
};
struct memoryHeap_vk {

};

//Each memory type has its own buffers
struct memorytype_vk {
	VkDeviceMemory Allocated_Memory;
	VkBuffer Buffer;
	std::atomic<uint32_t> UnusedSize = 0;
	uint32_t MemoryTypeIndex;	//Vulkan's index
	VkDeviceSize MaxSize = 0, ALLOCATIONSIZE = 0;
	void* MappedMemory = nullptr;
	memoryallocationtype_tgfx TYPE;
	threadlocal_vector<suballocation_vk> Allocated_Blocks;
	memorytype_vk() = default;
	memorytype_vk(const memorytype_vk& copy) : Allocated_Blocks(copy.Allocated_Blocks) {
		Allocated_Memory = copy.Allocated_Memory;
		Buffer = copy.Buffer;
		UnusedSize.store(UnusedSize.load());
		MemoryTypeIndex = copy.MemoryTypeIndex;
		MaxSize = copy.MaxSize;
		ALLOCATIONSIZE = copy.ALLOCATIONSIZE;
		MappedMemory = copy.MappedMemory;
	}
	inline VkDeviceSize FindAvailableOffset(VkDeviceSize RequiredSize, VkDeviceSize AlignmentOffset, VkDeviceSize RequiredAlignment) {
		VkDeviceSize FurthestOffset = 0;
		if (AlignmentOffset && !RequiredAlignment) {
			RequiredAlignment = AlignmentOffset;
		}
		else if (!AlignmentOffset && RequiredAlignment) {
			AlignmentOffset = RequiredAlignment;
		}
		else if (!AlignmentOffset && !RequiredAlignment) {
			AlignmentOffset = 1;
			RequiredAlignment = 1;
		}
		for (unsigned int ThreadID = 0; ThreadID < thread_count; ThreadID++) {
			for (unsigned int i = 0; i < Allocated_Blocks.size(ThreadID); i++) {
				suballocation_vk& Block = Allocated_Blocks.get(ThreadID, i);
				if (FurthestOffset <= Block.Offset) {
					FurthestOffset = Block.Offset + Block.Size;
				}
				if (!Block.isEmpty.load()) {
					continue;
				}

				VkDeviceSize Offset = CalculateOffset(Block.Offset, AlignmentOffset, RequiredAlignment);

				if (Offset + RequiredSize - Block.Offset > Block.Size ||
					Offset + RequiredSize - Block.Offset < (Block.Size / 5) * 3) {
					continue;
				}
				bool x = true, y = false;
				//Try to get the block first (Concurrent usages are prevented that way)
				if (!Block.isEmpty.compare_exchange_strong(x, y)) {
					continue;
				}
				//Don't change the block's own offset, because that'd probably cause shifting offset the memory block after free-reuse-free-reuse sequences
				return Offset;
			}
		}

		VkDeviceSize finaloffset = CalculateOffset(FurthestOffset, AlignmentOffset, RequiredAlignment);
		if (finaloffset + RequiredSize > ALLOCATIONSIZE) {
			printer(result_tgfx_FAIL, "Suballocation failed because memory type has not enough space for the data!");
			return UINT64_MAX;
		}
		//None of the current blocks is suitable, so create a new block in this thread's local memoryblocks list
		suballocation_vk newblock;
		newblock.isEmpty.store(false);
		newblock.Offset = finaloffset;
		newblock.Size = RequiredSize + newblock.Offset - FurthestOffset;
		Allocated_Blocks.push_back(newblock);
		return newblock.Offset;
	}
private:
	//Don't use this functions outside of the FindAvailableOffset
	inline VkDeviceSize CalculateOffset(VkDeviceSize baseoffset, VkDeviceSize AlignmentOffset, VkDeviceSize ReqAlignment) {
		VkDeviceSize FinalOffset = 0;
		VkDeviceSize LCM = std::lcm(AlignmentOffset, ReqAlignment);
		FinalOffset = (baseoffset % LCM) ? (((baseoffset / LCM) + 1) * LCM) : baseoffset;
		return FinalOffset;
	}
};
static std::vector<memorytype_vk> memorytypes;


//Returns the given offset
//RequiredSize: You should use vkGetBuffer/ImageRequirements' output size here
//AligmentOffset: You should use GPU's own aligment offset limitation for the specified type of data
//RequiredAlignment: You should use vkGetBuffer/ImageRequirements' output alignment here
inline result_tgfx suballocate_memoryblock(unsigned int memoryid, VkDeviceSize RequiredSize, VkDeviceSize AlignmentOffset, VkDeviceSize RequiredAlignment, VkDeviceSize* ResultOffset) {
	memorytype_vk& memtype = memorytypes[memoryid];
	*ResultOffset = memtype.FindAvailableOffset(RequiredSize, AlignmentOffset, RequiredAlignment);
	if (*ResultOffset == UINT64_MAX) {
		printer(result_tgfx_NOTCODED, "There is not enough space in the memory allocation, Vulkan backend should support multiple memory allocations");
		return result_tgfx_NOTCODED;
	}
	return result_tgfx_SUCCESS;
}

void analize_gpumemory() {
	memorytypes.resize(memory_props.memoryTypeCount);
	memdescs.resize(memory_props.memoryTypeCount);
	for (uint32_t MemoryTypeIndex = 0; MemoryTypeIndex < memory_props.memoryTypeCount; MemoryTypeIndex++) {
		VkMemoryType& MemoryType = memory_props.memoryTypes[MemoryTypeIndex];
		bool isDeviceLocal = false;
		bool isHostVisible = false;
		bool isHostCoherent = false;
		bool isHostCached = false;

		if ((MemoryType.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
			isDeviceLocal = true;
		}
		if ((MemoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
			isHostVisible = true;
		}
		if ((MemoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
			isHostCoherent = true;
		}
		if ((MemoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) == VK_MEMORY_PROPERTY_HOST_CACHED_BIT) {
			isHostCached = true;
		}


		if (!isDeviceLocal && !isHostVisible && !isHostCoherent && !isHostCached) {
			continue;
		}
		if (isDeviceLocal) {
			if (isHostVisible && isHostCoherent) {
				memory_description_tgfx& memtype_desc = memdescs[MemoryTypeIndex];
				memtype_desc.allocationtype = memoryallocationtype_FASTHOSTVISIBLE;
				memtype_desc.memorytype_id = MemoryTypeIndex;
				memtype_desc.max_allocationsize = memory_props.memoryHeaps[MemoryType.heapIndex].size;

				memorytype_vk& memtype = memorytypes[MemoryTypeIndex];
				memtype.MemoryTypeIndex = MemoryTypeIndex;
				memtype.TYPE = memoryallocationtype_FASTHOSTVISIBLE;
				memtype.MaxSize = memtype_desc.max_allocationsize;
				printer(result_tgfx_SUCCESS, ("Found FAST HOST VISIBLE BIT! Size: " + std::to_string(memtype_desc.allocationtype)).c_str());
			}
			else {
				memory_description_tgfx& memtype_desc = memdescs[MemoryTypeIndex];
				memtype_desc.allocationtype = memoryallocationtype_DEVICELOCAL;
				memtype_desc.memorytype_id = MemoryTypeIndex;
				memtype_desc.max_allocationsize = memory_props.memoryHeaps[MemoryType.heapIndex].size;

				memorytype_vk& memtype = memorytypes[MemoryTypeIndex];
				memtype.MemoryTypeIndex = MemoryTypeIndex;
				memtype.TYPE = memoryallocationtype_DEVICELOCAL;
				memtype.MaxSize = memtype_desc.max_allocationsize;
				printer(result_tgfx_SUCCESS, ("Found DEVICE LOCAL BIT! Size: " + std::to_string(memtype_desc.allocationtype)).c_str());
			}
		}
		else if (isHostVisible && isHostCoherent) {
			if (isHostCached) {
				memory_description_tgfx& memtype_desc = memdescs[MemoryTypeIndex];
				memtype_desc.allocationtype = memoryallocationtype_READBACK;
				memtype_desc.memorytype_id = MemoryTypeIndex;
				memtype_desc.max_allocationsize = memory_props.memoryHeaps[MemoryType.heapIndex].size;

				memorytype_vk& memtype = memorytypes[MemoryTypeIndex];
				memtype.MemoryTypeIndex = MemoryTypeIndex;
				memtype.TYPE = memoryallocationtype_READBACK;
				memtype.MaxSize = memtype_desc.max_allocationsize;
				printer(result_tgfx_SUCCESS, ("Found READBACK BIT! Size: " + std::to_string(memtype_desc.allocationtype)).c_str());
			}
			else {
				memory_description_tgfx& memtype_desc = memdescs[MemoryTypeIndex];
				memtype_desc.allocationtype = memoryallocationtype_HOSTVISIBLE;
				memtype_desc.memorytype_id = MemoryTypeIndex;
				memtype_desc.max_allocationsize = memory_props.memoryHeaps[MemoryType.heapIndex].size;

				memorytype_vk& memtype = memorytypes[MemoryTypeIndex];
				memtype.MemoryTypeIndex = MemoryTypeIndex;
				memtype.TYPE = memoryallocationtype_HOSTVISIBLE;
				memtype.MaxSize = memtype_desc.max_allocationsize;
				printer(result_tgfx_SUCCESS, ("Found HOST VISIBLE BIT! Size: " + std::to_string(memtype_desc.allocationtype)).c_str());
			}
		}
	}
}
void do_allocations() {
	for (unsigned int memorytype_i = 0; memorytype_i < memorytypes.size(); memorytype_i++) {
		memorytype_vk& memtype = memorytypes[memorytype_i];
		//Allocate 100MB memory from both device local and fast host visible memory
		if (memtype.TYPE == memoryallocationtype_DEVICELOCAL || memtype.TYPE == memoryallocationtype_FASTHOSTVISIBLE) {
			memtype.ALLOCATIONSIZE = 100 << 20;
		}
	}

	for (unsigned int allocindex = 0; allocindex < memorytypes.size(); allocindex++) {
		memorytype_vk& memtype = memorytypes[allocindex];
		if (!memtype.ALLOCATIONSIZE) {
			continue;
		}

		VkMemoryRequirements memrequirements;
		VkBufferUsageFlags USAGEFLAGs = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		uint64_t AllocSize = memtype.ALLOCATIONSIZE;
		VkBuffer GPULOCAL_buf = Create_VkBuffer(AllocSize, USAGEFLAGs);
		vkGetBufferMemoryRequirements(logicaldevice, GPULOCAL_buf, &memrequirements);
		if (!(memrequirements.memoryTypeBits & (1 << memtype.MemoryTypeIndex))) {
			printer(result_tgfx_FAIL, "GPU Local Memory Allocation doesn't support the MemoryType!");
			continue;
		}
		VkMemoryAllocateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		ci.allocationSize = memrequirements.size;
		ci.memoryTypeIndex = memtype.MemoryTypeIndex;
		VkDeviceMemory allocated_memory;
		auto allocatememorycode = vkAllocateMemory(logicaldevice, &ci, nullptr, &allocated_memory);
		if (allocatememorycode != VK_SUCCESS) {
			printer(result_tgfx_FAIL, ("vk_gpudatamanager initialization has failed because vkAllocateMemory has failed with code: " + std::to_string(allocatememorycode) + "!").c_str());
			continue;
		}
		memtype.Allocated_Memory = allocated_memory;
		memtype.UnusedSize.store(AllocSize);
		memtype.MappedMemory = nullptr;
		memtype.Buffer = GPULOCAL_buf;
		if (vkBindBufferMemory(logicaldevice, GPULOCAL_buf, allocated_memory, 0) != VK_SUCCESS) {
			printer(result_tgfx_FAIL, "Binding buffer to the allocated memory has failed!");
		}

		//If allocation is device local, it is not mappable. So continue.
		if (memtype.TYPE == memoryallocationtype_DEVICELOCAL) {
			continue;
		}

		if (vkMapMemory(logicaldevice, allocated_memory, 0, memrequirements.size, 0, &memtype.MappedMemory) != VK_SUCCESS) {
			printer(result_tgfx_FAIL, "Mapping the HOSTVISIBLE memory has failed!");
			continue;
		}
	}
}

