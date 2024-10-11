#include "iouring.h"

#include <asm-generic/errno-base.h>
#include <liburing.h>
#include <liburing/io_uring.h>

#include <cerrno>



namespace kdb {

bool IOUring::Init(const uint32_t ring_depth) {
  ring_depth_ = ring_depth;
  // TODO: 考虑不同的选择方式
  // IORING_SETUP_IOPOLL : 执行忙等待 IO 完成, 而不是通过异步
  // IRQ(中断请求获取通知) IORING_SETUP_SQPOLL :
  auto init_ue = ::io_uring_queue_init(ring_depth_, &ring_, 0);
  // io_uring_queue_init 成功返回 0
  if (init_ue != 0) {
    return false;
  }
  isInit_ = true;
  return true;
}

void IOUring::Stop() {
  if (isInit_ == true) {
    ::io_uring_queue_exit(&ring_);
    isInit_ = false;
  }
}

void IOUring::PrepRead(ReadRequest* request) {
  // 提交队列
  auto submit_queue = io_uring_get_sqe(&ring_);
  ::io_uring_sqe_set_data(submit_queue, request->data_);
  ::io_uring_prep_read(submit_queue, request->fd_, request->data_,
                       request->data_size_, request->_file_offset_);
}

void IOUring::PrepWrite(WriteRequest* write_request) {
  auto submit_queue = io_uring_get_sqe(&ring_);
  ::io_uring_sqe_set_data(submit_queue, write_request->data_);
  ::io_uring_prep_write(submit_queue, write_request->fd_, write_request->data_,
                        write_request->data_size_, write_request->_file_offset);
}

uint32_t IOUring::Submit() {
  auto ue = ::io_uring_submit(&ring_);
  if (ue < 0) {
  }
  return ue;
}

struct ConSumptionQueue IOUring::PeekFinishQueue() {
  // 消费队列
  struct ConSumptionQueue result;
  auto ue = ::io_uring_peek_cqe(&ring_, &(result._con_queue_));
  if (ue == -EAGAIN) {
  }
  return result;
}

struct ConSumptionQueue IOUring::WaitFinishQueue() {
  // 消费队列
  struct ConSumptionQueue result;
  auto ue = ::io_uring_wait_cqe(&ring_, &(result._con_queue_));
  if (ue != 0) {
  }
  return result;
}

void IOUring::DeleteEvent(struct ConSumptionQueue* que) {
  //
  ::io_uring_cqe_seen(&ring_, que->_con_queue_);
}

// 将本次读取请求与下一次请求链接, 保证有序性
void IOUring::PrepReadLink(ReadRequest* request) {
  auto submit_que = ::io_uring_get_sqe(&ring_);
  ::io_uring_sqe_set_data(submit_que, request->data_);
  ::io_uring_prep_read(submit_que, request->fd_, request->data_,
                       request->data_size_, request->_file_offset_);
  submit_que->flags |= IOSQE_IO_LINK;
}

// 将本次写入请求与下一次请求链接, 保证有序性
void IOUring::PrepWriteLink(WriteRequest* write_request) {
  auto submit_queue = io_uring_get_sqe(&ring_);
  ::io_uring_sqe_set_data(submit_queue, write_request->data_);
  ::io_uring_prep_write(submit_queue, write_request->fd_, write_request->data_,
                        write_request->data_size_, write_request->_file_offset);
  submit_queue->flags |= IOSQE_IO_LINK;
}

IOUring::~IOUring() { Stop(); }

}  // namespace iouring

