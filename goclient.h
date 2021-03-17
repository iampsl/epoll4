#pragma once

#include <unordered_map>

#include "wrapsocket.h"

class GoClient {
 public:
  GoClient();
  void Start(Epoll *e, const char *szip, uint16_t port);
  void Start(Epoll *e, const char *unixPath);
  void QueryUserInfo(GoContext *ctx, const std::string &username);

 private:
  void Worker(GoContext &ctx);
  void Check(GoContext &ctx);
  ErrNo doWork(GoContext &ctx);
  void doCheck();
  std::tuple<size_t, ErrNo> onProcess(void *pdata, size_t size);

 private:
  struct PairHash {
    std::size_t operator()(const std::pair<uint32_t, uint16_t> &p) const {
      return p.first;
    }
  };

 private:
  Epoll *m_epoll;
  std::unordered_map<
      std::pair<uint32_t, uint16_t>,
      std::pair<std::function<void(ErrNo, void *, uint32_t)>, time_t>, PairHash>
      m_waitResp;
  std::vector<uint8_t> m_msg;
  TcpSocket *m_psocket;
  std::string m_ip;
  uint16_t m_port;
  std::string m_unixPath;
  uint32_t m_reqSeq;
};
