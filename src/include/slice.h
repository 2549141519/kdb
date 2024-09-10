// Slice 是一个简单的结构，包含一个指向某个外部存储的指针和一个大小。
// 使用 Slice 的用户必须确保在对应的外部存储被释放后，Slice 不再被使用。
// 切片只是一种映射 全是右值拷贝构造
// 多个线程可以在不进行外部同步的情况下调用 Slice 的 const 方法，
// 但如果任何线程可能调用非 const 方法，所有访问同一 Slice 的线程必须使用外部同步。


//其实就是写了一个自己用的string


#include <cassert>
#include <cstddef>
#include <cstring>
#include <string>

namespace kdb
{
class  Slice 
{
public:
    //默认构造函数  构造函数可以传入string类型和char *类型
    Slice() : data_(""), size_(0) {}
    Slice(const char* d, std::size_t n) : data_(d), size_(n) {}
    Slice(const std::string& s) : data_(s.data()), size_(s.size()) {}
    Slice(const char* s) : data_(s), size_(strlen(s)) {}

    //浅拷贝
    Slice(const Slice&) = default;
    Slice& operator=(const Slice&) = default;

    //返回data 和 长度
    const char* data() const { return data_; }
    std::size_t size() const { return size_; }

    bool empty() const { return size_ == 0; }

    //根据第几位返回对应的char值
    char operator[](std::size_t n) const 
    {
        assert(n < size());
        return data_[n];
    }

    //clear 不释放指针 而是清空slice的映射
    void clear()
    {
        data_ = "";
        size_ = 0;
    }

    //让data指向从第几位开始的元素
    void remove_prefix(std::size_t n)
    {
        assert(n <= size());
        data_ += n;
        size_ -= n;
    }

    //返回一个包含引用数据的string
    std::string ToString() const { return std::string(data_,size_); }
    
    int compare(const Slice& b) const;

    // Return true iff x是this的前缀
    bool starts_with(const Slice& x) const 
    {
    return ((size_ >= x.size_) && (memcmp(data_, x.data_, x.size_) == 0));
    }


private:
    const char* data_;
    std::size_t size_;
};

    inline bool operator==(const Slice& x, const Slice& y) {
    return ((x.size() == y.size()) &&
            (memcmp(x.data(), y.data(), x.size()) == 0));
    }

    inline bool operator!=(const Slice& x, const Slice& y) { return !(x == y); }

    //比较字典序
    inline int Slice::compare(const Slice& b) const 
    {
        const std::size_t min_len = (size_ < b.size_) ? size_ : b.size_;
        int r = memcmp(data_, b.data_, min_len);
        if (r == 0) {
            if (size_ < b.size_)
            r = -1;
            else if (size_ > b.size_)
            r = +1;
        }
        return r;
    }


};





