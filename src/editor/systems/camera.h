#pragma once
#ifdef __cplusplus
extern "C" {
#endif


struct rtCameraController {
  static struct tapi_ecs_entity* (*createCamera)(struct rtScene* scene, unsigned char isPerspective);
  static void (*setCameraProps)(struct tapi_ecs_entity* cam, const struct tgfx_vec2* resolution,
                                const float* nearPlane, const float* farPlane,
                                const float* mouseSensitivity, const float* fov, const unsigned char* isActive);
  // @param FoV = radian, camDir = normalized
  static void (*getCameraMatrixes)(bool isRightHanded, struct rtMat4* viewMat, struct rtMat4* projMat,
                                   struct tgfx_vec3* camPos, struct tgfx_vec3* camDir, float* fov);
  static void (*update)();
};
extern const struct rtCameraController* cameraController;
static void                             initializeCameraController();

#ifdef __cplusplus
}
#endif