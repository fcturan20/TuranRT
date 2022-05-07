#include <vulkan_backend/vk_core.h>
#include <vulkan_backend/renderer.h>
#include "editor_windows/editor_windows.h"



int main(){
    core_vk::initialize();

    MainWindow::initialize();


    while (true) {
        commandbufer_id first_cb;
        renderer.Begin_CommandBuffer(&first_cb);

        renderer.End_CommandBuffer();
        core_vk::run();
    }



    return 0;
}


#include <iostream>
void printer_editor(result_editor result, const char* log) {
    printf(log);
    if (result != result_editor::SUCCESS) { printf("Enter 1 to continue"); int i = 0; while (i != 1) { std::cin >> i; } }
}