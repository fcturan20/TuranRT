#pragma once

typedef struct allocator_sys_tapi;
extern allocator_sys_tapi* allocatorSys;
typedef struct tgfx_core   core_tgfx;
extern tgfx_core*          tgfx;
typedef struct filesys_tapi_type;
extern filesys_tapi_type* filesys;
typedef struct profiler_tapi_type;
extern profiler_tapi_type*         profilerSys;
typedef struct tgfx_renderer       renderer_tgfx;
extern tgfx_renderer*              renderer;
typedef struct tgfx_gpudatamanager gpudatamanager_tgfx;
extern tgfx_gpudatamanager*        contentManager;
typedef struct tgfx_gpu_obj*       gpu_tgfxhnd;
extern gpu_tgfxhnd                 gpu;
typedef struct tapi_ecs            ecs_tapi;
extern ecs_tapi*                   editorECS;

enum class result_editor { SUCCESS, CRASH, FAIL };
void printer_editor(result_editor result, const char* log);