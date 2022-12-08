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

// TuranLibraries headers
#include "TuranLibraries/editor/main.h"
#include "TuranLibraries/editor/pecfManager/pecfManager.h"
#include "allocator_tapi.h"
#include "array_of_strings_tapi.h"
#include "ecs_tapi.h"
#include "filesys_tapi.h"
#include "logger_tapi.h"
#include "profiler_tapi.h"
#include "threadingsys_tapi.h"
#include "tgfx_core.h"

allocator_sys_tapi*            allocatorSys   = {};
tgfx_core*                     tgfx           = {};
FILESYS_TAPI_PLUGIN_LOAD_TYPE  filesys        = {};
PROFILER_TAPI_PLUGIN_LOAD_TYPE profilerSys    = {};

void load_plugins() {
  pluginHnd_ecstapi threadingPlugin = editorECS->loadPlugin("tapi_threadedjobsys.dll");
  auto              threadingSys =
    ( THREADINGSYS_TAPI_PLUGIN_LOAD_TYPE )editorECS->getSystem(THREADINGSYS_TAPI_PLUGIN_NAME);
  printf("Thread Count: %u\n", threadingSys->funcs->thread_count());

  pluginHnd_ecstapi arrayOfStringsPlugin = editorECS->loadPlugin("tapi_array_of_strings_sys.dll");
  auto              AoSsys =
    ( ARRAY_OF_STRINGS_TAPI_LOAD_TYPE )editorECS->getSystem(ARRAY_OF_STRINGS_TAPI_PLUGIN_NAME);

  pluginHnd_ecstapi profilerPlugin = editorECS->loadPlugin("tapi_profiler.dll");
  profilerSys = ( PROFILER_TAPI_PLUGIN_LOAD_TYPE )editorECS->getSystem(PROFILER_TAPI_PLUGIN_NAME);

  pluginHnd_ecstapi filesysPlugin = editorECS->loadPlugin("tapi_filesys.dll");
  filesys = ( FILESYS_TAPI_PLUGIN_LOAD_TYPE )editorECS->getSystem(FILESYS_TAPI_PLUGIN_NAME);

  pluginHnd_ecstapi loggerPlugin = editorECS->loadPlugin("tapi_logger.dll");
  auto loggerSys = ( LOGGER_TAPI_PLUGIN_LOAD_TYPE )editorECS->getSystem(LOGGER_TAPI_PLUGIN_NAME);

  allocatorSys =
    ( ALLOCATOR_TAPI_PLUGIN_LOAD_TYPE )editorECS->getSystem(ALLOCATOR_TAPI_PLUGIN_NAME);

  {
    pluginHnd_ecstapi tgfxPlugin = editorECS->loadPlugin("tgfx_core.dll");
    auto              tgfxSys    = ( TGFX_PLUGIN_LOAD_TYPE )editorECS->getSystem(TGFX_PLUGIN_NAME);
    if (tgfxSys) {
      tgfx           = tgfxSys->api;
    }
  }
}

glm::vec3 camPos(0, 0, 0);
glm::vec3 camTarget(0, 0, 1);
glm::vec3 world_up(0, 1, 0);
glm::vec3 frontVector = glm::normalize(camTarget);
glm::vec3 rightVector = -glm::normalize(glm::cross(world_up, frontVector));

// This Component doesn't use ROTATION and SCALE components of GameComponent!
// Instead, you will specify the target vector of camera to look at
struct Camera_Component {
  glm::vec3      POSITION, ROTATION, SCALE;
  glm::mat4      View_Matrix, Projection_Matrix;
  unsigned short FOV_in_Angle = 45, Aspect_Width = 1920.0f, Aspect_Height = 1080.0f;
  float          Near_Plane = 0.01f, Far_Plane = 100000.0f;
  bool           is_Projection_Matrix_changed = true, is_Target_Changed = true;
  glm::vec3      Target;

 public:
  Camera_Component(glm::vec3 target);

  // Getters
  glm::mat4x4 Calculate_Projection_Matrix() {
    if (is_Projection_Matrix_changed) {
      glm::mat4x4 proj_mat;
      proj_mat                     = glm::perspective(glm::radians(float(FOV_in_Angle)),
                                                      float(Aspect_Width / Aspect_Height), Near_Plane, Far_Plane);
      Projection_Matrix            = proj_mat;
      is_Projection_Matrix_changed = false;
    }
    return Projection_Matrix;
  }
  glm::mat4x4 Calculate_View_Matrix() {
    if (true) {
      glm::mat4x4 view_mat;
      glm::vec3   Front_Vector = -Target;
      glm::vec3   World_UP     = glm::vec3(0, 1, 0);

      view_mat    = glm::lookAt(POSITION, Front_Vector + POSITION, World_UP);
      View_Matrix = view_mat;
    }
    return View_Matrix;
  }

  // Setters
  void Set_Camera_Properties(unsigned short fov_in_Angle, float aspect_Width, float aspect_Height,
                             float near_plane, float far_plane) {
    FOV_in_Angle                 = fov_in_Angle;
    Aspect_Width                 = aspect_Width;
    Aspect_Height                = aspect_Height;
    Near_Plane                   = near_plane;
    Far_Plane                    = far_plane;
    is_Projection_Matrix_changed = true;
  }
};
float camYaw = 0.0f;

void keyCB(window_tgfxhnd windowHnd, void* userPointer, key_tgfx key, int scanCode,
           key_action_tgfx action, keyMod_tgfx mode) {
  switch (key) {
    case key_tgfx_A: camPos -= rightVector; break;
    case key_tgfx_S: camPos -= frontVector; break;
    case key_tgfx_W: camPos += frontVector; break;
    case key_tgfx_D: camPos += rightVector; break;
    case key_tgfx_RIGHT: camYaw += 1.0f; break;
    case key_tgfx_LEFT: camYaw -= 1.0f; break;
  }
  // camYaw    = glm::max(-89.0f, glm::min(camYaw, 89.0f));
  camTarget.x = glm::cos(glm::radians(0.0)) * glm::cos(glm::radians(camYaw));
  camTarget.y = glm::sin(glm::radians(0.0));
  camTarget.z = glm::cos(glm::radians(0.0)) * glm::sin(glm::radians(camYaw));
};

void load_systems() {
  load_plugins();

  rtRenderer::initialize(keyCB);
  rtMeshManager::initializeManager();
  rtSceneManager::initializeManager();

  uint64_t    resourceCount  = {};
  rtResource* firstResources = rtResourceManager::importAssimp(
    // SOURCE_DIR "Content/cube.fbx"
    "D:/Desktop/Meshes/Gun/Handgun_fbx_7.4_binary.fbx", &resourceCount);


  int i = 0;
  uint64_t duration = {};
  while (++i && i < 1000000) {
    TURAN_PROFILE_SCOPE_MCS(profilerSys->funcs, "presentation", &duration);

    frontVector         = glm::normalize(camTarget);
    rightVector         = -glm::normalize(glm::cross(world_up, frontVector));
    glm::vec3 UP_VECTOR = -glm::normalize(glm::cross(frontVector, rightVector));
    rtRenderer::setActiveFrameCamProps(
      glm::lookAt(camPos, camPos + frontVector, world_up),
      glm::perspective(glm::radians(90.0f), 16.0f / 9.0f, 0.01f, 100.0f));
    
    
    rtMeshManager::frame();
    rtRenderer::renderFrame();
    tgfx->takeInputs();
    STOP_PROFILE_PRINTFUL_TAPI(profilerSys->funcs);
    printf("Finished and frame index: %u\n", i);
  }

  rtRenderer::close();
}

#include "ecs_tapi.h"

ecs_tapi*   editorECS = nullptr;
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