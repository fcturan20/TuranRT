#pragma once
#ifdef __cplusplus
extern "C" {
#endif

  //Include tgfx_structs.h before this
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

struct rtInputAllocation;
typedef void (*inputAllocationEvent_rt)(struct rtInputAllocation* allocHnd, keyAction_tgfx state,
                                        void* userPtr);
struct rtInputSystem {
  // Returns nullptr if fails
  // Returns a hnd to allow inheritance
  static struct rtInputAllocation* (*allocate)(unsigned int keyCount, const key_tgfx* keyList,
                                               inputAllocationEvent_rt event, void* userPtr,
                                               struct rtInputAllocation* parentAlloc);
  static void (*update)();
  static tgfx_windowKeyCallback (*getCallback)();
};
void                               initializeInputSystem();
extern const struct rtInputSystem* inputSys;

#ifdef __cplusplus
}
#endif