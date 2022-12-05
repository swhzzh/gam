// NOTICE: this file is adapted from Cavalia
#ifndef __DATABASE_TXN_TRANSACTION_MANAGER_H__
#define __DATABASE_TXN_TRANSACTION_MANAGER_H__

#include <iostream>
#include <vector>

#include "Meta.h"
#include "StorageManager.h"
#include "TimestampManager.h"
#include "Record.h"
#include "Records.h"
#include "TableRecord.h"
#include "TableRecords.h"
#include "TxnParam.h"
#include "CharArray.h"
#include "TxnContext.h"
#include "TxnAccess.h"
// #include "Profiler.h"
#include "Profilers.h"
#include "log.h"

#include "Epoch.h"

namespace Database {
class TransactionManager {
 public:
  TransactionManager(StorageManager* storage_manager, TimestampManager* ts_manager, size_t thread_count,
                     size_t thread_id)
      : storage_manager_(storage_manager),
        ts_manager_(ts_manager),
        thread_count_(thread_count),
        thread_id_(thread_id) {
  }
  ~TransactionManager() {
  }

  bool InsertRecord(TxnContext* context, size_t table_id, const IndexKey* keys,
                    size_t key_num, Record *record, const GAddr& data_addr);

  bool SearchRecord(TxnContext* context, size_t table_id,
                    const IndexKey& primary_key, Record *&record,
                    AccessType access_type) {
    BEGIN_PHASE_MEASURE(thread_id_, INDEX_READ);
    GAddr data_addr = storage_manager_->tables_[table_id]->SearchRecord(
        primary_key, gallocators[thread_id_], thread_id_);
    END_PHASE_MEASURE(thread_id_, INDEX_READ);
    if (data_addr != Gnullptr) {
      bool ret = SelectRecordCC(context, table_id, record, data_addr,
                                access_type);
      return ret;
    } else {
      epicLog(LOG_WARNING, "table_id=%d cannot find the record with  key=%lx",
          table_id, primary_key);
      return false;
    }
  }

  bool SearchRecords(TxnContext* context, size_t table_id, size_t index_id,
                     const IndexKey& secondary_key, Records *records,
                     AccessType access_type) {
    epicLog(LOG_FATAL, "not supported for now");
    return true;
  }

  bool CommitTransaction(TxnContext* context, TxnParam* param,
                         CharArray& ret_str);

  void AbortTransaction();

  size_t GetThreadId() const {
    return thread_id_;
  }

 private:
  bool SelectRecordCC(TxnContext* context, size_t table_id,
                      Record *&record, const GAddr &data_addr,
                      AccessType access_type);

  bool TryWLockRecord(const GAddr& data_addr, size_t data_size) {
    epicLog(LOG_DEBUG, "TryWLockRecord, this=%p, data_addr=%lx, data_size=%d",
            this, data_addr, data_size);
    bool success = true;
    size_t try_count = 0;
    while (gallocators[thread_id_]->Try_WLock(data_addr, data_size) != 0) {
      if (++try_count >= kTryLockLimit) {
        success = false;
        break;
      }
    }
    return success;
  }

  bool TryRLockRecord(const GAddr& data_addr, size_t data_size) {
    epicLog(LOG_DEBUG, "TryRLockRecord, this=%p, data_addr=%lx, data_size=%d",
            this, data_addr, data_size);
    bool success = true;
    size_t try_count = 0;
    while (gallocators[thread_id_]->Try_RLock(data_addr, data_size) != 0) {
      if (++try_count >= kTryLockLimit) {
        success = false;
        break;
      }
    }
    return success;
  }

  void RLockRecord(const GAddr& data_addr, size_t data_size) {
    epicLog(LOG_DEBUG, "RLockRecord, this=%p, data_addr=%lx, data_size=%d",
            this, data_addr, data_size);
    gallocators[thread_id_]->RLock(data_addr, data_size);
  }

  void WLockRecord(const GAddr& data_addr, size_t data_size) {
    epicLog(LOG_DEBUG, "WLockRecord, this=%p, data_addr=%lx, data_size=%d",
            this, data_addr, data_size);
    gallocators[thread_id_]->WLock(data_addr, data_size);
  }

  void UnLockRecord(const GAddr &data_addr, size_t data_size) {
    epicLog(LOG_DEBUG, "this=%p, data_addr=%lx, data_size=%d",
        this, data_addr, data_size);
    gallocators[thread_id_]->UnLock(data_addr, data_size);
  }

  uint64_t GenerateScalableTimestamp(const uint64_t &curr_epoch, const uint64_t &max_rw_ts){
    uint64_t max_global_ts = max_rw_ts >> 32;
    uint32_t max_local_ts = max_rw_ts & 0xFFFFFFFF;
    assert(curr_epoch >= max_global_ts);
    assert(curr_epoch >= this->local_epoch_);
    // init.
    if (curr_epoch > this->local_epoch_) {
      this->local_epoch_ = curr_epoch;
      this->local_ts_ = this->thread_id_;
    }
    assert(this->local_epoch_ == curr_epoch);
    // compute commit timestamp.
    if (curr_epoch == max_global_ts) {
      if (this->local_ts_ <= max_local_ts) {
        this->local_ts_ = (max_local_ts / thread_count_ + 1)*thread_count_ + thread_id_;
        assert(this->local_ts_ > max_local_ts);
      }
      assert(this->local_ts_ > max_local_ts);
    }
    assert(this->local_epoch_ == max_global_ts && this->local_ts_ >= max_local_ts || this->local_epoch_ > max_global_ts);

    uint64_t commit_ts = (this->local_epoch_ << 32) | this->local_ts_;
    assert(commit_ts >= max_rw_ts);
    return commit_ts;
  }

  uint64_t GenerateMonotoneTimestamp(const uint64_t &curr_epoch, const uint64_t &monotone_ts){
    uint32_t lower_bits = monotone_ts & 0xFFFFFFFF;
    uint64_t commit_ts = (curr_epoch << 32) | lower_bits;
    return commit_ts;
  }

  uint64_t GetEpoch() {
    return ts_manager_->GetEpoch(gallocators[thread_id_]);
  }

  uint64_t GetMonotoneTimestamp() {
    return ts_manager_->GetMonotoneTimestamp(gallocators[thread_id_]);
  }

 public:
  StorageManager* storage_manager_;
  TimestampManager* ts_manager_;

 protected:
  size_t thread_id_;
  size_t thread_count_;
  // uint64_t start_timestamp_;
  // bool is_first_access_;
  uint64_t local_epoch_;
  uint32_t local_ts_;

  AccessList<kMaxAccessLimit> access_list_;

#if defined(SILO)
  // write set.
  AccessPtrList<kMaxAccessNum> write_list_;
#endif
};
}

#endif
