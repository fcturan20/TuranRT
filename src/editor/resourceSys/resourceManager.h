#pragma once

// Resource management is 2-level: rtResourceHnd (rtRH) and specific resource handle (SRH).
// 1) rtRH is an offline description of a resource and rtResourceManager can store&use it
//  however it wants. For example: It can store all resources in a list and save it while closing
//  the app, then reload all resources by looking at the saved list. Then if saved resourceHnd says
//  that resource was in ver1 but resource is now in ver2 in disk, deserialization may fail etc.
// 2) SRH (void*) are just run-time datas. Their offline representation (path, static data etc) is
//  all handled in their manager while serializing them to disk with a rtRH.
//  Long story short: rtRHs points to SRH but not the other way around. So SRH doesn't have to know
//  about rtRH. This decoupling allows same SRH to be saved to different files without changing SRH
// (read-only dependency, good for multithreading).

// Resource types are important because if 2 different plugins uses same file extension, we should
//  be able to specify which one to use without testing to load with both plugins
typedef struct resource_rt*            rtResource;
typedef struct resourceManagerType_rt* rtResourceManagerType;
typedef struct resourceDesc_rt {
  // Example: C:\dev\firstScene.scene
  const char*           pathNameExt;
  bool                  isBinary;
  rtResourceManagerType managerType;
  void*                 managerData; // Manager may need extra data
  uint32_t              managerDataSize;
  void*                 resourceHnd;
} rtResourceDesc;
typedef struct resourceManager_rt {
  // Import resource from a file
  // @return resourceCount sized resource handle list
  static rtResource* importFile(const wchar_t* PATH, uint64_t* resourceCount);
  // Load a resource from file
  // @param desc: Description is read but can be changed if there are problems
  // @param resourceHnd: Handle that you can use in rtResourceManager
  // @return Handle returned by specified manager's deserializeResource
  static bool deserializeResource(rtResourceDesc* desc, rtResource* resourceHnd);
  // Should be called from resource managers
  // Desc's loaded resourceHnd has to be valid!
  static rtResource createResource(rtResourceDesc desc);
  // @param managerType: managerType of the resource is returned, so you can check the resource type
  // @return resource's handle
  static void*      getResourceHnd(rtResource resource, rtResourceManagerType& managerType);

  typedef bool (*deserializeResourceFnc)(rtResourceDesc* desc);
  typedef bool (*isResourceValidFnc)(void* dataHnd);
  struct managerDesc {
    const char*            managerName;
    uint32_t               managerVer;
    deserializeResourceFnc deserialize;
    isResourceValidFnc     validate;
  };
  static rtResourceManagerType registerManager(managerDesc desc);
} rtResourceManager;