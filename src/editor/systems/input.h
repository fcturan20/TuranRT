#pragma once
#include "../editor_includes.h"
#include "tgfx_forwarddeclarations.h"

/*
* Input system is to force you carefully design when you need to get input from the user
* You can't allocate a key combination if any of the subset of the keys is already allocated
* With this way, it is easy to detect when input operations collide
* For example: Your game uses E button as talk-interaction button but also as
environment-interaction. When environment and talk is interactible, later allocation will fail and
return nullptr. This means you should order your interactions beforehand, otherwise your allocations
will randomly be not-working.
* All allocations are freed each update(). So you may call this a per-frame allocation system
*/

typedef struct inputAllocation_rt* rtInputAllocationHnd;
typedef void (*inputAllocationEvent_rt)(rtInputAllocationHnd allocHnd, key_action_tgfx state,
                                     void* userPtr);
typedef struct inputSystem_rt {
  // Returns nullptr if fails
  // Returns a hnd to allow inheritance
  static rtInputAllocationHnd allocate(unsigned int keyCount, const key_tgfx* keyList,
                                       inputAllocationEvent_rt event, void* userPtr,
                                       rtInputAllocationHnd parentAlloc);
  static void                 update();
  static tgfx_windowKeyCallback getCallback();
} rtInputSystem;