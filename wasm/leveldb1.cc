#include <string>
#include <array>
#include <stdio.h>
#include "leveldb/db.h"
#include "leveldb/write_batch.h"

#include <emscripten.h>
#include <emscripten/bind.h>


using namespace std;
using namespace emscripten;

struct KeyValue {
  /** Key length */
  int keyLen;
  /** Key + Value 值*/
  string data;
};


class LevelDB {
  public:
    LevelDB() {
      options_.create_if_missing = true;
      options_.compression = leveldb::kNoCompression;
      writeOptions_.sync = false;
    }

    bool compaction() {
      db_->MayBeCompaction();
      return true;
    }    

    bool open(string path) {
      leveldb::DB* db;
      leveldb::Status s = leveldb::DB::Open(options_, path, &db);
      db_ = db;
      return s.ok();
    }

    string get(string key) {
      string value;
      leveldb::Status s = db_->Get(readOptions_, key, &value);
      if (s.ok()) {
        return value;
      } else {
        return "";
      }
    }

    bool put(string key, string value) {
      leveldb::Status s = db_->Put(writeOptions_, key, value);
      return s.ok();
    }

    /**
     * 替换KEY - 用来替换指定Key的Key值
     */
    bool replaceKey( string key1, string key2) {
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

    void openBatch() {
      leveldb::WriteBatch* batch;
      batch_ = &batch;
    }

    void batchPut(string key, string value) {
      (*batch_)->Put(key, value);
    }

    void batchDelete(string key) {
      (*batch_)->Delete(key);
    }

    void clearBatch() {
      (*batch_)->Clear();
    }

    bool writeBatch() {
      leveldb::Status s = db_->Write(writeOptions_, *batch_);
      return s.ok();
    }

    void closeBatch() {
      delete *batch_;
    }

    bool remove(string key) {
      leveldb::Status s = db_->Delete(writeOptions_, key);
      return s.ok();
    }

    void createIterator() {
      it_ = db_->NewIterator(readOptions_);
      newIterator_ = true; 
    }

    KeyValue next() {
      if (newIterator_) {
        it_->SeekToFirst();
        newIterator_ = false;
      }

      KeyValue kv = {0, ""};
      if (it_->Valid()) {
        string key = it_->key().ToString();
        kv.keyLen = key.length();
        kv.data = key +":::"+ it_->value().ToString();
        it_->Next();
      }
      
      return kv;

    }

    KeyValue prev() {
      if(newIterator_) {
        it_->SeekToLast();
        newIterator_ = false;
      }

      KeyValue kv = {0, ""};
      if (it_->Valid()) {
        string key = it_->key().ToString();
        kv.keyLen = key.length();
        kv.data = key +":::"+ it_->value().ToString();
        it_->Prev();
      }

      return kv;
    }

    bool seek(string start, string end, bool isStart) {
      if (isStart) {
        it_->Seek(start);
        limit_ = end;
      } else {
        it_->Seek(end);
        limit_ = start;
      }

      newIterator_ = false;

      return true;
    }

    KeyValue seekNext() {
      KeyValue kv = {0, ""};
      if (it_->Valid() && it_->key().ToString() < limit_) {
        string key = it_->key().ToString();
        kv.keyLen = key.length();
        kv.data = key + ":::" + it_->value().ToString();
        it_->Next();
      }

      return kv;
    }

    KeyValue seekPrev() {
      KeyValue kv = {0, ""};
      if (it_->Valid() && it_->key().ToString() > limit_) {
        string key = it_->key().ToString();
        kv.keyLen = key.length();
        kv.data = key + ":::" + it_->value().ToString();
        it_->Prev();
      }

      return kv;
    }

    void closeIterator() {
      delete it_;
    }

    bool RepairDB(string dbname) {
      leveldb::Status s = leveldb::RepairDB(dbname, options_);

      if (s.ok()) {
        return true;
      }
      return false;
    }

    ~LevelDB() {
      delete db_;
    }

  private:

    leveldb::DB* db_;

    leveldb::Iterator* it_; 
    bool newIterator_ = false;
    string limit_ = "";

    leveldb::WriteBatch** batch_;

    leveldb::Options options_;
    leveldb::ReadOptions readOptions_;
    leveldb::WriteOptions writeOptions_;
};

EMSCRIPTEN_BINDINGS(leveldb1) {

  value_object<KeyValue>("KeyValue")
    .field("keyBytesLength", &KeyValue::keyLen)
    .field("data", &KeyValue::data)
    ;
  
  class_<LevelDB>("LevelDB")
    .constructor<>()
    .function("open", &LevelDB::open)
    .function("get", &LevelDB::get)
    .function("put", &LevelDB::put)
    .function("remove", &LevelDB::remove)
    .function("createIterator", &LevelDB::createIterator)
    .function("closeIterator", &LevelDB::closeIterator)
    .function("seek", &LevelDB::seek)
    .function("seekNext", &LevelDB::seekNext)
    .function("seekPrev", &LevelDB::seekPrev)
    .function("next", &LevelDB::next)
    .function("prev", &LevelDB::prev)
    .function("replaceKey", &LevelDB::replaceKey)

    .function("openBatch", &LevelDB::openBatch)
    .function("closeBatch", &LevelDB::closeBatch)
    .function("clearBatch", &LevelDB::clearBatch)
    .function("writeBatch", &LevelDB::writeBatch)
    .function("batchPut", &LevelDB::batchPut)
    .function("bacthDelete", &LevelDB::batchDelete)

    .function("compaction", &LevelDB::compaction)
    .function("repairDB", &LevelDB::RepairDB)
    
    ; 
}