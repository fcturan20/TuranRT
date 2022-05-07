#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

#include "threadingsys_tapi.h"


threadingsys_tapi threadingsys = {};

class JobSystem {
	//This class has to wait as std::conditional_variable() if there is no job when pop_front_strong() is called
	//Because it contains jobs and shouldn't look for new jobs while already waiting for a job
	class TSJobifiedRingBuffer {
		std::mutex WaitData;
		std::condition_variable NewJobWaitCond;
		std::function<void()> data[256];
		unsigned int head = 0, tail = 0;
	public:
		std::atomic_uint64_t IsThreadBusy = 0;
		bool isEmpty() {
			WaitData.lock();
			bool result = head == tail;
			WaitData.unlock();
			return result;
		}
		bool push_back_weak(const std::function<void()>& item) {
			WaitData.lock();
			if ((head + 1) % 256 != tail) {
				data[head] = [this, item](){
					this->IsThreadBusy.fetch_add(1);
					item();
					this->IsThreadBusy.fetch_sub(1);
					};
				head = (head + 1) % 256;
				NewJobWaitCond.notify_one();
				WaitData.unlock();
				return true;
			}

			WaitData.unlock();
			return false;
		}
		void push_back_strong(const std::function<void()>& item) {
			while (!push_back_weak(item)) {
				std::function<void()> Job;
				pop_front_strong(Job);
				Job();
			}
		}
		bool pop_front_weak(std::function<void()>& item) {
			WaitData.lock();
			if (tail != head) {
				item = data[tail];
				tail = (tail + 1) % 256;
				WaitData.unlock();
				return true;
			}
			WaitData.unlock();
			return false;
		}
		void pop_front_strong(std::function<void()>& item) {
			while (!pop_front_weak(item)) {
				std::unique_lock<std::mutex> Locker(WaitData);
				NewJobWaitCond.wait(Locker);
			}
		}
	};
	public:
	std::map<std::thread::id, unsigned char> ThreadIDs;
	std::atomic<unsigned char> ShouldClose;
	TSJobifiedRingBuffer Jobs;
};
static JobSystem* JOBSYS = nullptr;

void threadingsys_tapi::execute_otherjobs(){
	//ShouldClose will be true if JobSystem destructor is called
	//Destructor will wait for all other jobs to finish
	while (!JOBSYS->ShouldClose.load()) {
		std::function<void()> Job;
		JOBSYS->Jobs.pop_front_strong(Job);
		Job();
	}
}
inline std::atomic_bool& WaitHandleConverter(wait_handle_tapi handle) { return *(std::atomic_bool*)(handle); } 


void threadingsys_tapi::execute_withwait(void(*func)(), wait_handle_tapi wait) {
	//That means job list is full, wake up a thread (if there is anyone sleeping) and yield current thread!
	JOBSYS->Jobs.push_back_strong([&wait, func]() {
		WaitHandleConverter(wait).store(1);
		func();
		WaitHandleConverter(wait).store(2);
		}
	);
} 
void threadingsys_tapi::execute_withoutwait(void(*func)()) {
	JOBSYS->Jobs.push_back_strong(func);
}

wait_handle_tapi threadingsys_tapi::create_wait() {
	return (wait_handle_tapi)new std::atomic_bool;
}
void threadingsys_tapi::waitjob_busy(wait_handle_tapi wait) {
	if (WaitHandleConverter(wait).load() != 2) {
		std::this_thread::yield();
	}
	while (WaitHandleConverter(wait).load() != 2) {
		std::function<void()> Job;
		JOBSYS->Jobs.pop_front_strong(Job);
		Job();
	}
}
void threadingsys_tapi::waitjob_empty(wait_handle_tapi wait) {
	while (WaitHandleConverter(wait).load() != 2) {
		std::this_thread::yield();
	}
}
void threadingsys_tapi::clear_waitinfo(wait_handle_tapi wait) {
	WaitHandleConverter(wait).store(0);
}
void threadingsys_tapi::wait_all_otherjobs() {
	while (!JOBSYS->Jobs.isEmpty()) {
		std::function<void()> Job;
		JOBSYS->Jobs.pop_front_strong(Job);
		Job();
	}
	while(JOBSYS->Jobs.IsThreadBusy.load()){
		std::function<void()> Job;
		if(JOBSYS->Jobs.pop_front_weak(Job)){
			Job();
		}
		else{
			std::this_thread::yield();
		}
	}
}
unsigned int threadingsys_tapi::this_thread_index() {
	return JOBSYS->ThreadIDs[std::this_thread::get_id()];
}
unsigned int threadingsys_tapi::thread_count() {
	return std::thread::hardware_concurrency();
}









void initialize_threadingsys() {
	JOBSYS = new JobSystem;
	JOBSYS->ShouldClose.store(false);

	JOBSYS->ThreadIDs.insert(std::pair<std::thread::id, unsigned char>(std::this_thread::get_id(), 0));
	unsigned int ThreadCount = std::thread::hardware_concurrency();
	if (ThreadCount == 1) {
		printf("Job system didn't create any std::thread objects because your CPU only has 1 core!\n");
	}
	else {
		ThreadCount--;
	}
	for (unsigned int ThreadIndex = 0; ThreadIndex < ThreadCount; ThreadIndex++) {
		std::thread newthread([ThreadIndex]() {JOBSYS->ThreadIDs.insert(std::pair<std::thread::id, unsigned char>(std::this_thread::get_id(), ThreadIndex + 1)); threadingsys.execute_otherjobs(); });
		newthread.detach();
	}
}

/*







tapi_atomic_uint::tapi_atomic_uint(unsigned int Ref) : data(Ref) {
}
tapi_atomic_uint::tapi_atomic_uint() : data(0) {

}
unsigned int tapi_atomic_uint::DirectAdd(const unsigned int& add) {
	return data.fetch_add(add);
}
unsigned int tapi_atomic_uint::DirectSubtract(const unsigned int& sub) {
	return data.fetch_sub(sub);
}
void tapi_atomic_uint::DirectStore(const unsigned int& Store) {
	data.store(Store);
}
unsigned int tapi_atomic_uint::DirectLoad() const {
	return data.load();
}
bool tapi_atomic_uint::LimitedAdd_weak(const unsigned int& add, const unsigned int& maxlimit) {
	unsigned int x = data.load();
	if (x + add > maxlimit ||	//Addition is bigger
		x + add < x				//UINT overflow
		) {
		return false;
	}
	if (!data.compare_exchange_strong(x, x + add)) {
		return LimitedSubtract_weak(add, maxlimit);
	}
	return true;
}
void tapi_atomic_uint::LimitedAdd_strong(const unsigned int& add, const unsigned int& maxlimit) {
	while (true) {
		if (LimitedAdd_weak(add, maxlimit)) {
			return;
		}
		std::this_thread::yield();
	}
}
bool tapi_atomic_uint::LimitedSubtract_weak(const unsigned int& subtract, const unsigned int& minlimit) {
	unsigned int x = data.load();
	if (x - subtract < minlimit ||	//Subtraction is bigger
		x - subtract > x				//UINT overflow
		) {
		return false;
	}
	if (!data.compare_exchange_strong(x, x - subtract)) {
		return LimitedSubtract_weak(subtract, minlimit);
	}
	return true;
}
void tapi_atomic_uint::LimitedSubtract_strong(const unsigned int& subtract, const unsigned int& minlimit) {
	while (true) {
		if (LimitedSubtract_weak(subtract, minlimit)) {
			return;
		}
		std::this_thread::yield();
	}
}
*/