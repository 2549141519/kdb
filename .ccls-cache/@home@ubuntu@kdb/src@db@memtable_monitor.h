#ifndef MEMTABLE_MONITOR_H
#define MEMTABLE_MONITOR_H

#include <thread>
#include <vector>
#include <mutex>
#include <memory>
#include "memtable.h"

namespace kdb {

class MemTableMonitor {
public:
    MemTableMonitor(std::vector<std::shared_ptr<Memtable>>& memtable_vec, 
                    std::vector<std::shared_ptr<Memtable>>& readonly_memtable_vec);
    ~MemTableMonitor();

    // 启动监视线程
    void StartMonitoring();
    // 停止监视线程
    void StopMonitoring();

private:
    void MonitorMemTable();  // 监视MemTable的方法

    std::vector<std::shared_ptr<Memtable>>& memtable_vec_;            // 引用外部的MemTable向量
    std::vector<std::shared_ptr<Memtable>>& readonly_memtable_vec_;   // 引用外部的只读MemTable向量

    std::thread monitor_thread_;       // 监视线程
    std::mutex memtable_mutex_;        // 保护memtable_vec_的互斥锁
    bool stop_monitoring_;             // 用于停止线程的标志

    static const size_t MEMTABLE_THRESHOLD = 128 * 1024 * 1024;  // 128MB的阈值
};
}
#endif  // MEMTABLE_MONITOR_H
