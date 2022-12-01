#pragma once
#ifndef __CAVALIA_DATABASE_EPOCH_H__
#define __CAVALIA_DATABASE_EPOCH_H__

#include <cstdint>
#include <boost/thread.hpp>
#include "gallocator.h"

#if defined(__linux__)
#define COMPILER_MEMORY_FENCE asm volatile("" ::: "memory")
#else
#define COMPILER_MEMORY_FENCE
#endif

namespace Database {
	class Epoch {
	public:
		// Epoch() : 
		// 	epoch_addr_(Gnullptr), ts_thread_(NULL), is_master_(false), default_gallocator_(NULL){
		// 		// empty
		// }
		Epoch() = delete;

		Epoch(GAddr epoch_addr, bool is_master, GAlloc* default_gallocator) : 
			epoch_addr_(Gnullptr), ts_thread_(NULL), is_master_(is_master), default_gallocator_(default_gallocator) {
			if (is_master) {
				ts_thread_ = new boost::thread(boost::bind(&Epoch::Start, this));
			}
		}

		~Epoch() {
			if (!ts_thread_) {
				delete ts_thread_;
				ts_thread_ = NULL;
			}
		}

		// called by each node/thread with its own gallocator
		uint64_t GetEpoch(GAlloc* gallocator) {
			gallocator->RLock(epoch_addr_, sizeof(uint64_t));
			uint64_t cur_epoch;
			gallocator->Read(epoch_addr_, &cur_epoch, sizeof(uint64_t));
			gallocator->UnLock(epoch_addr_, sizeof(uint64_t));
			return cur_epoch;
		}

	private:
		void Start() {
			while (true) {
				boost::this_thread::sleep(boost::posix_time::milliseconds(40));
				default_gallocator_->WLock(epoch_addr_, sizeof(uint64_t));
				uint64_t cur_epoch;
				default_gallocator_->Read(epoch_addr_, &cur_epoch, sizeof(uint64_t));
				cur_epoch += 1;
				default_gallocator_->Write(epoch_addr_, &cur_epoch, sizeof(cur_epoch));
				default_gallocator_->UnLock(epoch_addr_, sizeof(uint64_t));
			}
		}

	private:
		// static volatile uint64_t curr_epoch_;
		GAddr epoch_addr_;
		boost::thread *ts_thread_;
		bool is_master_;
		GAlloc* default_gallocator_;
	};
}


#endif
