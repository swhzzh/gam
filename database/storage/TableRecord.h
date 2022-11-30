#pragma once
#ifndef __DATABASE_TABLE_RECORD_H__
#define __DATABASE_TABLE_RECORD_H__

#include "Record.h"

#if defined(LOCK_WAIT)
#include "../content/LockWaitContent.h"
#elif defined(LOCK) || defined(ST)
#include "../content/LockContent.h"
#elif defined(OCC) || defined(SILO)
#include "../content/LockContentWithTs.h"
#elif defined(SILOCK) || defined(SIOCC)
#include "../content/SiLockContent.h"
#elif defined(TO)
#include "../content/ToContent.h"
#elif defined(MVTO)
#include "../content/MvToContent.h"
#elif defined(TVLOCK)
#include "../content/TvLockContent.h"
#elif defined(MVLOCK)
#include "../content/MvLockContent.h"
#elif defined(MVLOCK_WAIT)
#include "../content/MvLockWaitContent.h"
#elif defined(MVOCC)
#include "../content/MvOccContent.h"
#elif defined(DBX)
#include "../content/DbxContent.h"
#elif defined(RTM)
#include "../content/RtmContent.h"
#elif defined(OCC_RTM) || defined(LOCK_RTM)
#include "../content/LockRtmContent.h"
#endif

namespace Database{
#ifdef __linux__
    class __attribute__((aligned(64))) TableRecord : public GAMObject {
#else
    class TableRecord : public GAMObject {
#endif
public:
    // Write the content to the global memory addr
    void Serialize(const GAddr& addr, GAlloc *gallocator) {
        // serialize record first
        record_->Serialize(addr, gallocator);
        // serialize content
        size_t off  = record_->GetSerializeSize();
        const GAddr& content_addr = GADD(addr, off);
        content_.Serialize(content_addr, gallocator);
    }
    // Read the content from the global memory addr
    void Deserialize(const GAddr& addr, GAlloc *gallocator) {
        record_->Deserialize(addr, gallocator);
        size_t off = record_->GetSerializeSize();
        const GAddr& content_addr = GADD(addr, off);
        content_.Deserialize(content_addr, gallocator);
    }

    size_t GetSerializeSize() {
        return record_->GetSerializeSize() + content_.GetSerializeSize();
    }

    // static size_t GetContentSerializeSize() {
    //     return content_.GetSerializeSize();
    // }

#if defined(MVLOCK_WAIT) || defined(MVLOCK) || defined(MVOCC) || defined(MVTO) || defined(SILOCK) || defined(SIOCC)
    TableRecord(Record *record) : record_(record), content_(record->data_ptr_) {}
#elif defined(TO)
    TableRecord(Record *record) : record_(record), content_(record->data_ptr_, record->schema_ptr_->GetSchemaSize()) {}
#else
    // TableRecord(RecordSchema *schema_ptr) : record_(new Record(schema_ptr)) {}
    TableRecord(Record *record) : record_(record) {}
#endif
    // NOTE(weihaosun): table_record takes ownership of record now, thus is responsible to free record when deconstructing
    ~TableRecord(){
        delete record_;
    }
    
    Record* record_;

#if defined(LOCK_WAIT)
        LockWaitContent content_;
#elif defined(LOCK) || defined(ST)
        LockContent content_;
#elif defined(OCC) || defined(SILO)
        LockContentWithTs content_;
#elif defined(SILOCK) || defined(SIOCC)
        SiLockContent content_;
#elif defined(TO)
        ToContent content_;
#elif defined(MVTO)
        MvToContent content_;
#elif defined(TVLOCK)
        TvLockContent content_;
#elif defined(MVLOCK)
        MvLockContent content_;
#elif defined(MVLOCK_WAIT)
        MvLockWaitContent content_;
#elif defined(MVOCC)
        MvOccContent content_;
#elif defined(DBX)
        DbxContent content_;
#elif defined(RTM)
        RtmContent content_;
#elif defined(OCC_RTM) || defined(LOCK_RTM)
        LockRtmContent content_;
#endif
    };
}


#endif
