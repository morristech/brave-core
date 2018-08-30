/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 //#include "brave_sync_worker.h"
#include "brave/components/brave_sync/object_map.h"

#include <string>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h" // TODO, AB: remove and use Task with sequence checker
#include "chrome/common/chrome_paths.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace brave_sync {
namespace storage {

static const char DB_FILE_NAME[] = "brave_sync_db";

leveldb::DB* g_level_db;
static std::mutex* g_pLevel_db_init_mutex = new std::mutex();

ObjectMap::ObjectMap() {
  LOG(ERROR) << "TAGAB brave_sync::ObjectMap::ObjectMap CTOR";
}

ObjectMap::~ObjectMap() {
  LOG(ERROR) << "TAGAB brave_sync::ObjectMap::ObjectMap DTOR";
  Close();
}

void TraceAll() {
  LOG(ERROR) << "TAGAB brave_sync::ObjectMap::TraceAll:-----------------------";
  leveldb::Iterator* it = g_level_db->NewIterator(leveldb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    LOG(ERROR) << "<" << it->key().ToString() << ">: <" << it->value().ToString() << ">";
  }
  DCHECK(it->status().ok());  // Check for any errors found during the scan
  delete it;
  LOG(ERROR) << "TAGAB brave_sync::ObjectMap::TraceAll:^----------------------";
}

void CreateOpenDatabase() {
  if (!g_pLevel_db_init_mutex) {
    return;
  }
  std::lock_guard<std::mutex> guard(*g_pLevel_db_init_mutex);

  if (nullptr == g_level_db) {
    base::FilePath app_data_path;
    bool success = base::PathService::Get(chrome::DIR_USER_DATA, &app_data_path);
    CHECK(success);
    LOG(ERROR) << "TAGAB CreateOpenDatabase success=" << success;
    LOG(ERROR) << "TAGAB CreateOpenDatabase app_data_path=" << app_data_path;

    base::FilePath dbFilePath = app_data_path.Append(DB_FILE_NAME);
    LOG(ERROR) << "TAGAB CreateOpenDatabase dbFilePath=" << dbFilePath;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, dbFilePath.value().c_str(), &g_level_db);
    LOG(ERROR) << "TAGAB CreateOpenDatabase status=" << status.ToString();
    if (!status.ok() || !g_level_db) {
        if (g_level_db) {
            delete g_level_db;
            g_level_db = nullptr;
        }

        LOG(ERROR) << "sync level db open error " << DB_FILE_NAME;
        return;
    }
    LOG(ERROR) << "TAGAB DB opened";
    ///TraceAll();
  }
}

std::string ObjectMap::GetLocalIdByObjectId(const std::string &objectId) {
  base::ScopedAllowBlockingForTesting sab;// TODO, AB: remove
  CreateOpenDatabase();
  if (nullptr == g_level_db) {
      return "";
  }

  std::string value;
  leveldb::Status db_status = g_level_db->Get(leveldb::ReadOptions(), objectId, &value);
  if (!db_status.ok()) {
    LOG(ERROR) << "sync level db get error " << db_status.ToString();
  }

  return value;
}

std::string ObjectMap::GetObjectIdByLocalId(const std::string &localId) {
  base::ScopedAllowBlockingForTesting sab;// TODO, AB: remove
  CreateOpenDatabase();
  if (nullptr == g_level_db) {
      return "";
  }

  std::string value;
  leveldb::Status db_status = g_level_db->Get(leveldb::ReadOptions(), localId, &value);

  if (!db_status.ok()) {
    LOG(ERROR) << "sync level db get error " << db_status.ToString();
  }

  return value;
}

void ObjectMap::SaveObjectId(
  const std::string &localId,
  const std::string &objectIdJSON, //may be an order or empty
  const std::string &objectId) {
  LOG(ERROR) << "TAGAB brave_sync::ObjectMap::SaveObjectId - enter";
  base::ScopedAllowBlockingForTesting sab;// TODO, AB: remove
  CreateOpenDatabase();
  if (nullptr == g_level_db) {
    LOG(ERROR) << "TAGAB brave_sync::ObjectMap::SaveObjectId nullptr == g_level_db ???";
    return;
  }

  leveldb::Status db_status = g_level_db->Put(leveldb::WriteOptions(), localId, objectIdJSON);
  if (!db_status.ok()) {
    LOG(ERROR) << "sync level db put error " << db_status.ToString();
  }

  if (0 != objectId.size()) {
      db_status = g_level_db->Put(leveldb::WriteOptions(), objectId, localId);
      if (!db_status.ok()) {
        LOG(ERROR) << "sync level db put error " << db_status.ToString();
      }
  }
  LOG(ERROR) << "TAGAB brave_sync::ObjectMap::SaveObjectId - DONE";
}

void ObjectMap::DeleteByLocalId(const std::string &localId) {
  base::ScopedAllowBlockingForTesting sab;// TODO, AB: remove
  CreateOpenDatabase();
  if (nullptr == g_level_db) {
      return;
  }

  std::string value;
  leveldb::Status db_status = g_level_db->Get(leveldb::ReadOptions(), localId, &value);
  if (!db_status.ok()) {
    LOG(ERROR) << "sync level db get error " << db_status.ToString();
  }

  db_status = g_level_db->Delete(leveldb::WriteOptions(), localId);
  if (!db_status.ok()) {
    LOG(ERROR) << "sync level db delete error " << db_status.ToString();
  }
  db_status = g_level_db->Delete(leveldb::WriteOptions(), value);
  if (!db_status.ok()) {
    LOG(ERROR) << "sync level db delete error " << db_status.ToString();
  }
}

void ObjectMap::Close() {
  if (g_level_db) {
      delete g_level_db;
      g_level_db = nullptr;
  }
  if (g_pLevel_db_init_mutex) {
      delete g_pLevel_db_init_mutex;
      g_pLevel_db_init_mutex = nullptr;
  }
}

void ObjectMap::CloseDBHandle() {
  if (g_level_db) {
      delete g_level_db;
      g_level_db = nullptr;
  }
}

void ObjectMap::DestroyDB() {
  base::ScopedAllowBlockingForTesting sab;// TODO, AB: remove
  if (!g_pLevel_db_init_mutex) {
    return;
  }
  std::lock_guard<std::mutex> guard(*g_pLevel_db_init_mutex);
  LOG(ERROR) << "TAGAB brave_sync::ObjectMap::ResetObjects";
  CloseDBHandle();
  base::FilePath app_data_path;
  bool success = base::PathService::Get(chrome::DIR_USER_DATA, &app_data_path);
  CHECK(success);
  base::FilePath dbFilePath = app_data_path.Append(DB_FILE_NAME);
  LOG(ERROR) << "TAGAB ResetObjects dbFilePath=" << dbFilePath;
  leveldb::Status db_status = leveldb::DestroyDB(dbFilePath.value(), leveldb::Options());
  if (!db_status.ok()) {
    LOG(ERROR) << "sync level db destroy error " << db_status.ToString();
  }
}

void ObjectMap::ResetSync(const std::string& key) {
  base::ScopedAllowBlockingForTesting sab;// TODO, AB: remove
  CreateOpenDatabase();
  if (nullptr == g_level_db) {
      return;
  }
  g_level_db->Delete(leveldb::WriteOptions(), key);
}

} // namespace storage
} // namespace brave_sync