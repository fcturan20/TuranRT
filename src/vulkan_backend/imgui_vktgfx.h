#pragma once
#include "predefinitions_vk.h"


//This class is to manage windows easier
typedef struct imguiwindow_tgfx {
	unsigned char isWindowOpen;
	const char* WindowName;
	void (*RunWindow)(imguiwindow_tgfx* windowdata);
	void* userdata;
} imguiwindow_tgfx;



struct vkimgui {
	//WINDOW MANAGEMENT

	static void Run_IMGUI_WINDOWs();
	static void Register_WINDOW(imguiwindow_tgfx* WINDOW);
	static void Delete_WINDOW(imguiwindow_tgfx* WINDOW);


	static unsigned char Check_IMGUI_Version();
	static void Destroy_IMGUI_Resources();
	static void Set_as_MainViewport();

	static unsigned char Show_DemoWindow();
	static unsigned char Show_MetricsWindow();

	//IMGUI FUNCTIONALITY!

	static unsigned char Create_Window(const char* title, unsigned char* should_close, unsigned char has_menubar);
	static void End_Window();
	static void Text(const char* text);
	static unsigned char Button(const char* button_name);
	static unsigned char Checkbox(const char* name, unsigned char* variable);
	//This is not const char*, because dear ImGui needs a buffer to set a char!
	static unsigned char Input_Text(const char* name, char** Text);
	//Create a menubar for a IMGUI window!
	static unsigned char Begin_Menubar();
	static void End_Menubar();
	//Create a menu button! Returns if it is clicked!
	static unsigned char Begin_Menu(const char* name);
	static void End_Menu();
	//Create a item for a menu! 
	static unsigned char Menu_Item(const char* name);
	//Write a paragraph text!
	static unsigned char Input_Paragraph_Text(const char* name, char** Text);
	//Put the next item to the same line with last created item
	//Use between after last item's end - before next item's begin!
	static void Same_Line();
	static unsigned char Begin_Tree(const char* name);
	static void End_Tree();
	//Create a select list that extends when clicked and get the selected_index in one-line of code!
	//Returns if any item is selected in the list! Selected item's index is the selected_index's pointer's value!
	static unsigned char SelectList_OneLine(const char* name, unsigned int* selected_index, const char* const* item_names);
	static void Selectable(const char* name, unsigned char* is_selected);
	//Create a box of selectable items in one-line of code!
	//Returns if any item is selected in the list! Selected item's index is the selected_index's pointer's value!
	static unsigned char Selectable_ListBox(const char* name, int* selected_index, const char* const* item_names);
	//Display a texture that is in the GPU memory, for example a Render Target or a Texture
	static void Display_Texture(texture_tgfx_handle TextureHandle, unsigned int Display_WIDTH, unsigned int Display_HEIGHT, unsigned char should_Flip_Vertically);
	static unsigned char Begin_TabBar();
	static void End_TabBar();
	static unsigned char Begin_TabItem(const char* name);
	static void End_TabItem();
	static void Separator();
	static glm::vec2 GetLastItemRectMin();
	static glm::vec2 GetLastItemRectMax();
	static glm::vec2 GetItemWindowPos();
	static glm::vec2 GetMouseWindowPos();

	//Add here Unsigned Int, Unsigned Short & Short, Unsigned Char & Char sliders too!

	static unsigned char Slider_Int(const char* name, int* data, int min, int max);
	static unsigned char Slider_Float(const char* name, float* data, float min, float max);
	static unsigned char Slider_Vec2(const char* name, glm::vec2* data, float min, float max);
	static unsigned char Slider_Vec3(const char* name, glm::vec3* data, float min, float max);
	static unsigned char Slider_Vec4(const char* name, glm::vec4* data, float min, float max);
};
