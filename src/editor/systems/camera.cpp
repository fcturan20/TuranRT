#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include "tgfx_core.h"
#include <logger_tapi.h>

#include "camera.h"
#include "../render_context/rendercontext.h"
#include "input.h"

static rtCamera  activeCam_rt           = nullptr;
static glm::vec3 worldUp                = glm::vec3(0, 1, 0);
static bool      isCameraControllerInit = false;
struct camera_rt {
  glm::vec3 pos = glm::vec3(0,0,-10), rot = {}, scale = {}, target = glm::vec3(0,0,-1);
  float     fov = 45.0f, nearPlane = 0.01f, farPlane = 100.0f, mouseSensitivity = 0.001f,
        mouseYaw = -90.0f, mousePitch = 0.0f;
  tgfx_vec2 res = {1920.0f, 1080.0f};
};

key_tgfx             buttons[4]       = {key_tgfx_W, key_tgfx_S, key_tgfx_A, key_tgfx_D};
bool                 buttonPressed[4] = {};
rtInputAllocationHnd buttonHnd[4]     = {};

void camMoveEvent(rtInputAllocationHnd allocHnd, key_action_tgfx state, void* userPtr) {
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
tgfx_vec2 lastMousePosXY = {};
void camRightClickEvent(rtInputAllocationHnd allocHnd, key_action_tgfx state, void* userPtr) {
  // Allocate WASD keys to move camera position
  if (state != keyAction_tgfx_RELEASE) {
    for (uint32_t i = 0; i < 4; i++) {
      key_tgfx allocKey = buttons[i];
      buttonHnd[i]      = rtInputSystem::allocate(1, &allocKey, camMoveEvent, userPtr, allocHnd);
    }

    // If you want to make sensitivity framerate-independent, you should set mouse sensitivity with
    // setCameraProps()
    float      multiplier = activeCam_rt->mouseSensitivity;
    float&     mouseYaw   = activeCam_rt->mouseYaw;
    float&     mousePitch = activeCam_rt->mousePitch;
    glm::vec3& target     = activeCam_rt->target;
    glm::vec3& pos        = activeCam_rt->pos;

    tgfx_vec2 curMousePos   = tgfx->getCursorPos(mainWindowRT);
    if (state == keyAction_tgfx_PRESS) {
      lastMousePosXY = curMousePos;
    }
    tgfx_vec2 mouseOffsetXY = {curMousePos.x - lastMousePosXY.x, curMousePos.y - lastMousePosXY.y};
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

rtCamera cameraController_rt::createCamera(bool isPerspective) {
  rtCamera cam = new camera_rt;
  if (!isPerspective) {
    cam->fov = FLT_MAX;
  }
  activeCam_rt = cam;

  if (!isCameraControllerInit) {
    key_tgfx inputKey = key_tgfx_MOUSE_RIGHT;
    rtInputSystem::allocate(1, &inputKey, camRightClickEvent, nullptr, nullptr);
  }
  return cam;
}

void cameraController_rt::setActiveCamera(rtCamera cam) { activeCam_rt = cam; }

void cameraController_rt::setCameraProps(rtCamera cam, const tgfx_vec2* resolution,
                                         const float* nearPlane, const float* farPlane,
                                         const float* mouseSensitivity, const float* fov) {
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
}

void      cameraController_rt::update() {
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

  rtMat4 viewMat, projMat;
  getCameraMatrixes(true, &viewMat, &projMat);
  tgfx_vec3 camPosTgfx = {pos.x, pos.y, pos.z};
  tgfx_vec3 camDirTgfx = {front.x, front.y, front.z};
  rtRenderer::setActiveFrameCamProps(&viewMat, &projMat, &camPosTgfx, &camDirTgfx, float(activeCam_rt->fov));
}

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

// Left handed isn't supported for now
void cameraController_rt::getCameraMatrixes(bool isRightHanded, rtMat4* viewMat, rtMat4* projMat) {
  // Calculate view matrix
  if (viewMat) {
    glm::vec3 frontVector = -activeCam_rt->target;
    *viewMat = getRTMAT4(glm::lookAt(activeCam_rt->pos, frontVector + activeCam_rt->pos, worldUp));
  }

  // Calculate projection matrix
  if (projMat) {
    *projMat = getRTMAT4(glm::perspective(glm::radians(float(activeCam_rt->fov)),
                                          activeCam_rt->res.x / activeCam_rt->res.y,
                                          activeCam_rt->nearPlane, activeCam_rt->farPlane));
  }
}