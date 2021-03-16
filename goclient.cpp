#include "goclient.h"

#include <cstdio>
#include <cstring>

/*
  消息由包头和包体组成,包头长度为固定11字节
  |4字节|4字节|1字节|2字节|包体|
  4字节   总个包的长度
  4字节   消息序列号
  1字节   请求响应标示   0代表请求包，1代表响应包
  2字节   消息id
  包体    根据消息id，对应proto文件中message序列化的字节数据
*/

const int MSG_HEAD_LEN = 11;

void parseMsgHead(void *pdata, uint32_t &length, uint32_t &seq, uint8_t &flag,
                  uint16_t &cmd) {
  uint32_t *plength = (uint32_t *)pdata;
  length = ntohl(*plength);
  uint32_t *pseq = plength + 1;
  seq = ntohl(*pseq);
  uint8_t *pflag = (uint8_t *)(pseq + 1);
  flag = *pflag;
  uint16_t *pcmd = (uint16_t *)(pflag + 1);
  cmd = ntohs(*pcmd);
}

GoClient::GoClient() {
  m_epoll = nullptr;
  m_psocket = nullptr;
  m_port = 0;
  m_reqSeq = 0;
}

void GoClient::Worker(GoContext &ctx) {
  while (true) {
    auto err = doWork(ctx);
    ErrorInfo(err);
    m_psocket = nullptr;
    m_msg.clear();
    for (auto iter = m_waitResp.begin(); iter != m_waitResp.end(); ++iter) {
      (iter->second)(err, nullptr, 0);
    }
    m_waitResp.clear();
    ctx.Sleep(2);
  }
}
ErrNo GoClient::doWork(GoContext &ctx) {
  TcpSocket connSocket(ctx.GetEpoll());
  ErrNo err = connSocket.Open();
  if (err) {
    return err;
  }
  err = connSocket.ConnectWithTimeOut(&ctx, m_ip.c_str(), m_port, 5);
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

std::tuple<size_t, ErrNo> GoClient::onProcess(void *pdata, size_t size) {
  if (size < MSG_HEAD_LEN) {
    return std::make_tuple<size_t, ErrNo>(size_t(0), ErrNo(0));
  }
  uint32_t length = 0;
  uint32_t seq = 0;
  uint8_t flag = 0;
  uint16_t cmd = 0;
  parseMsgHead(pdata, length, seq, flag, cmd);
  if (length < MSG_HEAD_LEN) {
    return std::make_tuple<size_t, ErrNo>(size_t(0), ErrNo(EBADMSG));
  }
  if (length > size) {
    return std::make_tuple<size_t, ErrNo>(size_t(0), ErrNo(0));
  }
  if (flag == 1) {
    auto iter = m_waitResp.find(std::pair<uint32_t, uint16_t>(seq, cmd));
    if (iter == m_waitResp.end()) {
      fprintf(stderr,
              "nobody need this response msg length=%lu seq=%lu cmd=%lu\n",
              (unsigned long int)(length), (unsigned long int)(seq),
              (unsigned long int)(cmd));
    } else {
      (iter->second)(ErrNo(0), ((uint8_t *)pdata) + MSG_HEAD_LEN,
                     length - MSG_HEAD_LEN);
      m_waitResp.erase(iter);
    }
  } else {
    fprintf(stderr, "recv a request msg length=%lu seq=%lu cmd=%lu\n",
            (unsigned long int)(length), (unsigned long int)(seq),
            (unsigned long int)(cmd));
  }
  return std::make_tuple<size_t, ErrNo>(size_t(length), ErrNo(0));
}

void GoClient::Start(Epoll *e, const char *szip, uint16_t port) {
  m_epoll = e;
  m_ip.assign(szip);
  m_port = port;
  m_epoll->Go(std::bind(&GoClient::Worker, this, std::placeholders::_1));
}