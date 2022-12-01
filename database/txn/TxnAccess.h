// NOTICE: this file is adapted from Cavalia
#ifndef __DATABASE_TXN_TXN_ACCESS_H__
#define __DATABASE_TXN_TXN_ACCESS_H__

// #include "Record.h"
#include "TableRecord.h"
#include "gallocator.h"

namespace Database {
struct Access {
  Access()
      : access_record_(nullptr), access_addr_(Gnullptr), timestamp_(0) {
  }
  AccessType access_type_;
  // Record *access_record_;
  TableRecord* access_record_;
  GAddr access_addr_;
  uint64_t timestamp_;
};

template<int N>
class AccessList {
 public:
  AccessList()
      : access_count_(0) {
  }

  Access *NewAccess() {
    assert(access_count_ < N);
    Access *ret = &(accesses_[access_count_]);
    ++access_count_;
    return ret;
  }

  Access *GetAccess(const size_t &index) {
    return &(accesses_[index]);
  }

  void Clear() {
    access_count_ = 0;
  }

  void Sort(){
    std::sort(accesses_, accesses_ + access_count_, GAddrCompFunction);
  }

  private:
    static bool CompFunction(Access lhs, Access rhs){
      return (uint64_t)(lhs.access_record_) < (uint64_t)(rhs.access_record_);
    }

    static bool GAddrCompFunction(Access lhs, Access rhs){
      return lhs.access_addr_ < rhs.access_addr_;
    }

 public:
  size_t access_count_;
 private:
  Access accesses_[N];
};
}

#endif
