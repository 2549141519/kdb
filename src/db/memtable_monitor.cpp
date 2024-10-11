#include "memtable_monitor.h"
#include <chrono>

namespace kdb {

MemTableMonitor::MemTableMonitor(std::vector<std::shared_ptr<Memtable>>& memtable_vec, 
                                 std::shared_ptr<Memtable>& readonly_memtable)
    : memtable_vec_(memtable_vec),
      readonly_memtable_(readonly_memtable),
      stop_monitoring_(false) {}

MemTableMonitor::~MemTableMonitor() {
    StopMonitoring();
}

void MemTableMonitor::StartMonitoring() {
    // 启动监视线程
    monitor_thread_ = std::thread(&MemTableMonitor::MonitorMemTable, this);
}

void MemTableMonitor::StopMonitoring() {
    stop_monitoring_ = true;
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();  // 等待线程结束
    }
}

void MemTableMonitor::MonitorMemTable() {
    while (!stop_monitoring_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));  // 每100ms检查一次

        std::lock_guard<std::mutex> lock(memtable_mutex_);  // 加锁保证线程安全
        for (auto it = memtable_vec_.begin(); it != memtable_vec_.end(); ) {
            if ((*it)->getMemSize() > MEMTABLE_THRESHOLD) { 
                // 超过阈值，将MemTable设置为只读并赋值给readonly_memtable_
                (*it)->setReadOnly();
                readonly_memtable_ = *it;
            } else {
                ++it;
            }
        }
    }
}
}