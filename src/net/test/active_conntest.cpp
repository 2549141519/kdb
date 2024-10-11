#include "../active_conn.h"

#include <functional>
#include <memory>
#include <thread>
#include <gtest/gtest.h>

#include "../connection.h"
#include "../net_server.h"
#include "../repeated_timer.h"

using ::net::Connection;
using ::net::NetServer;
using ::net::SingleTimer;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

namespace net {

struct Buffer {
  char* bufptr_;
  uint32_t offset_;
  uint32_t buflen_;
  Buffer() : bufptr_(nullptr), offset_(0), buflen_(0) {}
  Buffer(uint32_t len)
      : bufptr_(new (std::nothrow) char[len]), offset_(0), buflen_(len) {
    std::memset(bufptr_, 0, len);
  }
  ~Buffer() {
    if (nullptr != bufptr_) {
      delete[] bufptr_;
      bufptr_ = nullptr;
    }
    bufptr_ = nullptr;
  }
};

static const std::string kTimerHello =
    "for test\n";

class ActiveConnServer {
 public:
  ActiveConnServer() : buf_(1024) { server = std::make_shared<NetServer>(); }

  void Init(const uint32_t port) {
    server->Init(port, std::bind(&ActiveConnServer::writeHd, this, _1),
                 std::bind(&ActiveConnServer::closeHd, this, _1),
                 std::bind(&ActiveConnServer::timeoutHd, this, _1),
                 std::bind(&ActiveConnServer::readHd, this, _1));
    struct timeval time_val;
    time_val.tv_sec = 2;
    time_val.tv_usec = 0;
    timer_.Init(time_val, std::bind(&ActiveConnServer::timerHD, this, _1),
                server, "for test");
    thread_ = std::thread([&]() { server->Run(); });
    timer_.Run();
    conn_.Init("127.0.0.1", 9090, server,
               std::bind(&ActiveConnServer::readHd, this, _1),
               std::bind(&ActiveConnServer::writeHd, this, _1),
               std::bind(&ActiveConnServer::closeHd, this, _1),
               std::bind(&ActiveConnServer::timeoutHd, this, _1));
    conn_.Run();
  }

  int writeHd(const std::shared_ptr<Connection>& conn) {
    return 1;
  }

  int closeHd(const std::shared_ptr<Connection>& conn) {
    try {
        std::cout << "conn ip: " << conn->getPeerIP() << " port: " << conn->getPeerPort() << " close connection" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception in closeHd: " << e.what() << std::endl;
    }
    return 1;
  }

  int timeoutHd(const std::shared_ptr<Connection>& conn) {
    std::cout << "conn ip: " << conn->getPeerIP() << " port: " << conn->getPeerPort() << " connection time out" << std::endl;
    return 1;
  }

  int readHd(const std::shared_ptr<Connection>& conn) {
    return -1;
  }
  
  std::map<int, std::shared_ptr<Connection>> getConnMap() {
    return server->getConnMap();
}

  auto getserver() {
    return server;
  }

  int timerHD(RepeatedTimer* timer) {
    auto mtx = server->getMutex();
    mtx->lock();
    auto connMap = getConnMap();
    if (connMap.empty()) {
      mtx->unlock();
      return -1;
    }
    for (auto& [fd, conn] : connMap) {
      conn->Send(kTimerHello.c_str(), kTimerHello.size());
    }
    mtx->unlock();
    return 1;
  }

 private:
  std::thread thread_;
  Buffer buf_;
  std::shared_ptr<NetServer> server;
  RepeatedTimer timer_;
  Connectioner conn_;
};

TEST(ActiveConnServerTest, CloseHandlerTest) {
  ActiveConnServer server;
  server.Init(9999);
  auto conn = std::make_shared<Connection>(
      server.getConnMap().begin()->first,
      std::bind(&ActiveConnServer::readHd, &server, std::placeholders::_1),
      std::bind(&ActiveConnServer::writeHd, &server, std::placeholders::_1),
      std::bind(&ActiveConnServer::closeHd, &server, std::placeholders::_1),
      std::bind(&ActiveConnServer::timeoutHd, &server, std::placeholders::_1),
      server.getserver().get()
  );
  std::cout << "socket fd: " << server.getConnMap().begin()->first << std::endl;
  /*if (conn -> Init()){
    EXPECT_NO_THROW(server.closeHd(conn));
  }
  else {
    std::cout << "conn init failed" << std::endl;
  }*/
  conn->Init();
  server.closeHd(conn);
}
/*
TEST(ActiveConnServerTest, TimeoutHandlerTest) {
  ActiveConnServer server;
  server.Init(9999);
  auto conn = std::make_shared<Connection>("127.0.0.1", 9090); // 需根据实际连接创建方式修改
  EXPECT_NO_THROW(server.timeoutHd(conn));
}
*/
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
}  // namespace net