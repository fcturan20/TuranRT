#include "editor_windows.h"
#include <stdlib.h>
#include <stdio.h>

#include <vulkan_backend/imgui_vktgfx.h>

void MainWindow_run(imguiwindow_tgfx* windowdata) {
	printf("run");
}
void MainWindow::initialize() {
	imguiwindow_tgfx* window = new imguiwindow_tgfx;
	window->isWindowOpen = true;
	window->RunWindow = &MainWindow_run;
	window->userdata = window;
	window->WindowName = "Main Window";
	vkimgui::Register_WINDOW(window);
	printf("Initialized main window");
}