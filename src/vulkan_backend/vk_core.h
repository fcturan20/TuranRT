#pragma once
#include "predefinitions_vk.h"



struct core_vk {
	//Invalid handle is used to terminate array of i/o. NULL is used to say i/o is a failed handle and ignored if possible.
	static void* INVALIDHANDLE;


	static void initialize();
	static void run();


	//Destroy GFX API systems to create again (with a different Graphics API maybe?) or close application
	//This will close all of the systems with "GFX" prefix and you shouldn't call them either
	static void destroy_tgfx_resources();
};