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

#include "tgfx_forwarddeclarations.h"

// RTEditor headers
#include "editor_includes.h"
#include "resourceSys/resourceManager.h"
#include "render_context/rendercontext.h"
#include "resourceSys/mesh.h"
#include "resourceSys/scene.h"
#include "systems/input.h"
#include "systems/camera.h"

// TuranLibraries headers
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

allocator_sys_tapi*           allocatorSys = {};
tgfx_core*                    tgfx         = {};
FILESYS_TAPI_PLUGIN_LOAD_TYPE fileSys      = {};
tapi_profiler                 profilerSys  = {};
tapi_stringSys*        stringSys = nullptr;

uint32_t findFirst(std::vector<bool>& stdBitset, bool isTrue) {
  for (uint32_t i = 0; i < stdBitset.size(); i++) {
    if (stdBitset[i] == isTrue) {
      return i;
    }
  }
  return UINT32_MAX;
}

void load_plugins() {
  pluginHnd_ecstapi threadingPlugin = editorECS->loadPlugin(THREADINGSYS_TAPI_PLUGIN_NAME ".dll");
  auto              threadingSys =
    ( THREADINGSYS_TAPI_PLUGIN_LOAD_TYPE )editorECS->getSystem(THREADINGSYS_TAPI_PLUGIN_NAME);

  pluginHnd_ecstapi stringPlugin = editorECS->loadPlugin(STRINGSYS_TAPI_PLUGIN_NAME ".dll");
  auto stringSysLoaded = ( STRINGSYS_TAPI_LOAD_TYPE )editorECS->getSystem(STRINGSYS_TAPI_PLUGIN_NAME);
  stringSys = stringSysLoaded->standardString;

  pluginHnd_ecstapi profilerPlugin = editorECS->loadPlugin(PROFILER_TAPI_PLUGIN_NAME ".dll");
  auto              profilerSysLoaded =
    ( PROFILER_TAPI_PLUGIN_LOAD_TYPE )editorECS->getSystem(PROFILER_TAPI_PLUGIN_NAME);
  profilerSys = profilerSysLoaded->funcs;

  pluginHnd_ecstapi filesysPlugin = editorECS->loadPlugin(FILESYS_TAPI_PLUGIN_NAME ".dll");
  fileSys = ( FILESYS_TAPI_PLUGIN_LOAD_TYPE )editorECS->getSystem(FILESYS_TAPI_PLUGIN_NAME);

  pluginHnd_ecstapi loggerPlugin = editorECS->loadPlugin(LOGGER_TAPI_PLUGIN_NAME ".dll");
  auto loggerSys = ( LOGGER_TAPI_PLUGIN_LOAD_TYPE )editorECS->getSystem(LOGGER_TAPI_PLUGIN_NAME);
  logSys         = loggerSys->funcs;
  logSys->init(string_type_tapi_UTF8, "mainLog.txt");

  pluginHnd_ecstapi bitsetPlugin = editorECS->loadPlugin(BITSET_TAPI_PLUGIN_NAME ".dll");
  auto bitsetSys = ( BITSET_TAPI_PLUGIN_LOAD_TYPE )editorECS->getSystem(BITSET_TAPI_PLUGIN_NAME);

  allocatorSys =
    ( ALLOCATOR_TAPI_PLUGIN_LOAD_TYPE )editorECS->getSystem(ALLOCATOR_TAPI_PLUGIN_NAME);

  {
    pluginHnd_ecstapi tgfxPlugin = editorECS->loadPlugin(TGFX_PLUGIN_NAME ".dll");
    auto              tgfxSys    = ( TGFX_PLUGIN_LOAD_TYPE )editorECS->getSystem(TGFX_PLUGIN_NAME);
    if (tgfxSys) {
      tgfx = tgfxSys->api;
    }
  }

  // TO-DO: Move this to tapi_bitset's itself as unit test
  {
    static constexpr uint32_t bitsetByteLength = 10 << 10;
    std::vector<bool>         stdBitset(bitsetByteLength * 8, false);
    bitset_tapi               tBitset = bitsetSys->funcs->createBitset(bitsetByteLength);

    time_t t;
    srand(( unsigned )time(&t));
    for (uint32_t i = 0; i < bitsetByteLength * 8; i++) {
      uint32_t bit   = rand() % (bitsetByteLength * 8);
      bool     v     = rand() % 2;
      stdBitset[bit] = v;
      bitsetSys->funcs->setBit(tBitset, bit, v);
    }
    if (findFirst(stdBitset, true) != bitsetSys->funcs->getFirstBitIndx(tBitset, true) ||
        findFirst(stdBitset, false) != bitsetSys->funcs->getFirstBitIndx(tBitset, false)) {
      printf("Firsts not match!");
      exit(-1);
    }
    for (uint32_t i = 0; i < bitsetByteLength * 8; i++) {
      unsigned char tapiV = bitsetSys->funcs->getBitValue(tBitset, i);
      unsigned char stdV  = stdBitset[i];
      if (stdV != tapiV) {
        printf("Index %d is not matching!\n", i);
      }
    }
  }
}

void load_systems() {
  load_plugins();

  rtRenderer::initialize(rtInputSystem::getCallback());
  rtMeshManager::initializeManager();
  rtSceneManager::initializeManager();

  uint64_t    resourceCount  = {};
  rtResource* firstResources = rtResourceManager::importAssimp( // SOURCE_DIR "Content/cube.glb"
    SOURCE_DIR "Content/gun.glb"
    //"D:\\Desktop\\Meshes\\Bakery\\scene.gltf"
    ,
    &resourceCount);
  rtScene     firstScene     = {};
  for (uint32_t resourceIndx = 0; resourceIndx < resourceCount; resourceIndx++) {
    rtResourceManagerType type = {};
    void* resHnd = rtResourceManager::getResourceHnd(firstResources[resourceIndx], type);
    if (type == rtSceneManager::managerType()) {
      firstScene = ( rtScene )resHnd;
    }
  }

  rtCamera cam = rtCameraController::createCamera(true);
  rtCameraController::setActiveCamera(cam);
  tgfx_vec2 res = {1920.0, 1080.0f};
  float     fov = 45.0f, nearPlane = 0.01f, farPlane = 100.0f, mouseSensitivity = 0.1f;
  rtCameraController::setCameraProps(cam, &res, &nearPlane, &farPlane, &mouseSensitivity, &fov);


  unsigned int i        = 0;
  uint64_t     duration = {};
  while (++i && i < 10000) {
    profiledscope_handle_tapi frameScope = {};
    profilerSys->start_profiling(&frameScope, "Frame Duration", &duration, 1);
    rtInputSystem::update();
    rtCameraController::update();
    rtRenderer::getSwapchainTexture();

    rtMeshManager::frame();
    rtSceneModifier::renderScene(firstScene);
    rtRenderer::renderFrame();
    profilerSys->finish_profiling(&frameScope);
    logSys->log(log_type_tapi_STATUS, false, L"Frame #%u", i);
    logSys->log(log_type_tapi_STATUS, false, L"Duration %u", duration);
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