#include <string>
#include <array>
#include <stdio.h>
#include <stdlib.h>
#include <leveldb/options.h>
#include "leveldb/db.h"
#include "leveldb/write_batch.h"

#include <emscripten.h>
#include <emscripten/bind.h>

using namespace std;
using namespace emscripten;

enum OptionsItem {
  COMPARATOR = 1,
  CREATE_IF_MISSING,
  ERROR_IF_EXISTS,
  PARANOID_CHECKS,
  ENV,
  INFO_LOG,
  WRITE_BUFFER_SIZE,
  MAX_OPEN_FILES,
  BLOCK_CACHE,
  BLOCK_SIZE,
  BLOCK_RESTART_INTERVAL,
  MAX_FILE_SIZE,
  COMPRESSION,
  ZSTD_COMPRESSION_LEVEL,
  REUSE_LOGS,
  FILTER_POLICY,
  /** Read Options */
  VERIFY_CHECKSUMS = 128,
  FILL_CACHE,
  SNAPSHOT,

  /** Write Options */
  SYNC
};

struct leveldb_iterator {
  leveldb::Iterator* rep;
  string limit;
};

struct leveldb_batch {
  leveldb::WriteBatch* rep;
};

typedef struct leveldb_iterator LevelDBIterator;
typedef struct leveldb_batch LevelDBBatch;

struct KeyValue {
  int keySize;
  int valueSize;
  string key;
  string value;
  string* errptr;
};

typedef struct KeyValue KV;

#define POINTER_STRUCT(Type, var)  Type* var = (Type* )malloc(sizeof(Type));

class LevelDB {
  public:
    LevelDB() {
      options_.create_if_missing = true;
      options_.compression = leveldb::kNoCompression;
      // writeOptions_.sync = true;
    }

    /** @todo 支持指针类型的选项设置 */
    bool setOption(int option, val value) {
      printf("error_if_exists %d, %d", option, value.as<int>(allow_raw_pointers()));

      switch(option) {

        case CREATE_IF_MISSING:
          options_.create_if_missing = value.as<int>(allow_raw_pointers());
          break;

        case ERROR_IF_EXISTS:
          options_.error_if_exists = value.as<int>(allow_raw_pointers());
          printf("error_if_exists %d", options_.error_if_exists);
          break;

        case PARANOID_CHECKS:
          options_.paranoid_checks = value.as<int>(allow_raw_pointers());
          break;

        case WRITE_BUFFER_SIZE:
          options_.write_buffer_size = value.as<size_t>(allow_raw_pointers());
          break;

        case MAX_OPEN_FILES:
          options_.max_open_files = value.as<int>(allow_raw_pointers());
          break;

        case BLOCK_SIZE:
          options_.block_size = value.as<size_t>(allow_raw_pointers());
          break;

        case BLOCK_RESTART_INTERVAL:
          options_.block_restart_interval = value.as<int>(allow_raw_pointers());
          break;

        case MAX_FILE_SIZE:
          options_.max_file_size = value.as<size_t>(allow_raw_pointers());
          break;

        case COMPRESSION:
          options_.compression = (leveldb::CompressionType) value.as<int>(allow_raw_pointers());
          break;
        
        case ZSTD_COMPRESSION_LEVEL:
          options_.zstd_compression_level = value.as<int>(allow_raw_pointers());
          break;
        
        case REUSE_LOGS:
          options_.reuse_logs = value.as<int>(allow_raw_pointers());
          break;

        /** Setting read option */
        case VERIFY_CHECKSUMS:
          readOptions_.verify_checksums = value.as<int>(allow_raw_pointers());
          break;
        
        case FILL_CACHE:
          readOptions_.fill_cache = value.as<int>(allow_raw_pointers());
          break;
        
        /** Setting write option */
        case SYNC:
          writeOptions_.sync = value.as<int>(allow_raw_pointers());
          break;
        
        default:
          return false;
      }

      return true;
    }

    void compaction() {
      db_->MayBeCompaction();
    }

    bool open(string path) {
      leveldb::DB* db;
      leveldb::Status s = leveldb::DB::Open(options_, path, &db);
      db_ = db;
      if (s.ok()) {
        printf("opened!");
        return true;
      }
      // error handler/save error.
      // saveError(s);
      return false;
    }

    string get(string key) {
      string value;
      leveldb::Status s = db_->Get(readOptions_, key, &value);
      if (s.ok()) {
        return value;
      }

      return "";
    }

    void saveError(const leveldb::Status& s) {
      lastError = s.ToString();
    }

    string getError() {
      return lastError;
    }

    bool put(string key, string value) {
      leveldb::Status s = db_->Put(writeOptions_, key, value);
      return s.ok();
    }

    bool remove(string key) {
      leveldb::Status s = db_->Delete(writeOptions_, key);
      return s.ok();
    }

    /**
     * 替换KEY - 用来替换指定Key的Key值
     */
    bool replaceKey(string key1, string key2) {
      string value;
      leveldb::Status s = db_->Get(readOptions_, key1, &value);
      if (s.ok()) {
        leveldb::WriteBatch* batch;
        batch->Delete(key1);
        batch->Put(key2, value);
        s = db_->Write(writeOptions_, batch);
        return s.ok();
      }
      return false;
    }

    LevelDBBatch *openBatch() {
      LevelDBBatch* it = new LevelDBBatch;
      leveldb::WriteBatch *batch_;
      it->rep = batch_;
      return it;
    }

    void batchPut(LevelDBBatch *it, string key, string value) {
      it->rep->Put(key, value);
    }

    void batchDelete(LevelDBBatch *it, string key) {
      it->rep->Delete(key);
    }

    void clearBatch(LevelDBBatch* it) {
      it->rep->Clear();
    }

    bool writeBatch(LevelDBBatch* it) {
      leveldb::Status s = db_->Write(writeOptions_, it->rep);
      return s.ok();
    }

    void closeBatch(LevelDBBatch *it) {
      delete it->rep;
      delete it;
    }

    LevelDBIterator *iterator() {
      LevelDBIterator* it = new LevelDBIterator;
      leveldb::Iterator* it_ = db_->NewIterator(readOptions_);
      it->rep = it_;
      return it;
    }

    void closeIterator(LevelDBIterator* it) {
      delete it->rep;
      delete it;
    }

    void seekToFirstOrLast(LevelDBIterator* it, bool toFirst = true) {
      leveldb::Iterator* it_ = it->rep;
      if (toFirst) {
        it_->SeekToFirst();
      } else {
        it_->SeekToLast();
      }
    }

    KeyValue next(LevelDBIterator* it) {
      leveldb::Iterator* it_ = it->rep;

      KeyValue kv = {0};
      if (!it_->Valid()) return kv;
      if (!it->limit.empty() && it_->key().ToString() >= it->limit) return kv;
      
      setKV(it_, &kv);
      it_->Next();
      return kv;
    }

    KeyValue prev(LevelDBIterator* it) {
      leveldb::Iterator* it_ = it->rep;

      KeyValue kv = {0};
      if (!it_->Valid()) return kv;
      if (!it->limit.empty() && it_->key().ToString() <= it->limit) return kv;

      setKV(it_, &kv);
      it_->Prev();
      return kv;
    }

    void seek(LevelDBIterator* it, string target) {
      leveldb::Iterator *it_ = it->rep;
      it_->Seek(target);
    }

    void setIteratorLimit(LevelDBIterator* it, string limit) {
      it->limit = limit;
    }

    bool RepairDB(string dbname) {
      leveldb::Status s = leveldb::RepairDB(dbname, options_);
      return s.ok();
    }

    ~LevelDB() {
      delete db_;
      delete &options_;
      delete &readOptions_;
      delete &writeOptions_;
    }

  private:

    void setKV(leveldb::Iterator* it_, KV* kv) {
      string key = it_->key().ToString();
      kv->keySize = key.length();
      kv->key = key;
      string value = it_->value().ToString();
      kv->valueSize = value.length();
      kv->value = value;
    }

    leveldb::DB* db_;

    string lastError;

    leveldb::Options options_;
    leveldb::ReadOptions readOptions_;
    leveldb::WriteOptions writeOptions_;
};

EMSCRIPTEN_BINDINGS(leveldbwasm) {

  class_<leveldb_iterator>("LevelDBIterator");
  class_<leveldb_batch>("LevelDBBatch");
  value_object<KeyValue>("KeyValue")
    .field("keySize", &KeyValue::keySize)
    .field("valueSize", &KeyValue::valueSize)
    .field("key", &KeyValue::key)
    .field("value", &KeyValue::value)
    ;
  
  class_<LevelDB>("LevelDB")
    .constructor<>()
    .function("open", &LevelDB::open)
    .function("get", &LevelDB::get)
    .function("put", &LevelDB::put)
    .function("remove", &LevelDB::remove)
    .function("iterator", &LevelDB::iterator, allow_raw_pointers())
    .function("closeIterator", &LevelDB::closeIterator, allow_raw_pointers())
    .function("seek", &LevelDB::seek, allow_raw_pointers())
    .function("seekToFirstOrLast", &LevelDB::seekToFirstOrLast, allow_raw_pointers())
    .function("setIteratorLimit", &LevelDB::setIteratorLimit, allow_raw_pointers())
    .function("next", &LevelDB::next, allow_raw_pointers())
    .function("prev", &LevelDB::prev, allow_raw_pointers())
    .function("replaceKey", &LevelDB::replaceKey)

    .function("openBatch", &LevelDB::openBatch, allow_raw_pointers())
    .function("closeBatch", &LevelDB::closeBatch, allow_raw_pointers())
    .function("clearBatch", &LevelDB::clearBatch, allow_raw_pointers())
    .function("writeBatch", &LevelDB::writeBatch, allow_raw_pointers())
    .function("batchPut", &LevelDB::batchPut, allow_raw_pointers())
    .function("bacthDelete", &LevelDB::batchDelete, allow_raw_pointers())

    .function("compaction", &LevelDB::compaction)
    .function("repairDB", &LevelDB::RepairDB)
    
    .function("getError", &LevelDB::getError)
    .function("setOption", &LevelDB::setOption, allow_raw_pointers())
    ; 
}
