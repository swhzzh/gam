#pragma once
#ifndef __DATABASE_LOCK_CONTENT_H__
#define __DATABASE_LOCK_CONTENT_H__

#include "GAMObject.h"

namespace Database {
    class LockContent : public GAMObject {

        LockContent() = default;
        ~LockContent() = default;

        // Write the content to the global memory addr
        void Serialize(const GAddr& addr, GAlloc *gallocator) {
            // empty
        }
        // Read the content from the global memory addr
        void Deserialize(const GAddr& addr, GAlloc *gallocator) {
            // empty
        }

        static size_t GetSerializeSize() {
            return 0;
        }
    };  


};  // namespace Database

#endif