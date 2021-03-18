#pragma once

#include <unordered_map>

#include "wrapsocket.h"

class ProtoRPC {
 public:
  ProtoRPC();
  void Start(Epoll *e, const char *szip, uint16_t port);
  void Start(Epoll *e, const char *unixPath);

 protected:
  template <typename T>
  void Call(GoContext *ctx, uint16_t cmd, const T &req) {
    m_buffer.resize(MSG_HEAD_LEN);
    if (!req.AppendToString(&m_buffer)) {
      fprintf(stderr, "%s:%d cmd:%lu AppendToString failed\n", __FILE__,
              __LINE__, (unsigned long int)cmd);
      return;
    }
    uint32_t seq = GetNextSeq();
    serialMsgHead(&(m_buffer[0]), uint32_t(m_buffer.size()), seq, cmd);
    if (m_psocket == nullptr) {
      m_msg.append(m_buffer);
      return;
    }
    m_psocket->Write(&(m_buffer[0]), m_buffer.size());
  }
  template <typename Req, typename Rsp>
  ErrNo Call(GoContext *ctx, uint16_t cmd, const Req &req, Rsp &rsp) {
    m_buffer.resize(MSG_HEAD_LEN);
    if (!req.AppendToString(&m_buffer)) {
      fprintf(stderr, "%s:%d cmd:%lu AppendToString failed\n", __FILE__,
              __LINE__, (unsigned long int)cmd);
      return EBADMSG;
    }
    GoChan ch(ctx->GetEpoll());
    ErrNo retErr = 0;
    std::function<void(ErrNo, void *, uint32_t)> cb =
        [cmd, &ch, &rsp, &retErr](ErrNo err, void *pdata, uint32_t size) {
          if (err) {
            retErr = err;
            ch.Wake();
            return;
          }
          if (!rsp.ParseFromArray(pdata, int(size))) {
            fprintf(stderr, "%s:%d cmd:%lu ParseFromArray failed\n", __FILE__,
                    __LINE__, (unsigned long int)cmd);
            retErr = EBADMSG;
            ch.Wake();
            return;
          }
          retErr = 0;
          ch.Wake();
        };
    time_t now = curtime();
    uint32_t seq = 0;
    while (true) {
      seq = GetNextSeq();
      auto p = m_waitResp.insert(
          std::pair<Key, Value>(Key(seq, cmd), Value(cb, now)));
      if (p.second) {
        break;
      }
    }
    serialMsgHead(&(m_buffer[0]), uint32_t(m_buffer.size()), seq, cmd);
    if (m_psocket == nullptr) {
      m_msg.append(m_buffer);
    } else {
      m_psocket->Write(&(m_buffer[0]), m_buffer.size());
    }
    ch.Wait(ctx);
    return retErr;
  }

 private:
  void Worker(GoContext &ctx);
  void Check(GoContext &ctx);
  ErrNo doWork(GoContext &ctx);
  void doCheck();
  std::tuple<size_t, ErrNo> onProcess(void *pdata, size_t size);
  uint32_t GetNextSeq() { return ++m_reqSeq; }
  void serialMsgHead(void *pdata, uint32_t length, uint32_t seq, uint16_t cmd);
  void parseMsgHead(void *pdata, uint32_t &length, uint32_t &seq,
                    uint16_t &cmd);

 private:
  struct Key {
    Key(uint32_t seq, uint16_t cmd) : Seq(seq), Cmd(cmd) {}
    bool operator==(const Key &other) const {
      if (Seq != other.Seq) {
        return false;
      }
      if (Cmd != other.Cmd) {
        return false;
      }
      return true;
    }
    uint32_t Seq;
    uint16_t Cmd;
  };
  struct Value {
    Value(std::function<void(ErrNo, void *, uint32_t)> cb, time_t t)
        : CallBack(std::move(cb)), Time(t) {}
    std::function<void(ErrNo, void *, uint32_t)> CallBack;
    time_t Time;
  };
  struct KeyHash {
    std::size_t operator()(const Key &p) const { return p.Seq; }
  };
  const unsigned int MSG_HEAD_LEN = 10;

 private:
  Epoll *m_epoll;
  std::unordered_map<Key, Value, KeyHash> m_waitResp;
  std::string m_msg;
  TcpSocket *m_psocket;
  std::string m_ip;
  uint16_t m_port;
  std::string m_unixPath;
  uint32_t m_reqSeq;
  std::string m_buffer;
};
