//#include "src/db/status.h"
#include "status.h"

namespace kdb {

Status::Status()
    : code_(StatusCode::kOk), code_ptr_(statuslev[StatusCode::kOk]) {}

void Status::setCode(StatusCode code) { code_ = code; }

StatusCode Status::getCode() { return code_; }

std::string_view Status::getCodeStr() {
  code_ptr_ = statuslev[static_cast<int>(code_)];
  return code_ptr_;
}

void Status::SetErrorLog(const std::string &error_log) {
  error_log_ = error_log;
}

}  // namespace db