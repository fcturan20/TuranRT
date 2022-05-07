#define VK_BACKEND
#include "renderer.h"
using namespace backend_vk_private;
renderer_vk renderer;


static std::vector<VkFence> CBsignal_Fences[2];
static std::vector<VkSemaphore> CBsignal_toPresent_Semaphores[2];	//This is not all semaphores in the frame, just the ones present is gonna be waiting for
static VkSemaphore SWPCHNsignal_semaphore[2];
static VkFence SWPCHNsignal_fence[2];
struct hidden_renderervk {
	uint64_t FrameIndex = 0;
};
static hidden_renderervk hidden;

unsigned char renderer_vk::get_frameindex() { return hidden.FrameIndex % 2; }
void Create_Renderer() {
	//Create signal fences for swapchain
	for (unsigned int i = 0; i < 2; i++) {
		VkFenceCreateInfo ci = {};
		ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		ci.pNext = nullptr;
		ThrowIfFailed(vkCreateFence(logicaldevice, &ci, nullptr, &SWPCHNsignal_fence[i]), "SWPCHN FENCE CREATION ERROR!");
	}
	//Create wait fences for swapchain
	for (unsigned int i = 0; i < 2; i++) {
		VkSemaphoreCreateInfo ci = {};
		ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		ci.flags = 0;
		ci.pNext = nullptr;
		ThrowIfFailed(vkCreateSemaphore(logicaldevice, &ci, nullptr, &SWPCHNsignal_semaphore[i]), "SWPCHN SEMAPHORE CREATION ERROR!");
	}
}


void renderer_Run() {
	unsigned char FrameIndex = renderer.get_frameindex();
	uint32_t presented_image_i = static_cast<uint32_t>(FrameIndex);

	//Wait for penultimate frame's command buffers
	if (CBsignal_Fences[FrameIndex].size()) {
		vkWaitForFences(logicaldevice, CBsignal_Fences[FrameIndex].size(), CBsignal_Fences[FrameIndex].data(), VK_TRUE, UINT64_MAX);
	}


	vkAcquireNextImageKHR(logicaldevice, window_swapchain, UINT64_MAX, SWPCHNsignal_semaphore[FrameIndex], SWPCHNsignal_fence[FrameIndex], &presented_image_i);
	vkWaitForFences(logicaldevice, 1, &SWPCHNsignal_fence[FrameIndex], VK_TRUE, UINT64_MAX);


	VkResult swpchn_result;
	VkPresentInfoKHR pi;
	pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	pi.pNext = nullptr;
	pi.swapchainCount = 1;
	pi.pSwapchains = &window_swapchain;
	pi.pImageIndices = &presented_image_i;
	pi.waitSemaphoreCount = CBsignal_toPresent_Semaphores[FrameIndex].size();
	pi.pWaitSemaphores = CBsignal_toPresent_Semaphores[FrameIndex].data();
	pi.pResults = &swpchn_result;

	ThrowIfFailed(vkQueuePresentKHR(presentationqueue->Queue, &pi), "SWPCHN PRESENT FAIL");

	hidden.FrameIndex++;
}


void renderer_vk::Create_RenderPass(framebuffer_id fb, renderpass_id* rp) {

}
void renderer_vk::Begin_CommandBuffer(commandbufer_id* cb_id) {
	 
}
void renderer_vk::Begin_RenderPass(renderpass_id rp) {

}
void renderer_vk::Render_IMGUI() {

}
void renderer_vk::End_RenderPass() {

}
void renderer_vk::End_CommandBuffer() {

}