#pragma once
#ifndef __DATABASE_LOCK_CONTENT_WITH_TS_H__
#define __DATABASE_LOCK_CONTENT_WITH_TS_H__

#include <atomic>
#include <assert.h>
#include "GAMObject.h"

namespace Database {
    class LockContentWithTs : public GAMObject{
    public:
        LockContentWithTs() : timestamp_(0) {}

        void SetTimestamp(const uint64_t &timestamp) {
            assert(timestamp_ <= timestamp);
            timestamp_.store(timestamp, std::memory_order_relaxed);
        }

        uint64_t GetTimestamp() const {
            return timestamp_.load(std::memory_order_relaxed);
        }

        // Write the content to the global memory addr
        void Serialize(const GAddr& addr, GAlloc *gallocator) {
            uint64_t ts = GetTimestamp();
            gallocator->Write(addr, &ts, sizeof(ts));
        }
        // Read the content from the global memory addr
        void Deserialize(const GAddr& addr, GAlloc *gallocator) {
            uint64_t ts;
            gallocator->Read(addr, &ts, sizeof(ts));
            SetTimestamp(ts);
        }

        static size_t GetSerializeSize() {
            return sizeof(uint64_t);
        }

    private:
        std::atomic<uint64_t> timestamp_;
    };  // class LockContentWithTs
};  // namespace Database

#endif
