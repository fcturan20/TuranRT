#pragma once

extern const struct tapi_allocatorSys*    allocatorSys;
extern const struct tapi_bitsetSys*                                bitsetSys;
extern const struct tapi_fileSys*                                  fileSys;
extern const struct tapi_ecs*                                      editorECS;
extern const struct tapi_logger*                                   logSys;
extern const struct tapi_stringSys*                                stringSys;
extern const struct tapi_virtualMemorySys*                         virmemSys;
extern const struct tgfx_core*                                     tgfx;
extern const struct tapi_profiler*         profilerSys;
extern const struct tgfx_renderer*         tgfxRenderer;
extern const struct tgfx_gpuDataManager*   contentManager;
extern struct tgfx_gpu*              gpu;

enum class result_editor { SUCCESS, CRASH, FAIL };
void printer_editor(result_editor result, const char* log);

extern "C" struct rtMat4 {
  float v[16];
};

#ifndef RT_STATIC_CONSTEXPR_DEFINITIONS
#define uint32_sc static constexpr uint32_t
#define uint16_sc static constexpr uint16_t
#define char_sc static constexpr const char*
#define length_c(constexprarray) sizeof(constexprarray) / sizeof(constexprarray[0])
#endif // RT_STATIC_CONSTEXPR_DEFINITIONS