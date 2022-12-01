#ifndef __DATABASE_TXN_TIMESTAMP_MANAGER_H__
#define __DATABASE_TXN_TIMESTAMP_MANAGER_H__

#include "Epoch.h"
#include "GlobalTimestamp.h"
#include "BatchTimestamp.h"

namespace Database {
  class TimestampManager
  {
  public:
    TimestampManager(GAddr epoch_addr, GAddr monotone_ts_addr, bool is_master, GAlloc* default_gallocator) : 
      epoch_(epoch_addr, is_master, default_gallocator), g_timestamp_(monotone_ts_addr) {

    }
    ~TimestampManager();

    uint64_t GetEpoch(GAlloc* gallocator) {
      return epoch_.GetEpoch(gallocator);
    }

		uint64_t GetMonotoneTimestamp(GAlloc* gallocator){
      return g_timestamp_.GetMonotoneTimestamp(gallocator);
		}

		uint64_t GetBatchMonotoneTimestamp(GAlloc* gallocator){
      return g_timestamp_.GetBatchMonotoneTimestamp(gallocator);
		}

  private:
    Epoch epoch_;
    GlobalTimestamp g_timestamp_;
  };  // class TimestampManager
};  // namespace Database

#endif