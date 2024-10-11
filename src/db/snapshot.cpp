#include "snapshot.h"

extern "C" {
#include <assert.h>
}



namespace kdb {

void SnapShot::Init(const uint64_t number) {
  sequence_number_ = number;
}

}  // namespace db

