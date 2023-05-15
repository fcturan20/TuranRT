#pragma once

typedef struct allocator_sys_tapi;
extern allocator_sys_tapi* allocatorSys;
typedef struct tgfx_core   core_tgfx;
extern tgfx_core*          tgfx;
typedef struct filesys_tapi;
extern filesys_tapi* fileSys;
typedef struct profiler_tapi* tapi_profiler;
extern tapi_profiler              profilerSys;
typedef struct tgfx_renderer       renderer_tgfx;
extern tgfx_renderer*              renderer;
typedef struct tgfx_gpudatamanager gpudatamanager_tgfx;
extern tgfx_gpudatamanager*        contentManager;
typedef struct tgfx_gpu_obj*       gpu_tgfxhnd;
extern gpu_tgfxhnd                 gpu;
typedef struct tapi_ecs            ecs_tapi;
extern ecs_tapi*                   editorECS;
typedef struct logger_tapi*        tapi_logger;
extern tapi_logger                 logSys;
typedef struct stringSys_tapi      tapi_stringSys;
extern tapi_stringSys*             stringSys;

enum class result_editor { SUCCESS, CRASH, FAIL };
void printer_editor(result_editor result, const char* log);


typedef struct mat4_rt {
  float v[16];
} rtMat4;

#ifndef RT_STATIC_CONSTEXPR_DEFINITIONS
#define uint32_sc static constexpr uint32_t
#define uint16_c static constexpr uint16_t
#define char_c static constexpr const char*
#define length_c(constexprarray) sizeof(constexprarray) / sizeof(constexprarray[0])
#endif // RT_STATIC_CONSTEXPR_DEFINITIONS