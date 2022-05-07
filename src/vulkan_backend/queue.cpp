#define VK_BACKEND
#include "predefinitions_vk.h"
#include <mutex>
#include <string>
using namespace backend_vk_private;



void analize_queues() {
	static constexpr unsigned char max_propertiescount = 10;
	bool is_presentationfound = false;
	queuefams.resize(queuefam_props.size());
	VkQueueFamilyProperties properties[max_propertiescount]; uint32_t dontuse;
#ifdef VULKAN_DEBUGGING
	if (max_propertiescount < queuefam_props.size()) { printer(result_tgfx_FAIL, "VK backend's queue analization process should support more queue families! Please report this!"); }
#endif
	vkGetPhysicalDeviceQueueFamilyProperties(physicaldevice, &dontuse, properties);
	for (unsigned int queuefamily_index = 0; queuefamily_index < queuefam_props.size(); queuefamily_index++) {
		VkQueueFamilyProperties* QueueFamily = &properties[queuefamily_index];
		queuefam_vk* queuefam = new queuefam_vk;


		queuefam->queueFamIndex = static_cast<uint32_t>(queuefamily_index);
		if (QueueFamily->queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			queuefam->supportflag.is_GRAPHICSsupported = true;
			queuefam->featurescore++;
		}
		if (QueueFamily->queueFlags & VK_QUEUE_COMPUTE_BIT) {
			queuefam->supportflag.is_COMPUTEsupported = true;
			queuefam->featurescore++;
		}
		if (QueueFamily->queueFlags & VK_QUEUE_TRANSFER_BIT) {
			queuefam->supportflag.is_TRANSFERsupported = true;
			queuefam->featurescore++;
		}
		queuefam->queuecount = QueueFamily->queueCount;
		queuefam->queues = new queue_vk[queuefam->queuecount];
		queuefams[queuefamily_index] = queuefam;
	}
	/*
	//Sort the queue families by their feature score (Example: Element 0 is Transfer Only, Element 1 is Transfer-Compute, Element 2 is Graphics-Transfer-Compute etc)
	//QuickSort Algorithm
	if (queuefam_props.size()) {
		bool should_Sort = true;
		while (should_Sort) {
			should_Sort = false;
			for (unsigned char i = 0; i < queuefam_props.size() - 1; i++) {
				if (queuefams[i + 1]->featurescore < queuefams[i]->featurescore) {
					should_Sort = true;
					queuefam_vk* secondqueuefam = queuefams[i + 1];
					queuefams[i + 1] = queuefams[i];
					queuefams[i] = secondqueuefam;
				}
			}
		}
	}*/
}




void get_queue_objects() {
	printer(result_tgfx_SUCCESS, "After vkGetDeviceQueue()");
}
commandbuffer_vk* get_commandbuffer(queuefam_vk* family, unsigned char FrameIndex) {
	commandpool_vk& CP = family->CommandPools[FrameIndex];

	VkCommandBufferAllocateInfo cb_ai = {};
	cb_ai.commandBufferCount = 1;
	cb_ai.commandPool = CP.CPHandle;
	cb_ai.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cb_ai.pNext = nullptr;
	cb_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

	commandbuffer_vk* VK_CB = new commandbuffer_vk;
	if (vkAllocateCommandBuffers(logicaldevice, &cb_ai, &VK_CB->CB) != VK_SUCCESS) {
		printer(result_tgfx_FAIL, "vkAllocateCommandBuffers() failed while creating command buffers for RGBranches, report this please!");
		return nullptr;
	}
	VK_CB->is_Used = false;

	CP.CBs.push_back(VK_CB);

	return VK_CB;
}
VkCommandBuffer get_commandbufferobj(commandbuffer_vk* id) {
	return id->CB;
}
bool does_queuefamily_support(queuefam_vk* family, const queueflag_vk& flag) {
	printer(result_tgfx_NOTCODED, "does_queuefamily_support() isn't coded");
	return false;
}
VkQueue get_queue(queuefam_vk* queuefam) {
	return queuefam->queues[0].Queue;
}