#ifndef SRC_BASE_NONCOPYABLE_H_
#define SRC_BASE_NONCOPYABLE_H_
namespace base {

class NonCopyable { //禁止对象复制和赋值
 protected:
  constexpr NonCopyable() = default;
  ~NonCopyable() = default;

 public:
  NonCopyable(const NonCopyable&) = delete;
  NonCopyable& operator=(const NonCopyable&) = delete;
};
}  // namespace base

#endif