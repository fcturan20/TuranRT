#include <vector>
#include <assert.h>

#include "input.h"
#include "tgfx_core.h"

struct inputAllocation_rt {
  inputAllocationEvent_rt     event = nullptr;
  const std::vector<key_tgfx> keyList;
  void*                       userPtr     = nullptr;
  rtInputAllocationHnd        parentAlloc = nullptr;
  uint32_t                    hierarchyLevelIndex = 0;
  inputAllocation_rt(unsigned int keyCount, const key_tgfx* keys) : keyList(keys, keys + keyCount) {}
};
static std::vector<rtInputAllocationHnd> inputAllocs;
static key_action_tgfx                   keyStates[key_tgfx_MAX_ENUM] = {keyAction_tgfx_RELEASE};
static uint32_t                          hierarchyLevelCount            = 0;

// Checks if src list is a subset of dst list
bool checkSubsetKeylist(const std::vector<key_tgfx>& src, const std::vector<key_tgfx>& dst) {
  bool isSubset = true;
  for (key_tgfx s : src) {
    bool isFound = false;
    for (key_tgfx d : dst) {
      if (s == d) {
        isFound = true;
      }
    }
    if (!isFound) {
      isSubset = false;
      break;
    }
  }
  return isSubset;
}
rtInputAllocationHnd inputSystem_rt::allocate(unsigned int keyCount, const key_tgfx* keyList,
                                              inputAllocationEvent_rt e, void* userPtr,
                                              rtInputAllocationHnd parentAlloc) {
  uint32_t hierarchyLevelIndex = 0;
  if (parentAlloc) {
    hierarchyLevelIndex = UINT32_MAX;
    for (rtInputAllocationHnd alloc : inputAllocs) {
      if (alloc == parentAlloc) {
        hierarchyLevelIndex = parentAlloc->hierarchyLevelIndex + 1;
      }
    }
  }
  hierarchyLevelCount = std::max(hierarchyLevelCount, hierarchyLevelIndex + 1);

  assert(hierarchyLevelIndex != UINT32_MAX && "Input key allocation's parent is invalid");
  assert(keyCount && "Key list is invalid");
  assert(e && "Key event is invalid");

  rtInputAllocationHnd newAlloc = nullptr;
  for (rtInputAllocationHnd alloc : inputAllocs) {
    bool isSubset =
      checkSubsetKeylist(alloc->keyList, std::vector<key_tgfx>(keyList, keyList + keyCount));
    if (!isSubset) {
      isSubset = checkSubsetKeylist(std::vector<key_tgfx>(keyList, keyList + keyCount), alloc->keyList);
    }
    if (isSubset) {
      newAlloc = alloc;
    }
  }
  if (!newAlloc) {
    newAlloc              = new inputAllocation_rt(keyCount, keyList);
    newAlloc->event       = e;
    newAlloc->parentAlloc = parentAlloc;
    newAlloc->userPtr     = userPtr;
    inputAllocs.push_back(newAlloc);
  } else {
    newAlloc = nullptr;
  }
  return newAlloc;
}

void inputSystem_rt::update() {
  tgfx->takeInputs();

  for (uint32_t hierarchyLevelIndex = 0; hierarchyLevelIndex < hierarchyLevelCount; hierarchyLevelIndex++) {
    for (uint32_t i = 0; i < inputAllocs.size(); i++) {
      rtInputAllocationHnd alloc = inputAllocs[i];
      if (hierarchyLevelIndex != alloc->hierarchyLevelIndex) {
        continue;
      }
      // Detect state
      key_action_tgfx keyCombState = keyAction_tgfx_RELEASE;
      for (key_tgfx k : alloc->keyList) {
        key_action_tgfx state = keyStates[k];
        // If even a single isn't in a press/hold state, combination fails
        if (state == keyAction_tgfx_RELEASE) {
          keyCombState = keyAction_tgfx_RELEASE;
          break;
        }
        // If this key is in hold state and;
        // 1) all previous key were in hold state or
        // 2) this key is the first key (so keyCombState is in release as default value)
        else if (state == keyAction_tgfx_REPEAT && keyCombState != keyAction_tgfx_PRESS) {
          keyCombState = keyAction_tgfx_REPEAT;
        }
        // If any key in the combination is just pressed, all combination is just pressed
        else if (state == keyAction_tgfx_PRESS) {
          keyCombState = keyAction_tgfx_PRESS;
        }
      }

      alloc->event(alloc, keyCombState, alloc->userPtr);
    }
  }
  for (rtInputAllocationHnd alloc : inputAllocs) {
    delete alloc;
  }

  inputAllocs.clear();
}

void windowKeyCallback_rt(window_tgfxhnd windowHnd, void* userPointer, key_tgfx key, int scanCode,
                          key_action_tgfx action, keyMod_tgfx mode) {
  keyStates[key] = action;
}
tgfx_windowKeyCallback inputSystem_rt::getCallback() { return windowKeyCallback_rt; }