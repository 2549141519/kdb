#include "conn_callback.h"

#include <new>

#include "connection.h"
#include "net_server.h"

extern "C" {
#include <arpa/inet.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <netinet/in.h>
#include <sys/socket.h>
}

namespace net {

namespace callback {

void ConnectionReadCallback(struct bufferevent* buf, void* data) {
  auto conn_pair = reinterpret_cast<std::pair<NetServer*, int>*>(data);
  auto conn = conn_pair->first->getConn(conn_pair->second);
  conn->ev_read_buf_ = bufferevent_get_input(buf);
  conn->ev_write_buf_ = bufferevent_get_output(buf);
  if (conn->readHandle_) {
    auto simple_read_len = conn->readHandle_(conn);
    // 重置 readBuf_ 的偏移量
    // 如果 server 没读完, 则返回 -1, 然后保存本次数据, 下一次继续读完就行了.
  }
}

void ConnectionWriteCallback(struct ::bufferevent* buf, void* data) {
  auto conn_pair = reinterpret_cast<std::pair<NetServer*, int>*>(data);
  auto conn = conn_pair->first->getConn(conn_pair->second);
  conn->ev_read_buf_ = bufferevent_get_input(buf);
  conn->ev_write_buf_ = bufferevent_get_output(buf);
  if (conn->writeHandle_) {
    conn->writeHandle_(conn);
  }
}

void ConnectionEventCallback(struct ::bufferevent* buf, short eventWhat,
                             void* data) {
  auto conn_pair = reinterpret_cast<std::pair<NetServer*, int>*>(data);
  auto conn = conn_pair->first->getConn(conn_pair->second);
  if (eventWhat & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    if (conn->closeHandle_) {
      conn->closeHandle_(conn);
    }
    conn->server_->removeConn(conn->socketFd_);
    return;
  } else if (eventWhat & (BEV_EVENT_TIMEOUT)) {
    if (conn->timeoutHandle_) {
      conn->timeoutHandle_(conn);
    }
    // 移除自己在 server 中的连接
    conn->server_->removeConn(conn->socketFd_);
    return;
  }
}

}  // namespace callback

}  // namespace net
