// C headers
#define T_INCLUDE_PLATFORM_LIBS
#include <assert.h>
#include <stdio.h>

// Cpp headers
#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/common.hpp>
#include <random>
#include <string>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// TuranLibraries headers
#include "unittestsys_tapi.h"
#include "TuranLibraries/editor/main.h"
#include "TuranLibraries/editor/pecfManager/pecfManager.h"
#include "allocator_tapi.h"
#include "string_tapi.h"
#include "ecs_tapi.h"
#include "filesys_tapi.h"
#include "logger_tapi.h"
#include "profiler_tapi.h"
#include "threadingsys_tapi.h"
#include "bitset_tapi.h"
#include "tgfx_core.h"

// TGFX headers
#include <tgfx_forwarddeclarations.h>

// RTEditor headers
#include "editor_includes.h"
#include "resourceSys/resourceManager.h"
#include "render_context/rendercontext.h"
#include "resourceSys/mesh.h"
#include "resourceSys/scene.h"
#include "systems/input.h"
#include "systems/camera.h"

unittestsys_tapi*   unitTestSys  = {};
allocator_sys_tapi* allocatorSys = {};
tgfx_core*          tgfx         = {};
filesys_tapi*       fileSys      = {};
tapi_profiler       profilerSys  = {};
tapi_stringSys*     stringSys    = nullptr;

static constexpr const char* pluginNames[]{
  UNITTEST_TAPI_PLUGIN_NAME, THREADINGSYS_TAPI_PLUGIN_NAME, STRINGSYS_TAPI_PLUGIN_NAME,
  PROFILER_TAPI_PLUGIN_NAME, FILESYS_TAPI_PLUGIN_NAME,      LOGGER_TAPI_PLUGIN_NAME,
  BITSET_TAPI_PLUGIN_NAME,   ALLOCATOR_TAPI_PLUGIN_NAME,    TGFX_PLUGIN_NAME};

void load_plugins() {
  for (uint32_t i = 0; i < sizeof(pluginNames) / sizeof(pluginNames[0]); i++) {
    pluginHnd_ecstapi pluginHnd = editorECS->loadPlugin(pluginNames[i]);
    if (!pluginHnd) {
      printf("Failed to load plugin %s\n", pluginNames[i]);
    }
    if (!editorECS->getSystem(pluginNames[i])) {
      printf("%s plugin is loaded but system isn't!", pluginNames[i]);
    }
  }
#define getSystemPtrRT(name) (( name##_PLUGIN_LOAD_TYPE )editorECS->getSystem(name##_PLUGIN_NAME))

  stringSys   = getSystemPtrRT(STRINGSYS_TAPI)->standardString;
  profilerSys = getSystemPtrRT(PROFILER_TAPI)->funcs;
  logSys      = getSystemPtrRT(LOGGER_TAPI)->funcs;
  logSys->init(string_type_tapi_UTF8, "mainLog.txt");
  allocatorSys = getSystemPtrRT(ALLOCATOR_TAPI);
  fileSys      = getSystemPtrRT(FILESYS_TAPI)->funcs;
  tgfx         = getSystemPtrRT(TGFX)->api;
}

void load_systems() {
  load_plugins();

  MM_initializeManager();
  SM_initializeManager();
  rtRenderer::initialize(rtInputSystem::getCallback());

  uint64_t            resourceCount  = {};
  struct rtResource** firstResources = RM_importFile( // SOURCE_DIR "Content/cube.glb"
    SOURCE_DIR L"Content/Gun.glb"
    //"D:\\Desktop\\Meshes\\Bakery\\scene.gltf"
    ,
    &resourceCount);
  struct rtScene*     firstScene     = {};
  for (uint32_t resourceIndx = 0; resourceIndx < resourceCount; resourceIndx++) {
    const rtResourceManagerType* type   = {};
    void*                        resHnd = RM_getResourceHnd(firstResources[resourceIndx], &type);
    if (type == SM_managerType()) {
      firstScene = ( struct rtScene* )resHnd;
    }
  }

  rtCamera cam = rtCameraController::createCamera(true);
  rtCameraController::setActiveCamera(cam);
  tgfx_vec2 res = {rtRenderer::getResolution().x, rtRenderer::getResolution().y};
  rtCameraController::setCameraProps(cam, &res);

  unsigned int i        = 0;
  uint64_t     duration = {};
  while (++i && i < 10000) {
    profiledscope_handle_tapi frameScope = {};
    profilerSys->start_profiling(&frameScope, "Frame Duration", &duration, 1);
    rtInputSystem::update();
    rtCameraController::update();
    rtRenderer::getSwapchainTexture();

    MM_frame();
    SM_renderScene(firstScene);
    rtRenderer::renderFrame();
    profilerSys->finish_profiling(&frameScope);
    logSys->log(log_type_tapi_STATUS, false, L"Frame #%u & Duration %lu microseconds", i, duration);
  }

  rtRenderer::close();
}

#include "ecs_tapi.h"

ecs_tapi*   editorECS = nullptr;
tapi_logger logSys    = nullptr;
extern void initialize_pecfManager();
extern void load_systems();

int main() {
  auto ecs_tapi_dll = DLIB_LOAD_TAPI("tapi_ecs.dll");
  if (!ecs_tapi_dll) {
    printf("There is no tapi_ecs.dll, initialization failed!");
    exit(-1);
  }
  load_ecstapi_func ecsloader =
    ( load_ecstapi_func )DLIB_FUNC_LOAD_TAPI(ecs_tapi_dll, "load_ecstapi");
  if (!ecsloader) {
    printf("tapi_ecs.dll is loaded but ecsloader func isn't found!");
    exit(-1);
  }
  editorECS = ecsloader();
  if (!editorECS) {
    printf("ECS initialization failed!");
    exit(-1);
  }

  // initialize_pecfManager();
  load_systems();

  return 1;
}