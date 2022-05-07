#define VK_BACKEND
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include "imgui_vktgfx.h"
using namespace backend_vk_private;


#include <vector>
#include <string>



unsigned char vkimgui::Slider_Int(const char* name, int* data, int min, int max) { return ImGui::SliderInt(name, data, min, max); }
unsigned char vkimgui::Slider_Float(const char* name, float* data, float min, float max) { return ImGui::SliderFloat(name, data, min, max); }
unsigned char vkimgui::Slider_Vec2(const char* name, glm::vec2* data, float min, float max) { return ImGui::SliderFloat2(name, (float*)data, min, max); }
unsigned char vkimgui::Slider_Vec3(const char* name, glm::vec3* data, float min, float max) { return ImGui::SliderFloat3(name, (float*)data, min, max); }
unsigned char vkimgui::Slider_Vec4(const char* name, glm::vec4* data, float min, float max) { return ImGui::SliderFloat4(name, (float*)data, min, max); }

struct imgui_vk_hidden {
	ImGuiContext* Context = nullptr;
	enum IMGUI_STATUS {
		UNINITIALIZED = 0,
		INITIALIZED = 1,
		NEW_FRAME = 2,
		RENDERED = 3
	};
	IMGUI_STATUS STAT;
	std::vector<imguiwindow_tgfx*> ALL_IMGUI_WINDOWs, Windows_toClose, Windows_toOpen;
};
static imgui_vk_hidden* hidden = nullptr;

void Create_IMGUI() {
	hidden = new imgui_vk_hidden;
	hidden->STAT = imgui_vk_hidden::UNINITIALIZED;


	//Create Context here!
	IMGUI_CHECKVERSION();
	hidden->Context = ImGui::CreateContext();
	if (hidden->Context == nullptr) {
		printer(result_tgfx_FAIL, "dear ImGui Context is nullptr after creation!");
		delete hidden;
		hidden = nullptr;
		return;
	}

	//Set Input Handling settings here! 
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;


	//Set color style to dark by default for now!
	ImGui::StyleColorsDark();
}
void Render_toCB(VkCommandBuffer cb) {
	if (hidden->STAT == imgui_vk_hidden::RENDERED) { return; }
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cb);
	hidden->STAT = imgui_vk_hidden::RENDERED;
}

unsigned char vkimgui::Check_IMGUI_Version() {
	//Check version here, I don't care for now!
	return IMGUI_CHECKVERSION();
}

void vkimgui::Set_as_MainViewport() {
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
	ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
}

void vkimgui::Destroy_IMGUI_Resources() {
	printer(result_tgfx_SUCCESS, "IMGUI resources are being destroyed!\n");
	ImGui::DestroyContext();
}

unsigned char vkimgui::Show_DemoWindow() {
	bool x = true;
	ImGui::ShowDemoWindow(&x);
	return x;
}

unsigned char vkimgui::Show_MetricsWindow() {
	bool x = true;
	ImGui::ShowMetricsWindow(&x);
	return x;
}


unsigned char vkimgui::Create_Window(const char* title, unsigned char* should_close, unsigned char has_menubar) {
	bool close_boolean = should_close;
	ImGuiWindowFlags window_flags = 0;
	window_flags |= (has_menubar ? ImGuiWindowFlags_MenuBar : 0);
	bool result = ImGui::Begin(title, &close_boolean, window_flags);
	*should_close = close_boolean;
	return result;
}

void vkimgui::End_Window() {
	ImGui::End();
}

void vkimgui::Text(const char* text) {
	ImGui::Text(text);
}

unsigned char vkimgui::Button(const char* button_name) {
	return ImGui::Button(button_name);
}

unsigned char vkimgui::Checkbox(const char* name, unsigned char* variable) {
	bool x = *variable;
	bool result = ImGui::Checkbox(name, &x);
	*variable = x;
	return result;
}

unsigned char vkimgui::Input_Text(const char* name, char** text) {
	printer(result_tgfx_NOTCODED, "Vulkan::tgfx_imguicore->Input_Text() is not coded!");
	/*
	if (ImGui::InputText(name, text, ImGuiInputTextFlags_EnterReturnsTrue)) {
		return true;
	}*/
	return false;
}

unsigned char vkimgui::Begin_Menubar() {
	return ImGui::BeginMenuBar();
}

void vkimgui::End_Menubar() {
	ImGui::EndMenuBar();
}

unsigned char vkimgui::Begin_Menu(const char* name) {
	return ImGui::BeginMenu(name);
}

void vkimgui::End_Menu() {
	ImGui::EndMenu();
}

unsigned char vkimgui::Menu_Item(const char* name) {
	return ImGui::MenuItem(name);
}

unsigned char vkimgui::Input_Paragraph_Text(const char* name, char** Text) {
	printer(result_tgfx_NOTCODED, "Vulkan::IMGUI::Input_Paragraph_Text() isn't coded!");
	/*
	if (ImGui::InputTextMultiline(name, Text, ImVec2(0, 0), ImGuiInputTextFlags_EnterReturnsTrue)) {
		return true;
	}*/
	return false;
}

//Puts the next item to the same line with last created item
//Use between after last item's end - before next item's begin!
void vkimgui::Same_Line() {
	ImGui::SameLine();
}

unsigned char vkimgui::Begin_Tree(const char* name) {
	return ImGui::TreeNode(name);
}

void vkimgui::End_Tree() {
	ImGui::TreePop();
}

unsigned char vkimgui::SelectList_OneLine(const char* name, unsigned int* selected_index, const char* const* item_names) {
	unsigned char is_new_item_selected = false;
	const char* preview_str = item_names[*selected_index];
	if (ImGui::BeginCombo(name, preview_str))	// The second parameter is the index of the label previewed before opening the combo.
	{
		unsigned int i = 0;
		while (item_names[i] != nullptr) {
			unsigned char is_selected = (*selected_index == i);
			if (ImGui::Selectable(item_names[i], is_selected)) {
				*selected_index = i;
				is_new_item_selected = true;
			}
			i++;
		}
		ImGui::EndCombo();
	}
	return is_new_item_selected;
}

//If selected, argument "is_selected" is set to its opposite!
void vkimgui::Selectable(const char* name, unsigned char* is_selected) {
	ImGui::Selectable(name, is_selected);
}

unsigned char vkimgui::Selectable_ListBox(const char* name, int* selected_index, const char* const* item_names) {
	int already_selected_index = *selected_index;
	unsigned char is_new_selected = false;
	if (ImGui::ListBoxHeader(name)) {
		unsigned int i = 0;
		while (item_names[i] != nullptr) {
			unsigned char is_selected = false;
			const char* item_name = item_names[i];
			Selectable(item_name, &is_selected);
			if (is_selected && (already_selected_index != i)) {
				*selected_index = i;
				is_new_selected = true;
			}
			i++;
		}

		ImGui::ListBoxFooter();
	}
	return is_new_selected;
}
/*
void CheckListBox(const char* name, Bitset items_status, const char* const* item_names) {
	if (ImGui::ListBoxHeader(name)) {
		unsigned int i = 0;
		while (item_names[i] != nullptr) {
			unsigned char x = GetBit_Value(items_status, i);
			Checkbox(item_names[i], &x);
			x ? SetBit_True(items_status, i) : SetBit_False(items_status, i);
			printer(result_tgfx_SUCCESS, ("Current Index: " + std::to_string(i)).c_str());
			printer(result_tgfx_SUCCESS, ("Current Name: " + std::string(item_names[i])).c_str());
			printer(result_tgfx_SUCCESS, ("Current Value: " + std::to_string(x)).c_str());
			i++;
		}
		ImGui::ListBoxFooter();
	}
}*/

unsigned char vkimgui::Begin_TabBar() {
	return ImGui::BeginTabBar("");
}
void vkimgui::End_TabBar() {
	ImGui::EndTabBar();
}
unsigned char vkimgui::Begin_TabItem(const char* name) {
	return ImGui::BeginTabItem(name);
}
void vkimgui::End_TabItem() {
	ImGui::EndTabItem();
}
void vkimgui::Separator() {
	ImGui::Separator();
}
glm::vec2 vkimgui::GetLastItemRectMin() {
	glm::vec2 vecvar;
	vecvar.x = ImGui::GetItemRectMin().x;
	vecvar.y = ImGui::GetItemRectMin().y;
	return vecvar;
}
glm::vec2 vkimgui::GetLastItemRectMax() {
	glm::vec2 vecvar;
	vecvar.x = ImGui::GetItemRectMax().x;
	vecvar.y = ImGui::GetItemRectMax().y;
	return vecvar;
}
glm::vec2 vkimgui::GetItemWindowPos() {
	glm::vec2 vecvar;
	vecvar.x = ImGui::GetCursorScreenPos().x;
	vecvar.y = ImGui::GetCursorScreenPos().y;
	return vecvar;
}
glm::vec2 vkimgui::GetMouseWindowPos() {
	glm::vec2 vecvar;
	vecvar.x = ImGui::GetMousePos().x;
	vecvar.y = ImGui::GetMousePos().y;
	return vecvar;
}



void vkimgui::Run_IMGUI_WINDOWs() {
	for (unsigned int i = 0; i < hidden->ALL_IMGUI_WINDOWs.size(); i++) {
		imguiwindow_tgfx* window = hidden->ALL_IMGUI_WINDOWs[i];
		printer(result_tgfx_SUCCESS, ("Running the Window: " + std::string(window->WindowName)).c_str());
		window->RunWindow(window);
	}

	if (hidden->Windows_toClose.size() > 0) {
		for (unsigned int window_delete_i = 0; window_delete_i < hidden->Windows_toClose.size(); window_delete_i++) {
			imguiwindow_tgfx* window_to_close = hidden->Windows_toClose[window_delete_i];
			for (unsigned int deleted_window_main_i = 0; deleted_window_main_i < hidden->ALL_IMGUI_WINDOWs.size(); deleted_window_main_i++) {
				if (window_to_close == hidden->ALL_IMGUI_WINDOWs[deleted_window_main_i]) {
					hidden->ALL_IMGUI_WINDOWs.erase(hidden->ALL_IMGUI_WINDOWs.begin() + deleted_window_main_i);
					//Inner loop will break because we found the deleted window in the main list!
					break;
				}
			}
		}

		hidden->Windows_toClose.clear();
	}

	if (hidden->Windows_toOpen.size() > 0) {
		unsigned int previous_size = hidden->ALL_IMGUI_WINDOWs.size();

		for (unsigned int i = 0; i < hidden->Windows_toOpen.size(); i++) {
			imguiwindow_tgfx* window_to_open = hidden->Windows_toOpen[i];
			hidden->ALL_IMGUI_WINDOWs.push_back(window_to_open);
		}
		hidden->Windows_toOpen.clear();
		//We should run new windows!
		for (previous_size; previous_size < hidden->ALL_IMGUI_WINDOWs.size(); previous_size++) {
			hidden->ALL_IMGUI_WINDOWs[previous_size]->RunWindow(hidden->ALL_IMGUI_WINDOWs[previous_size]);
		}
	}
}
void vkimgui::Register_WINDOW(imguiwindow_tgfx* Window) {
	printer(result_tgfx_SUCCESS, "Registering a Window!");
	hidden->Windows_toOpen.push_back(Window);
}
void vkimgui::Delete_WINDOW(imguiwindow_tgfx* Window) {
	hidden->Windows_toClose.push_back(Window);
}
