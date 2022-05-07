#include "profiler_tapi.h"
#include "threadingsys_tapi.h"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>


profiledscope_handle_tapi* last_handles;
unsigned int threadcount;

struct Profiled_Scope {
public:
	bool Is_Recording : 1;
	unsigned char TimingType : 2;
	unsigned long long START_POINT;
	unsigned long long* DURATION;
	std::string NAME;
};


void profiler_tapi::start_profiling(profiledscope_handle_tapi* handle, const char* NAME, unsigned long long* duration, unsigned char TimingTypeIndex) {
    unsigned int threadindex = (threadcount == 1) ? (0) : (threadingsys.this_thread_index());
    Profiled_Scope* profil = new Profiled_Scope;
    while(TimingTypeIndex > 3){printf("Profile started with index bigger than 3! Should be between 0-3!\n"); scanf(" %hhu", &TimingTypeIndex);}
    switch (TimingTypeIndex) {
    case 0:
        profil->START_POINT = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
        break;
    case 1:
        profil->START_POINT = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
        break;
    case 2:
        profil->START_POINT = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
        break;
    case 3:
        profil->START_POINT = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
        break;
    }
    profil->Is_Recording = true;
    profil->DURATION = duration;
    profil->TimingType = TimingTypeIndex;
    profil->NAME = NAME;
    *handle = (profiledscope_handle_tapi)profil;
    last_handles[threadindex]  = *handle;
}

void profiler_tapi::finish_profiling(profiledscope_handle_tapi* handle, unsigned char ShouldPrint) {
    Profiled_Scope* profil = (Profiled_Scope*)*handle;
    switch (profil->TimingType) {
    case 0:
        *profil->DURATION = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count() - profil->START_POINT;
        if (ShouldPrint) {printf((profil->NAME + " took: " + std::to_string(*profil->DURATION) + "nanoseconds!\n").c_str());}
        return;
    case 1:
        *profil->DURATION = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count() - profil->START_POINT;
        if (ShouldPrint) {printf((profil->NAME + " took: " + std::to_string(*profil->DURATION) + "microseconds!\n").c_str());}
        return;
    case 2:
        *profil->DURATION = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count() - profil->START_POINT;
        if (ShouldPrint) {printf((profil->NAME + " took: " + std::to_string(*profil->DURATION) + "milliseconds!\n").c_str());}
        return;
    case 3:
        *profil->DURATION = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
        if (ShouldPrint) {printf((profil->NAME + " took: " + std::to_string(*profil->DURATION) + "seconds!\n").c_str());}
        return;
    }
    delete profil;
}
void profiler_tapi::threadlocal_finish_last_profiling(unsigned char ShouldPrint) {
    unsigned int threadindex = (threadcount == 1) ? (0) : (threadingsys.this_thread_index());
    Profiled_Scope* profil = (Profiled_Scope*)last_handles[threadindex];
    switch (profil->TimingType) {
    case 0:
        *profil->DURATION = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count() - profil->START_POINT;
        if (ShouldPrint) {printf((profil->NAME + " took: " + std::to_string(*profil->DURATION) + "nanoseconds!").c_str());}
        return;
    case 1:
        *profil->DURATION = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count() - profil->START_POINT;
        if (ShouldPrint) {printf((profil->NAME + " took: " + std::to_string(*profil->DURATION) + "microseconds!").c_str());}
        return;
    case 2:
        *profil->DURATION = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count() - profil->START_POINT;
        if (ShouldPrint) {printf((profil->NAME + " took: " + std::to_string(*profil->DURATION) + "milliseconds!").c_str());}
        return;
    case 3:
        *profil->DURATION = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
        if (ShouldPrint) {printf((profil->NAME + " took: " + std::to_string(*profil->DURATION) + "seconds!").c_str());}
        return;
    }
    delete profil;
}

void initialize_profilersys() {
    unsigned int threadcount = threadingsys.thread_count();

    threadcount = threadcount;
    last_handles = new profiledscope_handle_tapi[threadcount];
    memset(last_handles, 0, sizeof(profiledscope_handle_tapi) * threadcount);
}
