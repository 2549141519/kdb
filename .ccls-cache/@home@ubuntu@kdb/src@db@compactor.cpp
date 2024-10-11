#include "compactor.h"
#include "absl/container/btree_set.h"
namespace kdb {

Compactor::Compactor(TableViewManager& manager)
    : table_view_manager_(manager), stop_compaction_(false), next_sstable_number_(1) {}

Compactor::~Compactor() {
    StopCompaction();
}

uint64_t Compactor::GenerateSSTableNumber() {
    return next_sstable_number_++;
}

void Compactor::StartCompaction() {
    check_thread_ = std::thread(&Compactor::CheckForCompaction, this);
}

void Compactor::StopCompaction() {
    stop_compaction_ = true;
    task_cv_.notify_all();
    if (check_thread_.joinable()) {
        check_thread_.join();
    }
}

void Compactor::CheckForCompaction() {
    while (!stop_compaction_) {
        // 检查刷盘任务
        if (!readonly_memtable_) {
            std::thread compaction_thread(&Compactor::brushdisk_task, this, "brushdisk_task");
            if (compaction_thread.joinable()) {
                compaction_thread.join();  // 等待刷盘任务完成
            }
        }
        // 检查L0到L1合并任务
        else if (table_view_manager_.Needs_L0_to_L1_Compaction()) {
            std::thread compaction_thread(&Compactor::minorcompaction_task_L0_to_L1, this, "minorcompaction_task_L0_to_L1");
            if (compaction_thread.joinable()) {
                compaction_thread.join();  // 等待 L0 到 L1 合并任务完成
            }
        }
        // 检查L1到L2合并任务
        else if (table_view_manager_.Needs_L1_to_L2_Compaction()) {
            std::thread compaction_thread(&Compactor::minorcompaction_task_L1_to_L2, this, "minorcompaction_task_L1_to_L2");
            if (compaction_thread.joinable()) {
                compaction_thread.join();  // 等待 L1 到 L2 合并任务完成
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));  // 定期检查是否有新的合并任务
    }
}

void Compactor::brushdisk_task() {
    while (!readonly_memtable_) {
        //readonly_memtable_vec_.erase(readonly_memtable_vec_.begin());
        // 使用 MemBTreeView 存储内部键，保持键的有序性，并传入比较器
        MemBTreeView btree_set;
//===============================================================  
//布隆过滤器
//===============================================================           
        auto bloom_filter = readonly_memtable_.getBloomFilter();

        std::string min_user_key;  // 记录最小用户键
        std::string max_user_key;  // 记录最大用户键
        bool first_key = true;     // 标志位，用于设置第一个键为最小键
        uint32_t all_key_size = 0; // 记录总的键值对数量

        // 遍历 MemTable，通过迭代器获取每个内部键
        SkipList<std::string,Comparator>::Iterator iter(readonly_memtable_->getSkipTable().get());
        iter.SeekToFirst();  // 从跳表的第一个键开始遍历

        // 将内部键插入到 BTree 中，并解析用户键
        while (iter.Valid()) {
            std::string_view internal_key = iter.key();  // 获取当前的内部键

            uint32_t mem_key_size = 0;
            auto end_ptr = GetVarint32Ptr(internal_key.data(), internal_key.data() + 5, &mem_key_size);
            assert(end_ptr != nullptr);
            std::string_view user_key(end_ptr, mem_key_size);

            // 更新最小和最大用户键
            if (first_key) {
                min_user_key = user_key;
                max_user_key = user_key;
                first_key = false;
            } else {
                if (user_key < min_user_key) {
                    min_user_key = user_key;
                }
                if (user_key > max_user_key) {
                    max_user_key = user_key;
                }
            }

            // 将内部键插入到 BTree 中
            btree_set.insert(internal_key); 
            all_key_size++;
            iter.Next();  // 移动到下一个内部键
        }

        // 创建新的 L0 层的 SSTable 并持久化内部键
        std::shared_ptr<SSTable> sstable = std::make_shared<SSTable>();
        std::string sstable_file_path = "/home/ubuntu/kdb/file/level0";  // 替换为实际路径
        uint32_t sstable_number = static_cast<uint32_t>(GenerateSSTableNumber());
        uint32_t sstable_level = 0;  // L0 层

        std::string sstable_filename = "sstable_" + std::to_string(sstable_number) + ".sst";

        // 初始化 SSTable
        if (!sstable->Init(sstable_file_path, sstable_filename, sstable_level, sstable_number)) {
            std::cerr << "Failed to initialize SSTable" << std::endl;
            return;
        }
        
        sstable->setMinKey(min_user_key);
        sstable->setMaxKey(max_user_key);

        // 使用 mmap 将 BTree 中的键值对写入到 SSTable
        char* mmap_ptr = nullptr;
        size_t offset = 0;
        if (sstable->InitMmap()) {
            mmap_ptr = sstable->getMmapPtr();
            offset = 0;
            

            // 1. 写入版本号、创建时间、层级、文件编号和键的总大小
            EncodeVarint32(mmap_ptr + offset, 1);
            offset += sizeof(uint32_t);

            // 获取当前时间
            auto current_time = std::chrono::system_clock::now();
            time_t time_t_format = std::chrono::system_clock::to_time_t(current_time);
            // 将 time_t 转换为 uint64_t
            uint64_t time_as_uint64 = static_cast<uint64_t>(time_t_format);
            EncodeFixed64(mmap_ptr + offset, time_as_uint64);
            offset += sizeof(uint64_t);

            EncodeVarint32(mmap_ptr + offset, sstable_level);
            offset += sizeof(uint32_t);

            EncodeVarint32(mmap_ptr + offset, sstable_number);
            offset += sizeof(uint32_t);

            EncodeVarint32(mmap_ptr + offset, all_key_size);
            offset += sizeof(uint32_t);

            // 2. 写入布隆过滤器种子和大小
            EncodeFixed64(mmap_ptr + offset, bloom_filter.getFilterSeed());
            offset += sizeof(uint64_t);

            uint32_t bloomfilter_size = bloom_filter.getBitSetSize();
            EncodeVarint32(mmap_ptr + offset, bloomfilter_size);
            offset += sizeof(uint32_t);

            // 3. 写入布隆过滤器数据
            std::memcpy(mmap_ptr + offset, bloom_filter.getFilterData()->getData(), bloomfilter_size);
            offset += bloomfilter_size;

            // 4. 写入最小键和最大键
            uint32_t min_key_length = min_user_key.size();
            uint32_t max_key_length = max_user_key.size();

            EncodeVarint32(mmap_ptr + offset, min_key_length);  // 写入最小键的长度
            offset += VarintLength(min_key_length);
            std::memcpy(mmap_ptr + offset, min_user_key.data(), min_key_length);  // 写入最小键
            offset += min_key_length;

            EncodeVarint32(mmap_ptr + offset, max_key_length);  // 写入最大键的长度
            offset += VarintLength(max_key_length);
            std::memcpy(mmap_ptr + offset, max_user_key.data(), max_key_length);  // 写入最大键
            offset += max_key_length;

            // 5. 遍历 BTree，写入键值对数据
            for (const auto& key_to_write : btree_set) {
                // 1. 获取内部键信息，解析键和值
                std::string internal_key = key_to_write;

                uint32_t key_size = 0;
                auto key_ptr = GetVarint32Ptr(internal_key.data(), internal_key.data() + 5, &key_size);  // 获取键的大小
                assert(key_ptr != nullptr);
                std::string user_key(key_ptr, key_size);  // 获取用户键

                key_ptr += key_size;  // 跳过键部分
                uint64_t sequence_number;
                key_ptr = GetVarint64Ptr(key_ptr, key_ptr + 8, &sequence_number);  // 获取序列号

                uint8_t key_type = DecodeFixed8(key_ptr);  // 获取键的类型（标志位）

                // 2. 写入键的长度
                EncodeVarint32(mmap_ptr + offset, key_size);
                offset += VarintLength(key_size);

                // 3. 写入键内容
                std::memcpy(mmap_ptr + offset, user_key.data(), key_size);
                offset += key_size;

                // 4. 无论是否删除，写入原先的值数据
                uint32_t value_size = 0;
                auto value_ptr = GetVarint32Ptr(key_ptr + 1, key_ptr + 6, &value_size);  // 获取值的大小

                // 如果值存在，则写入
                if (value_ptr != nullptr) {
                    // 写入值的长度
                    EncodeVarint32(mmap_ptr + offset, value_size);
                    offset += VarintLength(value_size);

                    // 写入值内容
                    std::memcpy(mmap_ptr + offset, value_ptr, value_size);
                    offset += value_size;
                }
            }
        }

        // 更新 SSTableView，将 SSTable 的数据和元信息传入 TableViewManager
        table_view_manager_.PushTableView(sstable_level, mmap_ptr, static_cast<uint32_t>(offset), 
                                          sstable, min_user_key, max_user_key);
    }
}

void Compactor::minorcompaction_task_L0_to_L1() {
    while (table_view_manager_.Needs_L0_to_L1_Compaction()) {
    
    //获取L0层最早的sstable
    std::shared_ptr<SSTable> l0_sstable = table_view_manager_.GetL0SSTable();
    
    // 读取 L0 文件中的元信息和键值对数据
    MemBTreeView btree_set;  // 使用 BTree 存储内部键
    
    std::string min_user_key;  // 记录最小用户键
    std::string max_user_key;  // 记录最大用户键
    uint32_t all_key_size = 0; // 记录总的键值对数量
    
    // 从 L0 文件中读取数据
    char* l0_mmap_ptr = l0_sstable->getMmapPtr();
    uint32_t offset = 0;
    const char* kv_data_start = l0_mmap_ptr;
    
    // 1. 读取 L0 文件中的元信息
    uint32_t version, sstable_level, sstable_number, bloomfilter_size, all_key_count;
    uint64_t creation_time, bloomfilter_seed;
    
    // 读取版本号
    version = DecodeFixed32(kv_data_start);
    kv_data_start += sizeof(uint32_t);

    // 读取创建时间
    creation_time = DecodeFixed64(kv_data_start);
    kv_data_start += sizeof(uint64_t);

    // 读取层级
    sstable_level = DecodeFixed32(kv_data_start);
    kv_data_start += sizeof(uint32_t);

    // 读取文件编号
    sstable_number = DecodeFixed32(kv_data_start);
    kv_data_start += sizeof(uint32_t);

    // 读取总键数
    all_key_count = DecodeFixed32(kv_data_start);
    kv_data_start += sizeof(uint32_t);

    // 读取布隆过滤器种子
    bloomfilter_seed = DecodeFixed64(kv_data_start);
    kv_data_start += sizeof(uint64_t);

    // 读取布隆过滤器大小
    bloomfilter_size = DecodeFixed32(kv_data_start);
    kv_data_start += sizeof(uint32_t);


    const char* bloom_filter_data = kv_data_start;
    kv_data_start += bloomfilter_size;  // 跳过布隆过滤器数据

    // 2. 读取最小键和最大键
    uint32_t min_key_length, max_key_length;
    kv_data_start = GetVarint32Ptr(kv_data_start, kv_data_start + 5, &min_key_length);
    min_user_key = std::string(kv_data_start, min_key_length);
    kv_data_start += min_key_length;

    kv_data_start = GetVarint32Ptr(kv_data_start, kv_data_start + 5, &max_key_length);
    max_user_key = std::string(kv_data_start, max_key_length);
    kv_data_start += max_key_length;

    // 3. 遍历键值对数据，插入到 BTree 中
    while (kv_data_start < (l0_mmap_ptr + l0_sstable->getFileSize())) {
        uint32_t key_size = 0, value_size = 0;
        
        // 读取键的长度
        auto key_ptr = GetVarint32Ptr(kv_data_start, kv_data_start + 5, &key_size);
        assert(key_ptr != nullptr);
        std::string internal_key(key_ptr, key_size);
        kv_data_start += key_size;  // 跳过键

        // 读取值的长度
        auto value_ptr = GetVarint32Ptr(kv_data_start, kv_data_start + 5, &value_size);
        kv_data_start += value_size;  // 跳过值

        // 插入 BTree
        btree_set.insert(internal_key);
        all_key_size++;
    }

    //判断L1有没有文件
    if (table_view_manager_.isLevelEmpty(1)) {

        // 创建新的 L1 文件并写入数据
        std::shared_ptr<SSTable> l1_sstable = std::make_shared<SSTable>();
        std::string sstable_file_path = "/home/ubuntu/kdb/file/level1";  // L1 层文件路径
        sstable_number = static_cast<uint32_t>(GenerateSSTableNumber());
        sstable_level = 1;  // L1 层

        std::string sstable_filename = "sstable_" + std::to_string(sstable_number) + ".sst";

        // 初始化 L1 SSTable
        if (!l1_sstable->Init(sstable_file_path, sstable_filename, sstable_level, sstable_number)) {
            std::cerr << "Failed to initialize L1 SSTable" << std::endl;
            return;
        }

        // 使用 mmap 将 BTree 中的键值对按元信息格式写入 L1 文件
        char* l1_mmap_ptr = nullptr;
        size_t offset_l1 = 0;
        if (l1_sstable->InitMmap()) {
            l1_mmap_ptr = l1_sstable->getMmapPtr();
            offset_l1 = 0;

            // 1. 写入版本号、创建时间、层级、文件编号和键的总数量
            EncodeVarint32(l1_mmap_ptr + offset_l1, version);
            offset_l1 += sizeof(uint32_t);
            
            auto current_time = std::chrono::system_clock::now();
            time_t time_t_format = std::chrono::system_clock::to_time_t(current_time);
            uint64_t time_as_uint64 = static_cast<uint64_t>(time_t_format);
            EncodeFixed64(l1_mmap_ptr + offset, time_as_uint64);
            offset += sizeof(uint64_t);
            
            EncodeVarint32(l1_mmap_ptr + offset_l1, sstable_level);
            offset_l1 += sizeof(uint32_t);
            EncodeVarint32(l1_mmap_ptr + offset_l1, sstable_number);
            offset_l1 += sizeof(uint32_t);
            EncodeVarint32(l1_mmap_ptr + offset_l1, all_key_size);
            offset_l1 += sizeof(uint32_t);

            // 2. 写入布隆过滤器种子和大小
            EncodeFixed64(l1_mmap_ptr + offset_l1, bloomfilter_seed);
            offset_l1 += sizeof(uint64_t);
            EncodeVarint32(l1_mmap_ptr + offset_l1, bloomfilter_size);
            offset_l1 += sizeof(uint32_t);

            // 3. 写入布隆过滤器数据（直接使用 L0 的数据）
            std::memcpy(l1_mmap_ptr + offset_l1, bloom_filter_data, bloomfilter_size);
            offset_l1 += bloomfilter_size;

            // 4. 写入最小键和最大键
            EncodeVarint32(l1_mmap_ptr + offset_l1, min_user_key.size());
            offset_l1 += VarintLength(min_user_key.size());
            std::memcpy(l1_mmap_ptr + offset_l1, min_user_key.data(), min_user_key.size());
            offset_l1 += min_user_key.size();

            EncodeVarint32(l1_mmap_ptr + offset_l1, max_user_key.size());
            offset_l1 += VarintLength(max_user_key.size());
            std::memcpy(l1_mmap_ptr + offset_l1, max_user_key.data(), max_user_key.size());
            offset_l1 += max_user_key.size();

            // 5. 遍历 BTree，写入键值对数据
            for (const auto& key_to_write : btree_set) {
                // 获取内部键信息
                std::string internal_key = key_to_write;
                uint32_t key_size = 0;
                auto key_ptr = GetVarint32Ptr(internal_key.data(), internal_key.data() + 5, &key_size);
                assert(key_ptr != nullptr);
                std::string user_key(key_ptr, key_size);

                // 写入键的长度和内容
                EncodeVarint32(l1_mmap_ptr + offset_l1, key_size);
                offset_l1 += VarintLength(key_size);
                std::memcpy(l1_mmap_ptr + offset_l1, user_key.data(), key_size);
                offset_l1 += key_size;

                // 写入原先的值数据
                uint32_t value_size = 0;
                auto value_ptr = GetVarint32Ptr(key_ptr + 1, key_ptr + 6, &value_size);
                if (value_ptr != nullptr) {
                    EncodeVarint32(l1_mmap_ptr + offset_l1, value_size);
                    offset_l1 += VarintLength(value_size);
                    std::memcpy(l1_mmap_ptr + offset_l1, value_ptr, value_size);
                    offset_l1 += value_size;
                }
            }
        }
        // 更新 SSTableView，将 L1 文件数据和元信息传入 TableViewManager
        table_view_manager_.PushTableView(sstable_level, l1_mmap_ptr, static_cast<uint32_t>(offset_l1), 
                                            l1_sstable, min_user_key, max_user_key);

        // 删除 L0 文件
        table_view_manager_.RemoveTableFromLevel(0, l0_sstable->getNumber());
        continue;
        }
    else {
        // 开始合并 L0 和 L1 文件
        std::vector<std::shared_ptr<SSTable>> l1_sstables = table_view_manager_.GetL1SSTables();
        MemBTreeView l1_btree_set;  // 用来存储合并的 L1 层数据
        uint32_t merged_key_size = all_key_size;     // 记录合并后总的键值对数量

        BloomFilter<kBloomFilterDefaultSize> new_bloom_filter;

        // 遍历 L1 层所有 SSTable
        for (const auto& l1_sstable : l1_sstables) {
            if (l1_sstable->getMaxKey() >= min_user_key && l1_sstable->getMinKey() <= max_user_key) {
                // 键范围有重叠的 L1 文件，开始合并
                char* l1_mmap_ptr = l1_sstable->getMmapPtr();
                
                // 读取 L1 文件中的元信息，计算 meta
                uint32_t version, sstable_level, sstable_number, bloomfilter_size, all_key_count;
                uint64_t creation_time, bloomfilter_seed;
                
                const char* l1_kv_data_start = l1_mmap_ptr;

                // 读取元信息部分
                version = DecodeFixed32(l1_kv_data_start);  // 版本号
                l1_kv_data_start += sizeof(uint32_t);  // 移动指针到下一个数据位置

                creation_time = DecodeFixed64(l1_kv_data_start);  // 创建时间
                l1_kv_data_start += sizeof(uint64_t);

                sstable_level = DecodeFixed32(l1_kv_data_start);  // 层级
                l1_kv_data_start += sizeof(uint32_t);

                sstable_number = DecodeFixed32(l1_kv_data_start);  // 文件编号
                l1_kv_data_start += sizeof(uint32_t);

                all_key_count = DecodeFixed32(l1_kv_data_start);  // 总键数量
                l1_kv_data_start += sizeof(uint32_t);

                bloomfilter_seed = DecodeFixed64(l1_kv_data_start);  // 布隆过滤器种子
                l1_kv_data_start += sizeof(uint64_t);

                bloomfilter_size = DecodeFixed32(l1_kv_data_start);  // 布隆过滤器大小
                l1_kv_data_start += sizeof(uint32_t);


                // 跳过布隆过滤器数据
                l1_kv_data_start += bloomfilter_size;

                // 跳过最小键和最大键
                uint32_t min_key_length, max_key_length;
                l1_kv_data_start = GetVarint32Ptr(l1_kv_data_start, l1_kv_data_start + 5, &min_key_length);
                l1_kv_data_start += min_key_length;  // 跳过最小键

                l1_kv_data_start = GetVarint32Ptr(l1_kv_data_start, l1_kv_data_start + 5, &max_key_length);
                l1_kv_data_start += max_key_length;  // 跳过最大键

                while (l1_kv_data_start < (l1_mmap_ptr + l1_sstable->getFileSize())) {
                    uint32_t key_size = 0, value_size = 0;
                    auto key_ptr = GetVarint32Ptr(l1_kv_data_start, l1_kv_data_start + 5, &key_size);
                    assert(key_ptr != nullptr);
                    std::string internal_key(key_ptr, key_size);
                    l1_kv_data_start += key_size;  // 跳过键

                    auto value_ptr = GetVarint32Ptr(l1_kv_data_start, l1_kv_data_start + 5, &value_size);
                    l1_kv_data_start += value_size;  // 跳过值

                    // 检查 BTree 中是否已经存在该键（即是否 L0 和 L1 键重复）
                    if (btree_set.find(internal_key) != btree_set.end()) {
                        continue;  // 键已经在 L0 层中存在，跳过
                    }

                    // 否则，将 L1 层的键值对插入 BTree
                    l1_btree_set.insert(internal_key);
                    
                    new_bloom_filter.Insert(internal_key);
                    merged_key_size++;
                }
            }
        }

        // 合并完成后，创建新的 L1 SSTable 文件
        std::shared_ptr<SSTable> new_l1_sstable = std::make_shared<SSTable>();
        std::string sstable_file_path = "/home/ubuntu/kdb/file/level1";  // L1 层文件路径
        uint32_t new_sstable_number = static_cast<uint32_t>(GenerateSSTableNumber());
        uint32_t new_sstable_level = 1;

        std::string sstable_filename = "sstable_" + std::to_string(new_sstable_number) + ".sst";

        // 初始化 L1 SSTable
        if (!new_l1_sstable->Init(sstable_file_path, sstable_filename, new_sstable_level, new_sstable_number)) {
            std::cerr << "Failed to initialize new L1 SSTable" << std::endl;
            return;
        }

        // 使用 mmap 将合并后的键值对写入 L1 文件
        char* new_l1_mmap_ptr = nullptr;
        size_t offset_new_l1 = 0;
        if (new_l1_sstable->InitMmap()) {
            new_l1_mmap_ptr = new_l1_sstable->getMmapPtr();
            offset_new_l1 = 0;

            // 写入元信息
            EncodeVarint32(new_l1_mmap_ptr + offset_new_l1, 1);  // 版本号为 1
            offset_new_l1 += sizeof(uint32_t);

            auto current_time = std::chrono::system_clock::now();
            time_t time_t_format = std::chrono::system_clock::to_time_t(current_time);
            uint64_t time_as_uint64 = static_cast<uint64_t>(time_t_format);
            EncodeFixed64(new_l1_mmap_ptr + offset_new_l1, time_as_uint64);
            offset_new_l1 += sizeof(uint64_t);

            EncodeVarint32(new_l1_mmap_ptr + offset_new_l1, new_sstable_level);
            offset_new_l1 += sizeof(uint32_t);
            EncodeVarint32(new_l1_mmap_ptr + offset_new_l1, new_sstable_number);
            offset_new_l1 += sizeof(uint32_t);
            EncodeVarint32(new_l1_mmap_ptr + offset_new_l1, merged_key_size);
            offset_new_l1 += sizeof(uint32_t);

            // 写入新的 Bloom 过滤器
            std::memcpy(new_l1_mmap_ptr + offset_new_l1, new_bloom_filter.getFilterData()->getData(), new_bloom_filter.getBitSetSize());
            offset_new_l1 += new_bloom_filter.getBitSetSize();


            // 写入合并后的键值对
            for (const auto& key_to_write : btree_set) {
                uint32_t key_size = 0, value_size = 0;
                auto key_ptr = GetVarint32Ptr(key_to_write.data(), key_to_write.data() + 5, &key_size);
                assert(key_ptr != nullptr);
                std::string user_key(key_ptr, key_size);

                // 写入键的长度和内容
                EncodeVarint32(new_l1_mmap_ptr + offset_new_l1, key_size);
                offset_new_l1 += VarintLength(key_size);
                std::memcpy(new_l1_mmap_ptr + offset_new_l1, user_key.data(), key_size);
                offset_new_l1 += key_size;

                // 写入键对应的值
                auto value_ptr = GetVarint32Ptr(key_ptr + 1, key_ptr + 6, &value_size);
                if (value_ptr != nullptr) {
                    EncodeVarint32(new_l1_mmap_ptr + offset_new_l1, value_size);
                    offset_new_l1 += VarintLength(value_size);
                    std::memcpy(new_l1_mmap_ptr + offset_new_l1, value_ptr, value_size);
                    offset_new_l1 += value_size;
                }
            }
        }

        // 更新 SSTableView，写入新的 L1 文件
        table_view_manager_.PushTableView(new_sstable_level, new_l1_mmap_ptr, static_cast<uint32_t>(offset_new_l1),
                                            new_l1_sstable, min_user_key, max_user_key);

        // 删除旧的 L0 和 L1 文件
        table_view_manager_.RemoveTableFromLevel(0, l0_sstable->getNumber());
        for (const auto& l1_sstable : l1_sstables) {
            table_view_manager_.RemoveTableFromLevel(1, l1_sstable->getNumber());
        }
    }
    }
}



void Compactor::minorcompaction_task_L1_to_L2() {
    while (table_view_manager_.Needs_L1_to_L2_Compaction()) {

        // 获取 L1 层最早的 SSTable
        std::shared_ptr<SSTable> l1_sstable = table_view_manager_.GetL1SSTable();

        // 读取 L1 文件中的元信息和键值对数据
        MemBTreeView btree_set;  // 使用 BTree 存储内部键
        std::string min_user_key;  // 记录最小用户键
        std::string max_user_key;  // 记录最大用户键
        uint32_t all_key_size = 0; // 记录总的键值对数量

        // 从 L1 文件中读取数据
        char* l1_mmap_ptr = l1_sstable->getMmapPtr();
        const char* kv_data_start = l1_mmap_ptr;
        
        // 读取元信息
        uint32_t version, sstable_level, sstable_number, bloomfilter_size, all_key_count;
        uint64_t creation_time, bloomfilter_seed;

        // 使用定长编码解码版本号
        version = DecodeFixed32(kv_data_start);
        kv_data_start += sizeof(uint32_t);  // 移动指针到下一个数据

        // 使用定长编码解码创建时间
        creation_time = DecodeFixed64(kv_data_start);
        kv_data_start += sizeof(uint64_t);

        // 使用定长编码解码层级
        sstable_level = DecodeFixed32(kv_data_start);
        kv_data_start += sizeof(uint32_t);

        // 使用定长编码解码文件编号
        sstable_number = DecodeFixed32(kv_data_start);
        kv_data_start += sizeof(uint32_t);

        // 使用定长编码解码总键数
        all_key_count = DecodeFixed32(kv_data_start);
        kv_data_start += sizeof(uint32_t);

        // 使用定长编码解码布隆过滤器种子
        bloomfilter_seed = DecodeFixed64(kv_data_start);
        kv_data_start += sizeof(uint64_t);

        // 使用定长编码解码布隆过滤器大小
        bloomfilter_size = DecodeFixed32(kv_data_start);
        kv_data_start += sizeof(uint32_t);


        const char* bloom_filter_data = kv_data_start;
        kv_data_start += bloomfilter_size;  // 跳过布隆过滤器数据

        // 读取最小键和最大键
        uint32_t min_key_length, max_key_length;
        kv_data_start = GetVarint32Ptr(kv_data_start, kv_data_start + 5, &min_key_length);
        min_user_key = std::string(kv_data_start, min_key_length);
        kv_data_start += min_key_length;

        kv_data_start = GetVarint32Ptr(kv_data_start, kv_data_start + 5, &max_key_length);
        max_user_key = std::string(kv_data_start, max_key_length);
        kv_data_start += max_key_length;

        // 遍历键值对数据，插入到 BTree 中
        while (kv_data_start < (l1_mmap_ptr + l1_sstable->getFileSize())) {
            uint32_t key_size = 0, value_size = 0;

            // 读取键的长度
            auto key_ptr = GetVarint32Ptr(kv_data_start, kv_data_start + 5, &key_size);
            assert(key_ptr != nullptr);
            std::string internal_key(key_ptr, key_size);
            kv_data_start += key_size;  // 跳过键

            // 读取值的长度
            auto value_ptr = GetVarint32Ptr(kv_data_start, kv_data_start + 5, &value_size);
            kv_data_start += value_size;  // 跳过值

            // 插入 BTree
            btree_set.insert(internal_key);
            all_key_size++;
        }

        // 判断 L2 有没有文件
        if (table_view_manager_.isLevelEmpty(2)) {

            // 创建新的 L2 文件并写入数据
            std::shared_ptr<SSTable> l2_sstable = std::make_shared<SSTable>();
            std::string sstable_file_path = "/home/ubuntu/kdb/file/level2";  // L2 层文件路径
            sstable_number = static_cast<uint32_t>(GenerateSSTableNumber());
            sstable_level = 2;  // L2 层

            std::string sstable_filename = "sstable_" + std::to_string(sstable_number) + ".sst";

            // 初始化 L2 SSTable
            if (!l2_sstable->Init(sstable_file_path, sstable_filename, sstable_level, sstable_number)) {
                std::cerr << "Failed to initialize L2 SSTable" << std::endl;
                return;
            }

            // 使用 mmap 将 BTree 中的键值对按元信息格式写入 L2 文件
            char* l2_mmap_ptr = nullptr;
            size_t offset_l2 = 0;
            if (l2_sstable->InitMmap()) {
                l2_mmap_ptr = l2_sstable->getMmapPtr();
                offset_l2 = 0;

                // 写入元信息
                EncodeVarint32(l2_mmap_ptr + offset_l2, version);
                offset_l2 += sizeof(uint32_t);

                auto current_time = std::chrono::system_clock::now();
                time_t time_t_format = std::chrono::system_clock::to_time_t(current_time);
                uint64_t time_as_uint64 = static_cast<uint64_t>(time_t_format);
                EncodeFixed64(l2_mmap_ptr + offset_l2, time_as_uint64);
                offset_l2 += sizeof(uint64_t);

                EncodeVarint32(l2_mmap_ptr + offset_l2, sstable_level);
                offset_l2 += sizeof(uint32_t);
                EncodeVarint32(l2_mmap_ptr + offset_l2, sstable_number);
                offset_l2 += sizeof(uint32_t);
                EncodeVarint32(l2_mmap_ptr + offset_l2, all_key_size);
                offset_l2 += sizeof(uint32_t);

                // 写入布隆过滤器
                EncodeFixed64(l2_mmap_ptr + offset_l2, bloomfilter_seed);
                offset_l2 += sizeof(uint64_t);
                EncodeVarint32(l2_mmap_ptr + offset_l2, bloomfilter_size);
                offset_l2 += sizeof(uint32_t);

                std::memcpy(l2_mmap_ptr + offset_l2, bloom_filter_data, bloomfilter_size);
                offset_l2 += bloomfilter_size;

                // 写入最小键和最大键
                EncodeVarint32(l2_mmap_ptr + offset_l2, min_user_key.size());
                offset_l2 += VarintLength(min_user_key.size());
                std::memcpy(l2_mmap_ptr + offset_l2, min_user_key.data(), min_user_key.size());
                offset_l2 += min_user_key.size();

                EncodeVarint32(l2_mmap_ptr + offset_l2, max_user_key.size());
                offset_l2 += VarintLength(max_user_key.size());
                std::memcpy(l2_mmap_ptr + offset_l2, max_user_key.data(), max_user_key.size());
                offset_l2 += max_user_key.size();

                // 遍历 BTree，写入键值对数据
                for (const auto& key_to_write : btree_set) {
                    uint32_t key_size = 0, value_size = 0;
                    auto key_ptr = GetVarint32Ptr(key_to_write.data(), key_to_write.data() + 5, &key_size);
                    assert(key_ptr != nullptr);
                    std::string user_key(key_ptr, key_size);

                    // 写入键的长度和内容
                    EncodeVarint32(l2_mmap_ptr + offset_l2, key_size);
                    offset_l2 += VarintLength(key_size);
                    std::memcpy(l2_mmap_ptr + offset_l2, user_key.data(), key_size);
                    offset_l2 += key_size;

                    // 写入值
                    auto value_ptr = GetVarint32Ptr(key_ptr + 1, key_ptr + 6, &value_size);
                    if (value_ptr != nullptr) {
                        EncodeVarint32(l2_mmap_ptr + offset_l2, value_size);
                        offset_l2 += VarintLength(value_size);
                        std::memcpy(l2_mmap_ptr + offset_l2, value_ptr, value_size);
                        offset_l2 += value_size;
                    }
                }
            }

            // 更新 TableViewManager
            table_view_manager_.PushTableView(sstable_level, l2_mmap_ptr, static_cast<uint32_t>(offset_l2),
                                              l2_sstable, min_user_key, max_user_key);

            // 删除 L1 文件
            table_view_manager_.RemoveTableFromLevel(1, l1_sstable->getNumber());
        } else {
            // 获取 L2 层的所有 SSTable
            std::vector<std::shared_ptr<SSTable>> l2_sstables = table_view_manager_.GetL2SSTables();
            MemBTreeView l2_btree_set;  // 用来存储合并的 L2 层数据
            uint32_t merged_key_size = all_key_size;     // 记录合并后总的键值对数量

            // 遍历 L2 层所有 SSTable，检查与 L1 层 SSTable 是否有键范围重叠
            for (const auto& l2_sstable : l2_sstables) {
                if (l2_sstable->getMaxKey() >= min_user_key && l2_sstable->getMinKey() <= max_user_key) {
                    // 键范围有重叠的 L2 文件，开始合并
                    char* l2_mmap_ptr = l2_sstable->getMmapPtr();
                    
                    // 读取 L2 文件中的元信息，跳过 meta 部分
                    uint32_t version, sstable_level, sstable_number, bloomfilter_size, all_key_count;
                    uint64_t creation_time, bloomfilter_seed;
                    
                    const char* l2_kv_data_start = l2_mmap_ptr;

                    // 读取元信息部分
                    version = DecodeFixed32(l2_kv_data_start);  // 版本号
                    l2_kv_data_start += sizeof(uint32_t);  // 移动指针到下一个数据

                    creation_time = DecodeFixed64(l2_kv_data_start);  // 创建时间
                    l2_kv_data_start += sizeof(uint64_t);

                    sstable_level = DecodeFixed32(l2_kv_data_start);  // 层级
                    l2_kv_data_start += sizeof(uint32_t);

                    sstable_number = DecodeFixed32(l2_kv_data_start);  // 文件编号
                    l2_kv_data_start += sizeof(uint32_t);

                    all_key_count = DecodeFixed32(l2_kv_data_start);  // 总键数量
                    l2_kv_data_start += sizeof(uint32_t);

                    bloomfilter_seed = DecodeFixed64(l2_kv_data_start);  // 布隆过滤器种子
                    l2_kv_data_start += sizeof(uint64_t);

                    bloomfilter_size = DecodeFixed32(l2_kv_data_start);  // 布隆过滤器大小
                    l2_kv_data_start += sizeof(uint32_t);


                    // 跳过布隆过滤器数据
                    l2_kv_data_start += bloomfilter_size;

                    // 跳过最小键和最大键
                    uint32_t min_key_length, max_key_length;
                    l2_kv_data_start = GetVarint32Ptr(l2_kv_data_start, l2_kv_data_start + 5, &min_key_length);
                    l2_kv_data_start += min_key_length;  // 跳过最小键

                    l2_kv_data_start = GetVarint32Ptr(l2_kv_data_start, l2_kv_data_start + 5, &max_key_length);
                    l2_kv_data_start += max_key_length;  // 跳过最大键

                    // 遍历 L2 层键值对，检查是否需要合并
                    while (l2_kv_data_start < (l2_mmap_ptr + l2_sstable->getFileSize())) {
                        uint32_t key_size = 0, value_size = 0;
                        auto key_ptr = GetVarint32Ptr(l2_kv_data_start, l2_kv_data_start + 5, &key_size);
                        assert(key_ptr != nullptr);
                        std::string internal_key(key_ptr, key_size);
                        l2_kv_data_start += key_size;  // 跳过键

                        auto value_ptr = GetVarint32Ptr(l2_kv_data_start, l2_kv_data_start + 5, &value_size);
                        l2_kv_data_start += value_size;  // 跳过值

                        // 检查 BTree 中是否已经存在该键（即是否 L1 和 L2 键重复）
                        if (btree_set.find(internal_key) != btree_set.end()) {
                            // 键已经在 L1 层中存在
                            continue;
                        } else {
                            // 否则，将 L2 层的键值对插入到合并的 BTree
                            l2_btree_set.insert(internal_key);
                            merged_key_size++;
                        }
                    }
                }
            }

            // 合并完成后，创建新的 L2 SSTable 文件
            std::shared_ptr<SSTable> new_l2_sstable = std::make_shared<SSTable>();
            std::string sstable_file_path = "/home/ubuntu/kdb/file/level2";  // L2 层文件路径
            uint32_t new_sstable_number = static_cast<uint32_t>(GenerateSSTableNumber());
            uint32_t new_sstable_level = 2;

            std::string sstable_filename = "sstable_" + std::to_string(new_sstable_number) + ".sst";

            // 初始化 L2 SSTable
            if (!new_l2_sstable->Init(sstable_file_path, sstable_filename, new_sstable_level, new_sstable_number)) {
                std::cerr << "Failed to initialize new L2 SSTable" << std::endl;
                return;
            }

            // 使用 mmap 将合并后的键值对写入 L2 文件
            char* new_l2_mmap_ptr = nullptr;
            size_t offset_new_l2 = 0;
            if (new_l2_sstable->InitMmap()) {
                new_l2_mmap_ptr = new_l2_sstable->getMmapPtr();
                offset_new_l2 = 0;

                // 写入元信息
                EncodeVarint32(new_l2_mmap_ptr + offset_new_l2, 1);  // 版本号为 1
                offset_new_l2 += sizeof(uint32_t);

                auto current_time = std::chrono::system_clock::now();
                time_t time_t_format = std::chrono::system_clock::to_time_t(current_time);
                uint64_t time_as_uint64 = static_cast<uint64_t>(time_t_format);
                EncodeFixed64(new_l2_mmap_ptr + offset_new_l2, time_as_uint64);
                offset_new_l2 += sizeof(uint64_t);

                EncodeVarint32(new_l2_mmap_ptr + offset_new_l2, new_sstable_level);
                offset_new_l2 += sizeof(uint32_t);
                EncodeVarint32(new_l2_mmap_ptr + offset_new_l2, new_sstable_number);
                offset_new_l2 += sizeof(uint32_t);
                EncodeVarint32(new_l2_mmap_ptr + offset_new_l2, merged_key_size);
                offset_new_l2 += sizeof(uint32_t);

                // 遍历合并后的 BTree，将键值对写入
                for (const auto& key_to_write : btree_set) {
                    uint32_t key_size = 0, value_size = 0;
                    auto key_ptr = GetVarint32Ptr(key_to_write.data(), key_to_write.data() + 5, &key_size);
                    assert(key_ptr != nullptr);
                    std::string user_key(key_ptr, key_size);

                    // 写入键的长度和内容
                    EncodeVarint32(new_l2_mmap_ptr + offset_new_l2, key_size);
                    offset_new_l2 += VarintLength(key_size);
                    std::memcpy(new_l2_mmap_ptr + offset_new_l2, user_key.data(), key_size);
                    offset_new_l2 += key_size;

                    // 写入值数据
                    auto value_ptr = GetVarint32Ptr(key_ptr + 1, key_ptr + 6, &value_size);
                    if (value_ptr != nullptr) {
                        EncodeVarint32(new_l2_mmap_ptr + offset_new_l2, value_size);
                        offset_new_l2 += VarintLength(value_size);
                        std::memcpy(new_l2_mmap_ptr + offset_new_l2, value_ptr, value_size);
                        offset_new_l2 += value_size;
                    }
                }
            }

            // 更新 TableViewManager，写入新的 L2 文件
            table_view_manager_.PushTableView(new_sstable_level, new_l2_mmap_ptr, static_cast<uint32_t>(offset_new_l2),
                                            new_l2_sstable, min_user_key, max_user_key);

            // 删除旧的 L1 和 L2 文件
            table_view_manager_.RemoveTableFromLevel(1, l1_sstable->getNumber());
            for (const auto& l2_sstable : l2_sstables) {
                table_view_manager_.RemoveTableFromLevel(2, l2_sstable->getNumber());
            }
        }
    }
}



int BComparator::operator()(std::string_view const& left, 
                            std::string_view const& right) const {
  uint32_t left_key_len = 0;
  auto left_key_len_ptr = GetVarint32Ptr(left.data(), left.data() + 5, &left_key_len);
  assert(left_key_len_ptr != nullptr);
  
  uint32_t right_key_len = 0;
  auto right_key_len_ptr = GetVarint32Ptr(right.data(), right.data() + 5, &right_key_len);
  assert(right_key_len_ptr != nullptr);
  
  std::string_view left_key_value(left_key_len_ptr,left_key_len);
  std::string_view right_key_value(right_key_len_ptr,right_key_len);
  // 比较键值
  if (left_key_value < right_key_value) {
    return -1;
  }
  else if (left_key_value > right_key_value) {
    return 1;
  }
  return 0;
}

}  // namespace kdb