#ifndef __DATABASE_UTILS_CLUSTER_SYNCHRONIZER_H__
#define __DATABASE_UTILS_CLUSTER_SYNCHRONIZER_H__

#include "gallocator.h"
#include "ClusterConfig.h"
#include <unordered_map>

namespace Database {

enum SyncType {
  STORAGE_MANAGER,
  EPOCH,
  MONOTONE_TIMESTAMP
};

class ClusterSync{
public:
  ClusterSync(ClusterConfig *config) : config_(config) {
    // storage_manager_sync_key_ = 1;
    // epoch_sync_key_ = 1;
    // timestamp_sync_key_ = 1;
    // storage_manager_prefix_ = SyncType::STORAGE_MANAGER;
    // epoch_prefix_ = SyncType::EPOCH;
    // timestamp_prefix_ = SyncType::TIMESTAMP;
    sync_key_mp_[SyncType::STORAGE_MANAGER] = 1;
    sync_key_mp_[SyncType::EPOCH] = 1;
    sync_key_mp_[SyncType::MONOTONE_TIMESTAMP] = 1;
    prefix_mp_[SyncType::STORAGE_MANAGER] = 1;
    prefix_mp_[SyncType::EPOCH] = 2;
    prefix_mp_[SyncType::MONOTONE_TIMESTAMP] = 3;
  }

  void Fence() {
    size_t partition_id = config_->GetMyPartitionId();
    size_t partition_num = config_->GetPartitionNum();
    bool *flags = new bool[partition_num];
    memset(flags, 0, sizeof(bool)*partition_num);
    this->MasterCollect<bool>(flags + partition_id, flags);
    this->MasterBroadcast<bool>(flags + partition_id);
    delete[] flags;
    flags = nullptr;
  }

  template<class T>
  void MasterCollect(T *send, T *receive) {
    MasterCollect<T>(send, receive, SyncType::STORAGE_MANAGER);
    // T data;
    // size_t partition_id = config_->GetMyPartitionId();
    // size_t partition_num = config_->GetPartitionNum();
    // if (config_->IsMaster()) {
    //   for (size_t i = 0; i < partition_num; ++i) {
    //     if (i != partition_id) {
    //       default_gallocator->Get(
    //           (uint64_t)(sync_key_ + i), &data);
    //       memcpy(receive + i, &data, sizeof(T));
    //     }
    //     else {
    //       memcpy(receive + i, send, sizeof(T));
    //     }
    //   }
    // }
    // else {
    //   default_gallocator->Put((uint64_t)
    //       (sync_key_ + partition_id), send, sizeof(T));
    // }
    // sync_key_ += partition_num;
  }

  template<class T>
  void MasterCollect(T *send, T *receive, SyncType sync_type) {
    T data;
    size_t partition_id = config_->GetMyPartitionId();
    size_t partition_num = config_->GetPartitionNum();
    uint64_t sync_key = getSyncKey(sync_type);
    if (config_->IsMaster()) {
      for (size_t i = 0; i < partition_num; ++i) {
        if (i != partition_id) {
          default_gallocator->Get(
              (uint64_t)(sync_key + i), &data);
          memcpy(receive + i, &data, sizeof(T));
        }
        else {
          memcpy(receive + i, send, sizeof(T));
        }
      }
    }
    else {
      default_gallocator->Put((uint64_t)
          (sync_key + partition_id), send, sizeof(T));
    }
    sync_key += partition_num;
  }

  template<class T>
  void MasterBroadcast(T *send) {
    MasterBroadcast<T>(send, SyncType::STORAGE_MANAGER);
    // size_t partition_id = config_->GetMyPartitionId();
    // size_t partition_num = config_->GetPartitionNum();
    // if (config_->IsMaster()) {
    //   default_gallocator->Put(
    //       (uint64_t)(sync_key_ + partition_id), send, sizeof(T));
    // }
    // else {
    //   const size_t master_partition_id = 0;
    //   default_gallocator->Get((uint64_t)
    //       (sync_key_ + master_partition_id), send);
    // }
    // sync_key_ += partition_num;
  }

  template<class T>
  void MasterBroadcast(T *send, SyncType sync_type) {
    size_t partition_id = config_->GetMyPartitionId();
    size_t partition_num = config_->GetPartitionNum();
    uint64_t sync_key = getSyncKey(sync_type);
    if (config_->IsMaster()) {
      default_gallocator->Put(
          (uint64_t)(sync_key + partition_id), send, sizeof(T));
    }
    else {
      const size_t master_partition_id = 0;
      default_gallocator->Get((uint64_t)
          (sync_key + master_partition_id), send);
    }
    sync_key += partition_num;
  }

private:
  uint64_t getSyncKey(SyncType sync_type) {
    uint64_t sync_key = sync_key_mp_[sync_type];
    uint64_t prefix = prefix_mp_[sync_type];
    return combine(prefix, sync_key);
  }

  uint64_t combine(uint64_t prefix, uint64_t sync_key) {
    uint32_t lower_bits = sync_key & 0xFFFFFFFF;
    return (prefix << 32) | lower_bits;
  }

private:
  ClusterConfig *config_;
  // uint64_t storage_manager_sync_key_;
  // uint64_t epoch_sync_key_;
  // uint64_t timestamp_sync_key_;
  // uint64_t storage_manager_prefix_;
  // uint64_t epoch_prefix_;
  // uint64_t timestamp_prefix_;
  std::unordered_map<SyncType, uint64_t> sync_key_mp_;
  std::unordered_map<SyncType, uint64_t> prefix_mp_;
};
}

#endif
