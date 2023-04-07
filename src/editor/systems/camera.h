#pragma once
#include "../editor_includes.h"
#include "tgfx_structs.h"

typedef struct camera_rt* rtCamera;

typedef struct cameraController_rt {
  static rtCamera createCamera(bool isPerspective);
  static void     setActiveCamera(rtCamera cam);
  static void     setCameraProps(rtCamera cam, const tgfx_vec2* resolution = nullptr,
                                 const float* nearPlane = nullptr, const float* farPlane = nullptr,
                                 const float* mouseSensitivity = nullptr, const float* fov = nullptr);
  static void     getCameraMatrixes(bool isRightHanded, rtMat4* viewMat = nullptr,
                                    rtMat4* projMat = nullptr);
  static void     update();
} rtCameraController;