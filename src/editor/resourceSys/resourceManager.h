#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// Resource management is 2-level: rtResourceHnd (rtRH) and specific resource handle (SRH).
// 1) rtRH is an offline description of a resource and rtResourceManager can store&use it
//  however it wants. For example: It can store all resources in a list and save it while closing
//  the app, then reload all resources by looking at the saved list. Then if saved resourceHnd says
//  that resource was in ver1 but resource is now in ver2 in disk, deserialization may fail etc.
// 2) SRH (void*) are just run-time datas. Their offline representation (path, data etc) is
//  all handled in their manager while serializing them to disk with a rtRH.
//  Long story short: rtRHs points to SRH but not the other way around. So SRH doesn't have to know
//  about rtRH. This decoupling allows same SRH to be saved to different files without changing SRH
// (read-only dependency, good for multithreading).

// Resource types are important because if 2 different plugins uses same file extension, we should
//  be able to specify which one to use without testing to load with both plugins
struct rtResourceDesc {
  // Example: C:\dev\firstScene.scene
  const char*                   pathNameExt;
  unsigned char                 isBinary;
  const struct rtResourceManagerType* managerType;
  void*                         managerData; // Manager may need extra data
  uint32_t                      managerDataSize;
  void*                         resourceHnd;
};

// Import resource from a file
// @return resourceCount sized resource handle list
struct rtResource** RM_importFile(const wchar_t* PATH, uint64_t* resourceCount);
// Load a resource from file
// @param desc: Description is read but can be changed if there are problems
// @param resourceHnd: Handle that you can use in rtResourceManager
// @return Handle returned by specified manager's deserializeResource
unsigned char RM_deserializeResource(const struct rtResourceDesc* desc,
                                         struct rtResource**   resourceHnd);
// Should be called from resource managers
// Desc's loaded resourceHnd has to be valid!
struct rtResource* RM_createResource(const struct rtResourceDesc* desc);
// @param managerType: managerType of the resource is returned, so you can check the resource type
// @return resource's handle
void* RM_getResourceHnd(struct rtResource*                   resource,
                            const struct rtResourceManagerType** managerType);

typedef unsigned char (*RM_deserializeResourceFnc)(const struct rtResourceDesc* desc);
typedef unsigned char (*RM_isResourceValidFnc)(void* dataHnd);
struct RM_managerDesc {
  const char*            managerName;
  uint32_t               managerVer;
  RM_deserializeResourceFnc deserialize;
  RM_isResourceValidFnc     validate;
};
const struct rtResourceManagerType* RM_registerManager(RM_managerDesc desc);

#ifdef __cplusplus
}
#endif