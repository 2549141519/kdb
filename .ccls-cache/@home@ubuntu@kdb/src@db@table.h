#ifndef SRC_DB_SHARED_MEMTABLE_H_
#define SRC_DB_SHARED_MEMTABLE_H_

#include <condition_variable>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "request.h"
#include "memtable.h"
#include "table_view_manager.h"
#include "../utils/klrucache.h"
#include "dbconfig.h"

namespace kdb {
kLRUCache klrucache_;
class Compactor;

using TableTask =   std::variant<std::shared_ptr<SetContext>,
                    std::shared_ptr<GetContext>,
                    std::shared_ptr<DeleteContext>,
                    std::shared_ptr<SnapShotContext>>;

class TaskWorker {
public:
  void addTask(TableTask task);

   // TaskWorker 共享 std::shared_ptr<Memtable> 的所有权
  std::shared_ptr<Memtable> memtable_;
  void Run();
private:
 
  uint64_t remove_N_;
  // 单个 TaskWorker 保存上一次的 remove 的序列号.
  uint64_t preRemove_N_;
  // TaskWorker 共享 std::shared_ptr<Memtable> 的所有权

  Memtable *only_read_memtable;
  
  // 保护 handle_vec_;
  std::mutex task_vec_mtx_;
  
  // 后台增加任务池, 真正执行的任务池
  std::vector<TableTask> task_vec_;

  // 增加任务的任务池
  std::vector<TableTask> bg_task_vec_;

  // 配合条件变量使用, 当有任务加入到TaskWorker 中,
  // 应该使用条件变量去唤醒
  std::mutex mtx_;

  // 条件变量用于唤醒 工作者 
  std::condition_variable cond_;

  // 执行任务的工作者
  std::thread worker_;

  // 是否正在执行
  bool isRunning_;

  // 单个 memtable 的最大容量
  uint32_t maxMemTableSize_;

  // 等待 Compactor 的刷入
  std::shared_ptr<Compactor> compactor_;

  std::mutex memtable_view_mtx_;

  std::shared_ptr<TableViewManager> memview_manager_;

  std::mutex memtable_view_mtx_;
};

class Table {
public:
 Table() = default;
  ~Table();

  void Run();

  // memtable 的数量, 每个 memtable 的峰值容量
  void Init(const DBConfig& db_config);
  void Set(const std::shared_ptr<SetContext>& set_context);
  void Get(const std::shared_ptr<GetContext>& get_context);
  void Delete(const std::shared_ptr<DeleteContext>& del_context);
  void SnapShot(const std::shared_ptr<SnapShotContext>& snapshot_context);
  //!!! 必须在 Init 之前调用, 2.
  void SetCompactorRef(const std::shared_ptr<Compactor>& compactor);

  // 让单个 mem_worker 拥有 memtable_view_manager 的所有权.
  void SetMemTableViewRef(
      const std::shared_ptr<TableViewManager>& memtable_view_manager);


  auto getMemTaskWorkerLevel(const uint32_t n) {
    return taskworkers_[n]->memtable_->getCompactionN();
  }

  auto getMemTaskWorkerNumber(const uint32_t n) {
    return taskworkers_[n]->memtable_->getMemNumber();
  }

  /*
   * @n : 对应的 memtable 号码
   *
   */
  void PushRemoveReadonlyMemtableContext(const uint32_t n);

 private:
  // 每个 memTable 的最大容量
  uint32_t singleMemTableSize_;
  // memtableVec 的容量
  uint32_t memtable_N_;

  // 当请求来之后,
  std::vector<std::shared_ptr<TaskWorker>> taskworkers_;
  // 哈希算法
  // 将请求散列到不同的 memtable 中去
  std::hash<std::string_view> stdHash_;
  //
  std::shared_ptr<Compactor> comp_actor_;
};





}

#endif