#ifndef SRC_DB_DBCONFIG_H_
#define SRC_DB_DBCONFIG_H_

#include <string>


namespace kdb {

struct DBConfig {
  // 预写日志的容量大小
  uint32_t wal_log_size = 64 * 1024 * 1024;

  // 预写日志是否 sync
  // sync 能保证预写日志落盘
  // 不 sync 的话, 性能会有一定提升.
  bool wal_log_is_sync = false;

  // kv 引擎文件所放置的目录
  std::string db_path;

  // NightSheep DB 拥有多个内存表
  // 此配置表示 NightSheepDB 的内存表的数量
  uint32_t memtable_N = 8;

  // 单个内存表的最大容量
  // 注意, 一个内存表被写满之后, 并不能立即被刷入
  // 他将会等待 Compactor 被刷入到  SStable 中去
  // 所以峰值内存占用的计算公式是:
  // (2 * memtable_N * memtable_trigger_size) + (2 * memtable_N * 8MB)
  // 注意峰值内存占用, 不然直接 oom
  uint32_t memtable_trigger_size = 128 * 1024 * 1024;

  // compactor 的线程个数
  // 当内存表过多时, 映射的 Compactor 如果线程数量太少, Compaction 操作明显变慢
  // BEST: 使用 memtable_trigger_size / 4 个线程 来进行 Compaction
  // 当然, 这里我们还应该考虑单个 memtable 的大小.
  uint32_t compactor_thread_size = 1;

  // 单个 worker 都带有一个 klrucache 缓存
  // 缓存容量的大小
  uint32_t lru_cache_size = 15 * 1024 * 1024;
  //
  // 每个 memtable 都拥有多层 level,
  // 限制 level 的最大层数.
  // 当超出了这些 level 之后,
  // 则会让 compactor 触发 major_compaction
  // 默认当超过 7 层时, 开始触发 major_compaction
  uint32_t max_level_size = 8;

  // 如果打开的目录已经存在, 是否尝试恢复
  // 否: 清空所有数据, 新建立一个 DB
  // 是: 尝试恢复数据, 读取原有的预写日志, 并尝试恢复
  // 尝试恢复之后, 若数据恢复失败, 则会停止启动服务器
  bool isRecover;

  // 当前版本信息
  uint32_t version_;
};

}  // namespace kdb



#endif