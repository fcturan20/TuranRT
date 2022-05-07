#pragma once
#include "predefinitions_vk.h"

struct renderer_vk {
	unsigned char get_frameindex();

	//Subpasses aren't supported for now
	void Create_RenderPass(framebuffer_id fb, renderpass_id* rp);
	void Begin_CommandBuffer(commandbufer_id* cb_id);
	void Begin_RenderPass(renderpass_id rp);
	void Render_IMGUI();
	void End_RenderPass();
	void End_CommandBuffer();
};

extern renderer_vk renderer;
