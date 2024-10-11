#include <gtest/gtest.h>
#include "../comp.h"
#include "../../utils/coding.h"
#include <fmt/core.h>

namespace kdb {

// 测试用例：用于比较 std::string_view 的 Comparator

const std::string kEmpty1Space = " ";

// for format_style
const std::string kEmpty2Space = "  ";
// for format_style
const std::string kEmpty3Space = "   ";
const std::string kEmpty4Space = "    ";
const std::string kEmpty5Space = "     ";
const std::string kEmpty6Space = "      ";
const std::string kEmpty7Space = "       ";
const std::string kEmpty8Space = "        ";
const std::string kEmpty9Space = "         ";

// 为了格式化, 8 个空格

const std::string format32_vec[] = {
    kEmpty1Space,  // 0 => 1 个空格
    kEmpty1Space,  // 1 => 1 个空格
    kEmpty2Space,  // 2 => 2 个空格
    kEmpty3Space,  // 3 => 3 个空格
    kEmpty4Space,  // 4 => 4 个空格
    kEmpty5Space,  // 5 => 5 个空格
};

const std::string format64_vec[] = {
    kEmpty1Space,  // 0 => 1 个空格
    kEmpty1Space,  // 1 => 1 个空格
    kEmpty2Space,  // 2 => 2 个空格
    kEmpty3Space,  // 3 => 3 个空格
    kEmpty4Space,  // 4 => 4 个空格
    kEmpty5Space,  // 5 => 5 个空格
    kEmpty6Space,  // 6 => 6 个空格
    kEmpty7Space,  // 7 => 7 个空格
    kEmpty8Space,  // 8 => 8 个空格
    kEmpty9Space,  // 9 => 9 个空格
};

// 为了格式化, 32 个空格
const std::string kEmpty32Space = "                                ";

enum ValueType {
  // 删除的标志
  kTypeDeletion = 0,
  kTypeValue = 1,
};


struct SetContext {
  SetContext() = default;
  std::string key;
  std::string value;
  uint64_t number;
  SetContext(const std::string_view& key_view,
             const std::string_view& value_view)
      : key(key_view), value(value_view) {}
};

std::string Set(const std::shared_ptr<SetContext>& set_context) {
  auto key_size = VarintLength(set_context->key.size());
  auto value_size = VarintLength(set_context->value.size());
  auto sequence_number = VarintLength(set_context->value.size());

  std::string simple_set_str = fmt::format(
      "{}{}{}{}{}{}", format32_vec[key_size], set_context->key,
      format64_vec[sequence_number], kEmpty1Space,
      format32_vec[value_size], set_context->value);

  char* start_ptr = simple_set_str.data();
  start_ptr = EncodeVarint32(start_ptr, set_context->key.size());

  start_ptr += set_context->key.size();
  start_ptr = EncodeVarint64(start_ptr, set_context->number);

  EncodeFixed8(start_ptr,ValueType::kTypeValue);

  start_ptr += 1;
  EncodeVarint32(start_ptr, set_context->value.size());

  return simple_set_str;
}

class ComparatorTest : public ::testing::Test {
protected:
    Comparator cmp;  // 创建一个 Comparator 实例
};

// 测试相同的 std::string_view
TEST_F(ComparatorTest, CompareEqualStringView) {
    SetContext context1(std::string_view("key1"), std::string_view("key"));
    context1.number = 1;
    SetContext context2(std::string_view("key2"), std::string_view("key"));
    context2.number = 2;

    std::string_view testcontext1 = Set(std::make_shared<SetContext>(context1));
    std::string_view testcontext2 = Set(std::make_shared<SetContext>(context2));

    EXPECT_EQ(-1,cmp(testcontext1, testcontext2));  // session1 < session2，
    EXPECT_EQ(1,cmp(testcontext2, testcontext1)); // session2 > session1，
}

// 测试不同的 std::string_view
TEST_F(ComparatorTest, CompareDifferentStringView) {
    SetContext context1(std::string_view("key1"), std::string_view("key1"));
    SetContext context2(std::string_view("key2"), std::string_view("key2"));
    
    std::string_view testcontext1 = Set(std::make_shared<SetContext>(context1));
    std::string_view testcontext2 = Set(std::make_shared<SetContext>(context2));

    EXPECT_EQ(-1,cmp(testcontext1, testcontext2)); 
    EXPECT_EQ(1,cmp(testcontext2, testcontext1)); 
}

// 测试空字符串的比较
TEST_F(ComparatorTest, CompareEmptyStringView) {
    SetContext context1(std::string_view("key1"), std::string_view(""));
    SetContext context2(std::string_view("key2"), std::string_view("key"));

    std::string_view testcontext1 = Set(std::make_shared<SetContext>(context1));
    std::string_view testcontext2 = Set(std::make_shared<SetContext>(context2));

    EXPECT_EQ(-1,cmp(testcontext1, testcontext2)); 
    EXPECT_EQ(1,cmp(testcontext2, testcontext1)); 
}

}