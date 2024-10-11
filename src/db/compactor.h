#ifndef COMPACTOR_H
#define COMPACTOR_H


#include <thread>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <ctime>
#include <string_view>
#include <condition_variable>

#include "memtable.h"
#include "sstable.h"
#include "table_view_manager.h"
#include "../include/bloomfilter.h"
#include "../utils/coding.h"
#include "memtable_monitor.h"
#include "absl/container/btree_set.h"
#include "key_format.h"

namespace kdb {

class TableViewManager;

class Compactor {
public:
    Compactor(TableViewManager& manager);
    ~Compactor();

    void StartCompaction();
    void StopCompaction();
    
    void AssignReadOnlyMemtable(std::shared_ptr<Memtable>& readonly_memtable);

    void CheckForCompaction(); 

    void brushdisk_task();
    
    void minorcompaction_task_L0_to_L1();

    void minorcompaction_task_L1_to_L2();

    uint64_t GenerateSSTableNumber();


private:
    std::mutex queue_mutex_;
    std::condition_variable task_cv_;

    // Data members
    std::shared_ptr<Memtable> readonly_memtable_;
    TableViewManager& table_view_manager_;
    std::thread check_thread_;
    std::thread compaction_thread_;
    bool stop_compaction_;
    uint64_t next_sstable_number_;

    // Constants
    static const size_t MEMTABLE_THRESHOLD = 128 * 1024 * 1024; // 128MB
};

}// namespace kdb
#endif // COMPACTOR_H
