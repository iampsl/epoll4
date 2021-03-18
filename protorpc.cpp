#include "protorpc.h"

#include <cstdio>
#include <cstring>

/*
  消息由包头和包体组成,包头长度为固定10字节
  |4字节|4字节|2字节|包体|
  4字节   总个包的长度
  4字节   消息序列号
  2字节   消息id
  包体    根据消息id，对应proto文件中message序列化的字节数据
*/

void ProtoRPC::parseMsgHead(void *pdata, uint32_t &length, uint32_t &seq,
                            uint16_t &cmd) {
  uint32_t *plength = (uint32_t *)pdata;
  length = ntohl(*plength);
  uint32_t *pseq = plength + 1;
  seq = ntohl(*pseq);
  uint16_t *pcmd = (uint16_t *)(pseq + 1);
  cmd = ntohs(*pcmd);
}

void ProtoRPC::serialMsgHead(void *pdata, uint32_t length, uint32_t seq,
                             uint16_t cmd) {
  uint32_t *plength = (uint32_t *)pdata;
  *plength = htonl(length);
  uint32_t *pseq = plength + 1;
  *pseq = htonl(seq);
  uint16_t *pcmd = (uint16_t *)(pseq + 1);
  *pcmd = htons(cmd);
}

ProtoRPC::ProtoRPC() {
  m_epoll = nullptr;
  m_psocket = nullptr;
  m_port = 0;
  m_reqSeq = 0;
}

void ProtoRPC::Check(GoContext &ctx) {
  while (true) {
    ctx.Sleep(60);
    doCheck();
  }
}

void ProtoRPC::doCheck() {
  if (m_psocket == nullptr) {
    return;
  }
  time_t now = curtime();
  std::vector<Key> willDel;
  for (auto &&v : m_waitResp) {
    if (now >= v.second.Time + 60) {
      (v.second.CallBack)(ETIMEDOUT, nullptr, 0);
      willDel.push_back(v.first);
    }
  }
  for (auto &&v : willDel) {
    fprintf(stderr, "%s:%d wait timeout seq=%lu cmd=%lu\n", __FILE__, __LINE__,
            (unsigned long int)(v.Seq), (unsigned long int)(v.Cmd));
    m_waitResp.erase(v);
  }
}

void ProtoRPC::Worker(GoContext &ctx) {
  while (true) {
    auto err = doWork(ctx);
    m_psocket = nullptr;
    m_msg.clear();
    for (auto iter = m_waitResp.begin(); iter != m_waitResp.end(); ++iter) {
      (iter->second.CallBack)(err, nullptr, 0);
    }
    m_waitResp.clear();
    ctx.Sleep(2);
  }
}
ErrNo ProtoRPC::doWork(GoContext &ctx) {
  TcpSocket connSocket(ctx.GetEpoll());
  ErrNo err = 0;
  if (m_unixPath.empty()) {
    err = connSocket.Connect(&ctx, m_ip.c_str(), m_port, 5);
  } else {
    err = connSocket.Connect(&ctx, m_unixPath.c_str(), 5);
  }
  if (err) {
    return err;
  }
  m_psocket = &connSocket;
  if (!m_msg.empty()) {
    connSocket.Write(&(m_msg[0]), m_msg.size());
    m_msg.clear();
  }
  char readBuffer[1024 * 1024 * 4];
  size_t readBytes = 0;
  while (true) {
    if (readBytes == sizeof(readBuffer)) {
      return EMSGSIZE;
    }
    ErrNo err = 0;
    size_t nread = 0;
    std::tie(nread, err) = connSocket.Read(&ctx, readBuffer + readBytes,
                                           sizeof(readBuffer) - readBytes);
    if (err) {
      return err;
    }
    if (nread == 0) {
      return ENODATA;
    }
    readBytes += nread;
    size_t procTotal = 0;
    while (true) {
      ErrNo err = 0;
      size_t proc = 0;
      std::tie(proc, err) =
          onProcess(readBuffer + procTotal, readBytes - procTotal);
      if (err) {
        return err;
      }
      if (proc == 0) {
        break;
      }
      procTotal += proc;
    }
    if (procTotal > 0) {
      memcpy(readBuffer, readBuffer + procTotal, readBytes - procTotal);
      readBytes -= procTotal;
    }
  }
}

std::tuple<size_t, ErrNo> ProtoRPC::onProcess(void *pdata, size_t size) {
  if (size < MSG_HEAD_LEN) {
    return std::make_tuple<size_t, ErrNo>(size_t(0), ErrNo(0));
  }
  uint32_t length = 0;
  uint32_t seq = 0;
  uint16_t cmd = 0;
  parseMsgHead(pdata, length, seq, cmd);
  if (length < MSG_HEAD_LEN) {
    return std::make_tuple<size_t, ErrNo>(size_t(0), ErrNo(EBADMSG));
  }
  if (length > size) {
    return std::make_tuple<size_t, ErrNo>(size_t(0), ErrNo(0));
  }
  auto iter = m_waitResp.find(Key(seq, cmd));
  if (iter == m_waitResp.end()) {
    fprintf(stderr,
            "%s:%d nobody need this response msg length=%lu seq=%lu cmd=%lu\n",
            __FILE__, __LINE__, (unsigned long int)(length),
            (unsigned long int)(seq), (unsigned long int)(cmd));
  } else {
    (iter->second.CallBack)(ErrNo(0), ((uint8_t *)pdata) + MSG_HEAD_LEN,
                            length - MSG_HEAD_LEN);
    m_waitResp.erase(iter);
  }
  return std::make_tuple<size_t, ErrNo>(size_t(length), ErrNo(0));
}

void ProtoRPC::Start(Epoll *e, const char *szip, uint16_t port) {
  m_epoll = e;
  m_ip.assign(szip);
  m_port = port;
  m_epoll->Go(std::bind(&ProtoRPC::Worker, this, std::placeholders::_1));
  m_epoll->Go(std::bind(&ProtoRPC::Check, this, std::placeholders::_1));
}

void ProtoRPC::Start(Epoll *e, const char *unixPath) {
  m_epoll = e;
  m_unixPath.assign(unixPath);
  m_epoll->Go(std::bind(&ProtoRPC::Worker, this, std::placeholders::_1));
  m_epoll->Go(std::bind(&ProtoRPC::Check, this, std::placeholders::_1));
}
