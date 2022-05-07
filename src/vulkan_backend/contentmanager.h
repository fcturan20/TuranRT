#pragma once
#include "predefinitions_vk.h"

struct attachmentdesc {
	texture_tgfx_handle textures[2];
	VkAttachmentLoadOp op;
};
struct gpucontentmanager {
	static void create_framebuffer(const std::vector<attachmentdesc>& attachments, framebuffer_id* created_fb);

};

extern gpucontentmanager contentmanager;;