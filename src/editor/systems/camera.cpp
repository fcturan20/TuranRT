#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include "tgfx_core.h"
#include <logger_tapi.h>
#include <ecs_tapi.h>

#include "camera.h"
#include "../render_context/rendercontext.h"
#include "../resourceSys/scene.h"
#include "input.h"

struct teCameraComponent {
  glm::vec3 pos = glm::vec3(0, 0, -1), rot = {}, scale = {}, target = glm::vec3(0, 0, -1);
  float     fov = 90.0f, nearPlane = 0.01f, farPlane = 100.0f, mouseSensitivity = 0.1f,
        mouseYaw = -90.0f, mousePitch = 0.0f;
  tgfx_vec2 res = {1920.0f, 1080.0f};
  bool      isActive = false;
};
static teCameraComponent* activeCam_rt           = nullptr;
static glm::vec3          worldUp                = glm::vec3(0, 1, 0);
static bool               isCameraControllerInit = false;

rtMat4 getRTMAT4(glm::mat4 src) {
  rtMat4 mat4;
  memcpy(mat4.v, &src[0][0], sizeof(src));
  return mat4;
}

glm::mat4 getGLMMAT4(rtMat4 src) {
  glm::mat4 mat4;
  memcpy(&mat4[0][0], src.v, sizeof(rtMat4));
  return mat4;
}

key_tgfx                  buttons[4]       = {key_tgfx_W, key_tgfx_S, key_tgfx_A, key_tgfx_D};
bool                      buttonPressed[4] = {};
rtInputAllocation      buttonHnd[4]     = {};
tapi_ecs_entityType*      cameraNttyTypeID = nullptr;
tapi_ecs_componentTypeID* cameraCompTypeID = nullptr;

tgfx_vec2 lastMousePosXY = {};
struct cameraControllerPrivate {
  static void camMoveEvent(rtInputAllocation allocHnd, keyAction_tgfx state, void* userPtr) {
    for (uint32_t i = 0; i < 4; i++) {
      if (allocHnd == buttonHnd[i]) {
        if (state == keyAction_tgfx_RELEASE) {
          buttonPressed[i] = false;
        } else {
          buttonPressed[i] = true;
        }
      }
    }
  }
  static void camRightClickEvent(rtInputAllocation allocHnd, keyAction_tgfx state,
                                 void* userPtr) {
    // Allocate WASD keys to move camera position
    if (state != keyAction_tgfx_RELEASE) {
      for (uint32_t i = 0; i < 4; i++) {
        key_tgfx allocKey = buttons[i];
        buttonHnd[i]      = rtInputSystem::allocate(1, &allocKey, camMoveEvent, userPtr, allocHnd);
      }

      // If you want to make sensitivity framerate-independent, you should set mouse sensitivity
      // with setCameraProps()
      float      multiplier = activeCam_rt->mouseSensitivity;
      float&     mouseYaw   = activeCam_rt->mouseYaw;
      float&     mousePitch = activeCam_rt->mousePitch;
      glm::vec3& target     = activeCam_rt->target;
      glm::vec3& pos        = activeCam_rt->pos;

      tgfx_vec2 curMousePos;
      tgfx->getCursorPos(mainWindowRT, &curMousePos);

      if (state == keyAction_tgfx_PRESS) {
        lastMousePosXY = curMousePos;
      }
      tgfx_vec2 mouseOffsetXY = {curMousePos.x - lastMousePosXY.x,
                                 curMousePos.y - lastMousePosXY.y};
      lastMousePosXY          = curMousePos;
      mouseYaw += mouseOffsetXY.x * multiplier;
      mousePitch += mouseOffsetXY.y * multiplier;

      // Limit Pitch direction to 90 degrees, because we don't want our camera to
      // upside-down!
      if (mousePitch > 89.0f) {
        mousePitch = 89.0f;
      } else if (mousePitch < -89.0f) {
        mousePitch = -89.0f;
      }

      // Set target direction
      double radPitch = glm::radians(mousePitch);
      double radYaw   = glm::radians(mouseYaw);
      target          = glm::vec3(glm::cos(radPitch) * glm::cos(radYaw), glm::sin(radPitch),
                                  glm::cos(radPitch) * glm::sin(radYaw));
      tgfx->setInputMode(mainWindowRT, cursorMode_tgfx_RAW, false, false, false);
    } else {
      tgfx->setInputMode(mainWindowRT, cursorMode_tgfx_NORMAL, false, false, false);
    }
  }
  static void destroyCameraComponent(tapi_ecs_component* comp) {
    delete ( teCameraComponent* )comp;
  }
  static tapi_ecs_component* createCameraComponent() {
    return ( tapi_ecs_component* )new teCameraComponent;
  }
  static struct tapi_ecs_componentTypeID* getCameraComponentTypeID() { return cameraCompTypeID; }
  static tapi_ecs_entity* createCamera(struct rtScene* scene, unsigned char isPerspective) {
    if (!isCameraControllerInit) {
      return nullptr;
    }

    tapi_ecs_entity*   entity = sceneManager->addEntity(scene, cameraNttyTypeID);
    teCameraComponent* cam =
      ( teCameraComponent* )editorECS->get_component_byEntityHnd(entity, cameraCompTypeID, nullptr);
    if (!isPerspective) {
      cam->fov = FLT_MAX;
    }
    activeCam_rt = cam;

    return entity;
  }

  static void setCameraProps(tapi_ecs_entity* ntty, const tgfx_vec2* resolution,
                             const float* nearPlane, const float* farPlane,
                             const float* mouseSensitivity, const float* fov, const unsigned char* isActive) {
    teCameraComponent* cam =
      ( teCameraComponent* )editorECS->get_component_byEntityHnd(ntty, cameraCompTypeID, nullptr);
    if (resolution) {
      cam->res = *resolution;
    }
    if (nearPlane) {
      cam->nearPlane = *nearPlane;
    }
    if (farPlane) {
      cam->farPlane = *farPlane;
    }
    if (mouseSensitivity) {
      cam->mouseSensitivity = *mouseSensitivity;
    }
    if (fov) {
      if (cam->fov == FLT_MAX) {
        assert(0 && "Orthogonal camera hasn't fov but you try to set it anyway!");
      }
      cam->fov = *fov;
    }
    if (isActive) {
      activeCam_rt->isActive = false;
      cam->isActive = true;
      activeCam_rt = cam;
    }
  }

  // Left handed isn't supported for now
  static void getCameraMatrixes(bool isRightHanded, rtMat4* viewMat, rtMat4* projMat,
                                struct tgfx_vec3* camPos, struct tgfx_vec3* camDir, float* fov) {
    // Calculate view matrix
    if (viewMat) {
      glm::vec3 frontVector = -activeCam_rt->target;
      *viewMat =
        getRTMAT4(glm::lookAt(activeCam_rt->pos, frontVector + activeCam_rt->pos, worldUp));
    }

    // Calculate projection matrix
    if (projMat) {
      *projMat = getRTMAT4(glm::perspective(glm::radians(float(activeCam_rt->fov)),
                                            activeCam_rt->res.x / activeCam_rt->res.y,
                                            activeCam_rt->nearPlane, activeCam_rt->farPlane));
    }

    if (camPos) {
      *camPos = {activeCam_rt->pos.x, activeCam_rt->pos.y, activeCam_rt->pos.z};
    }
    if (camDir) {
      glm::vec3 dir = glm::normalize(activeCam_rt->target - activeCam_rt->pos);
      *camDir       = {dir.x, dir.y, dir.z};
    }
    if (fov) {
      *fov = activeCam_rt->fov;
    }
  }
  static void update() {
    if (!activeCam_rt) {
      return;
    }
    key_tgfx inputKey = key_tgfx_MOUSE_RIGHT;
    rtInputSystem::allocate(1, &inputKey, camRightClickEvent, nullptr, nullptr);

    // If you want to make sensitivity framerate-independent, you should set mouse sensitivity with
    // setCameraProps()
    float      multiplier = activeCam_rt->mouseSensitivity;
    float&     mouseYaw   = activeCam_rt->mouseYaw;
    float&     mousePitch = activeCam_rt->mousePitch;
    glm::vec3& target     = activeCam_rt->target;
    glm::vec3& pos        = activeCam_rt->pos;

    glm::vec3 front, right, up;
    front = glm::normalize(target);
    right = glm::cross(front, worldUp);
    up    = -(glm::normalize(glm::cross(front, right)));

    // Use input api to allocate this keys
    if (buttonPressed[0]) {
      printf("W\n");
      pos -= front * multiplier;
    }
    if (buttonPressed[1]) {
      printf("S\n");
      pos += front * multiplier;
    }
    if (buttonPressed[2]) {
      printf("A\n");
      pos += right * multiplier;
    }
    if (buttonPressed[3]) {
      printf("D\n");
      pos -= right * multiplier;
    }
  }
};
static void initializeCameraController() {
  // Create system struct
  {
    rtCameraController* c = new rtCameraController;
    c->createCamera       = cameraControllerPrivate::createCamera;
    c->getCameraMatrixes  = cameraControllerPrivate::getCameraMatrixes;
    c->setCameraProps     = cameraControllerPrivate::setCameraProps;
    c->update             = cameraControllerPrivate::update;
    cameraController      = c;
  }

  key_tgfx inputKey = key_tgfx_MOUSE_RIGHT;
  rtInputSystem::allocate(1, &inputKey, cameraControllerPrivate::camRightClickEvent, nullptr,
                          nullptr);

  // Register TE Camera Component
  {
    struct ecs_compManager compManager;
    compManager.createComponent  = cameraControllerPrivate::createCameraComponent;
    compManager.destroyComponent = cameraControllerPrivate::destroyCameraComponent;
    cameraCompTypeID =
      editorECS->addComponentType("TE Camera Component", nullptr, compManager, nullptr, 0);
    cameraNttyTypeID = editorECS->addEntityType(&cameraCompTypeID, 1);
  }
}
