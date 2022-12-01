#if defined(OCC)
#include "TransactionManager.h"

namespace Database {
	bool TransactionManager::InsertRecord(TxnContext* context,
																				size_t table_id,
																				const IndexKey* keys,
																				size_t key_num,
																				Record *record,
																				const GAddr& data_addr) {
			BEGIN_PHASE_MEASURE(thread_id_, CC_INSERT);
			// insert with visibility bit set to false.
			record->SetVisible(false);
			TableRecord *table_record = new TableRecord(record);
			// the to-be-inserted record may have already existed.
			//if (storage_manager_->tables_[table_id]->InsertRecord(primary_key, tb_record) == true){
				Access *access = access_list_.NewAccess();
				access->access_type_ = INSERT_ONLY;
				access->access_record_ = table_record;
				access->access_addr_ = data_addr;
				// access->table_id_ = table_id;
				access->timestamp_ = 0;
				END_PHASE_MEASURE(thread_id_, CC_INSERT);
				return true;
			/*}
			else{
				// if the record has already existed, then we need to lock the original record.
				END_PHASE_MEASURE(thread_id_, CC_INSERT);
				return true;
			}*/
	}

  bool TransactionManager::SelectRecordCC(TxnContext* context,
                                          size_t table_id, 
                                          Record*& record,
                                          const GAddr &data_addr, 
                                          AccessType access_type) {
    epicLog(LOG_DEBUG, "thread_id=%u,table_id=%u,access_type=%u,data_addr=%lx, start SelectRecordCC", 
        thread_id_, table_id, access_type, data_addr);
    BEGIN_PHASE_MEASURE(thread_id_, CC_SELECT);
    RecordSchema *schema_ptr = storage_manager_->tables_[table_id]->GetSchema();
    record = new Record(schema_ptr);
    TableRecord* table_record = new TableRecord(record);
		table_record->Deserialize(data_addr, gallocators[thread_id_]);

		Access* access = access_list_.NewAccess();
		access->access_type_ = access_type;
		access->access_record_ = table_record;
		access->access_addr_ = data_addr;
		access->timestamp_ = table_record->content_.GetTimestamp();
		if (access_type == DELETE_ONLY) {
			record->SetVisible(false);
		}
		END_PHASE_MEASURE(thread_id_, CC_SELECT);
		return true;
	}

	bool TransactionManager::CommitTransaction(TxnContext *context, TxnParam *param, CharArray &ret_str){
		BEGIN_PHASE_MEASURE(thread_id_, CC_COMMIT);
		// step 1: acquire lock and validate
		size_t lock_count = 0;
		bool is_success = true;
		access_list_.Sort();
		for (size_t i = 0; i < access_list_.access_count_; ++i) {
			++lock_count;
			Access* access_ptr = access_list_.GetAccess(i);
			TableRecord* table_record = access_ptr->access_record_;
			auto &content_ref = table_record->content_;
			if (access_ptr->access_type_ == READ_ONLY) {
				RLockRecord(access_ptr->access_addr_, table_record->GetSerializeSize());
				// whether someone has changed the tuple after my read
				if (content_ref.GetTimestamp() != access_ptr->timestamp_) {
					UPDATE_CC_ABORT_COUNT(thread_id_, context->txn_type_, access_ptr->table_id_);
					is_success = false;
					break;
				}
			}
			else if (access_ptr->access_type_ == READ_WRITE) {
				// acquire write lock
				WLockRecord(access_ptr->access_addr_, table_record->GetSerializeSize());
				// whether someone has changed the tuple after my read
				if (content_ref.GetTimestamp() != access_ptr->timestamp_) {
					UPDATE_CC_ABORT_COUNT(thread_id_, context->txn_type_, access_ptr->table_id_);
					is_success = false;
					break;
				}
			}
			else {
				// insert_only or delete_only
				WLockRecord(access_ptr->access_addr_, table_record->GetSerializeSize());
			}
		}

		// step 2: if success, then overwrite and commit
		if (is_success == true) {
			BEGIN_CC_TS_ALLOC_TIME_MEASURE(thread_id_);
			uint64_t curr_epoch = GetEpoch();
// TODO(weihaosun): implement silo timestamp
#if defined(SCALABLE_TIMESTAMP)
			uint64_t max_rw_ts = 0;
			for (size_t i = 0; i < access_list_.access_count_; ++i){
				Access *access_ptr = access_list_.GetAccess(i);
				if (access_ptr->timestamp_ > max_rw_ts){
					max_rw_ts = access_ptr->timestamp_;
				}
			}
			uint64_t commit_ts = GenerateScalableTimestamp(curr_epoch, max_rw_ts);
#else
			uint64_t commit_ts = GenerateMonotoneTimestamp(curr_epoch, GetMonotoneTimestamp());
#endif
			END_CC_TS_ALLOC_TIME_MEASURE(thread_id_);

			for (size_t i = 0; i < access_list_.access_count_; ++i) {
				Access *access_ptr = access_list_.GetAccess(i);
				// SchemaRecord *global_record_ptr = access_ptr->access_record_->record_;
				// SchemaRecord *local_record_ptr = access_ptr->local_record_;
				TableRecord* table_record = access_ptr->access_record_;
				GAddr& access_addr = access_ptr->access_addr_;
				auto &content_ref = table_record->content_;
				if (access_ptr->access_type_ == READ_WRITE) {
					assert(commit_ts > access_ptr->timestamp_);
					// global_record_ptr->CopyFrom(local_record_ptr);
					// TODO(weihaosun): do not need local memory fence?
					// COMPILER_MEMORY_FENCE;
					content_ref.SetTimestamp(commit_ts);
					table_record->Serialize(access_addr, gallocators[thread_id_]);
				}
				else if (access_ptr->access_type_ == INSERT_ONLY) {
					assert(commit_ts > access_ptr->timestamp_);
					table_record->record_->SetVisible(true);
					// COMPILER_MEMORY_FENCE;
					content_ref.SetTimestamp(commit_ts);
					table_record->Serialize(access_addr, gallocators[thread_id_]);
				}
				else if (access_ptr->access_type_ == DELETE_ONLY) {
					assert(commit_ts > access_ptr->timestamp_);
					table_record->record_->SetVisible(false);
					// COMPILER_MEMORY_FENCE;
					content_ref.SetTimestamp(commit_ts);
					table_record->Serialize(access_addr, gallocators[thread_id_]);
				}
			}
			// commit.
#if defined(VALUE_LOGGING)
			logger_->CommitTransaction(this->thread_id_, curr_epoch, commit_ts, access_list_);
#elif defined(COMMAND_LOGGING)
			if (context->is_adhoc_ == true){
				logger_->CommitTransaction(this->thread_id_, curr_epoch, commit_ts, access_list_);
			}
			logger_->CommitTransaction(this->thread_id_, curr_epoch, commit_ts, context->txn_type_, param);
#endif

			// step 3: release locks and clean up.
			for (size_t i = 0; i < access_list_.access_count_; ++i) {
				Access *access_ptr = access_list_.GetAccess(i);
				// unlock
				this->UnLockRecord(access->access_addr_, access->access_record_->GetSerializeSize());
			}

			//GC
			for (size_t i = 0; i < access_list_.access_count_; ++i) {
				Access* access = access_list_.GetAccess(i);
				if (access->access_type_ == DELETE_ONLY) {
					gallocators[thread_id_]->Free(access->access_addr_);
					access->access_addr_ = Gnullptr;
				}
				delete access->access_record_;
				access->access_record_ = nullptr;
				access->access_addr_ = Gnullptr;
			}
		}
		// if failed.
		else {
			// step 3: release locks and clean up.
			for (size_t i = 0; i < access_list_.access_count_; ++i) {
				Access *access_ptr = access_list_.GetAccess(i);
				// unlock
				this->UnLockRecord(access->access_addr_, access->access_record_->GetSerializeSize());
				--lock_count;
				if (lock_count == 0) {
					break;
				}
			}

			//GC
			for (size_t i = 0; i < access_list_.access_count_; ++i) {
				Access* access = access_list_.GetAccess(i);
				delete access->access_record_;
				access->access_record_ = nullptr;
				access->access_addr_ = Gnullptr;
			}
		}
		assert(access_list_.access_count_ <= kMaxAccessNum);
		access_list_.Clear();
		END_PHASE_MEASURE(thread_id_, CC_COMMIT);
		return is_success;
	}

	void TransactionManager::AbortTransaction() {
		assert(false);
	}
}


#endif
