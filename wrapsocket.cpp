#include "wrapsocket.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

#define ErrorInfo(err) \
  fprintf(stderr, "%s:%d errno=%d\n", __FILE__, __LINE__, int(err))

ErrNo SetNoblock(int fd) {
  int iflag = fcntl(fd, F_GETFL, 0);
  if (-1 == iflag) {
    return errno;
  }
  iflag = fcntl(fd, F_SETFL, iflag | O_NONBLOCK);
  if (-1 == iflag) {
    return errno;
  }
  return 0;
}

void SockAddr(sockaddr_in &addr, const char *szip, uint16_t port) {
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(szip);
  addr.sin_port = htons(port);
}

AcceptSocket::AcceptSocket(Epoll *e) {
  m_epoll = e;
  m_fd = -1;
  m_inWait = nullptr;
}

AcceptSocket::~AcceptSocket() { Close(); }

ErrNo AcceptSocket::Listen(const char *szip, uint16_t port, int backlog) {
  if (m_fd != -1) {
    return EEXIST;
  }
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    return errno;
  }
  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(szip);
  addr.sin_port = htons(port);
  int ibind = ::bind(fd, (const sockaddr *)(&addr), sizeof(addr));
  if (-1 == ibind) {
    int err = errno;
    if (close(fd) != 0) {
      ErrorInfo(errno);
    }
    return err;
  }
  if (backlog < 128) {
    backlog = 128;
  }
  int ilisten = listen(fd, backlog);
  if (-1 == ilisten) {
    int err = errno;
    if (close(fd) != 0) {
      ErrorInfo(errno);
    }
    return err;
  }
  int iset = SetNoblock(fd);
  if (iset != 0) {
    if (close(fd) != 0) {
      ErrorInfo(errno);
    }
    return iset;
  }
  int iadd = m_epoll->add(fd, this);
  if (iadd == 0) {
    m_fd = fd;
    return 0;
  }
  if (close(fd) != 0) {
    ErrorInfo(errno);
  }
  return iadd;
}

ErrNo AcceptSocket::Listen(const char *unixPath, int backlog) {
  if (m_fd != -1) {
    return EEXIST;
  }
  sockaddr_un addr;
  if (strlen(unixPath) >= sizeof(addr.sun_path)) {
    return ENAMETOOLONG;
  }
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd == -1) {
    return errno;
  }
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, unixPath);
  int ibind = ::bind(fd, (const sockaddr *)&addr,
                     offsetof(sockaddr_un, sun_path) + strlen(addr.sun_path));
  if (-1 == ibind) {
    int err = errno;
    if (close(fd) != 0) {
      ErrorInfo(errno);
    }
    return err;
  }
  if (backlog < 128) {
    backlog = 128;
  }
  int ilisten = listen(fd, backlog);
  if (-1 == ilisten) {
    int err = errno;
    if (close(fd) != 0) {
      ErrorInfo(errno);
    }
    return err;
  }
  int iset = SetNoblock(fd);
  if (iset != 0) {
    if (close(fd) != 0) {
      ErrorInfo(errno);
    }
    return iset;
  }
  int iadd = m_epoll->add(fd, this);
  if (iadd == 0) {
    m_fd = fd;
    return 0;
  }
  if (close(fd) != 0) {
    ErrorInfo(errno);
  }
  return iadd;
}

std::tuple<int, ErrNo> AcceptSocket::Accept(GoContext *ctx) {
  while (true) {
    int s = accept(m_fd, nullptr, nullptr);
    if (s != -1) {
      return std::make_tuple(s, 0);
    }
    int err = errno;
    if (err != EAGAIN) {
      return std::make_tuple(-1, err);
    }
    m_inWait = ctx;
    ctx->Out();
  }
}

void AcceptSocket::Close() {
  if (-1 == m_fd) {
    return;
  }
  m_epoll->del(m_fd, this);
  if (close(m_fd) != 0) {
    ErrorInfo(errno);
  }
  m_fd = -1;
  if (m_inWait == nullptr) {
    return;
  }
  GoContext *tmpWait = m_inWait;
  m_inWait = nullptr;
  m_epoll->push([tmpWait]() { tmpWait->In(); });
}

void AcceptSocket::OnIn() {
  if (m_inWait == nullptr) {
    return;
  }
  GoContext *tmpWait = m_inWait;
  m_inWait = nullptr;
  tmpWait->In();
}

void AcceptSocket::OnOut() {}

TcpSocket::TcpSocket(Epoll *e) {
  m_epoll = e;
  m_inWait = nullptr;
  m_connWait = nullptr;
  m_fd = -1;
  m_sendFail = false;
}

TcpSocket::~TcpSocket() { Close(); }

ErrNo TcpSocket::Open(int fd) {
  if (m_fd != -1) {
    return EEXIST;
  }
  int iset = SetNoblock(fd);
  if (iset != 0) {
    if (close(fd) != 0) {
      ErrorInfo(errno);
    }
    return iset;
  }
  auto iadd = m_epoll->add(fd, this);
  if (iadd == 0) {
    m_fd = fd;
    return 0;
  }
  if (close(fd) != 0) {
    ErrorInfo(errno);
  }
  return iadd;
}

ErrNo TcpSocket::Connect(GoContext *ctx, const char *szip, uint16_t port,
                         unsigned int seconds) {
  if (m_fd != -1) {
    return EEXIST;
  }
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    return errno;
  }
  int iset = SetNoblock(fd);
  if (iset != 0) {
    if (close(fd) != 0) {
      ErrorInfo(errno);
    }
    return iset;
  }
  int iadd = m_epoll->add(fd, this);
  if (iadd != 0) {
    if (close(fd) != 0) {
      ErrorInfo(errno);
    }
    return iadd;
  }
  m_fd = fd;
  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(szip);
  addr.sin_port = htons(port);
  return doConnectWithTimeout(ctx, (const sockaddr *)(&addr), sizeof(addr),
                              seconds);
}
ErrNo TcpSocket::Connect(GoContext *ctx, const char *unixPath,
                         unsigned int seconds) {
  if (m_fd != -1) {
    return EEXIST;
  }
  sockaddr_un addr;
  if (strlen(unixPath) >= sizeof(addr.sun_path)) {
    return ENAMETOOLONG;
  }
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd == -1) {
    return errno;
  }
  int iset = SetNoblock(fd);
  if (iset != 0) {
    if (close(fd) != 0) {
      ErrorInfo(errno);
    }
    return iset;
  }
  int iadd = m_epoll->add(fd, this);
  if (iadd != 0) {
    if (close(fd) != 0) {
      ErrorInfo(errno);
    }
    return iadd;
  }
  m_fd = fd;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, unixPath);
  return doConnectWithTimeout(
      ctx, (const sockaddr *)(&addr),
      offsetof(sockaddr_un, sun_path) + strlen(addr.sun_path), seconds);
}

ErrNo TcpSocket::doConnect(GoContext *ctx, const sockaddr *addr,
                           socklen_t len) {
  int iconn = connect(m_fd, addr, len);
  if (0 == iconn) {
    return 0;
  }
  int err = errno;
  if (err != EINPROGRESS) {
    return err;
  }
  m_connWait = ctx;
  ctx->Out();
  int error = 0;
  socklen_t length = sizeof(error);
  if (getsockopt(m_fd, SOL_SOCKET, SO_ERROR, &error, &length) != 0) {
    return errno;
  }
  return error;
}

ErrNo TcpSocket::doConnectWithTimeout(GoContext *ctx, const sockaddr *addr,
                                      socklen_t len, unsigned int seconds) {
  if (seconds == 0) {
    return doConnect(ctx, addr, len);
  }
  GoChan tmpChan(ctx->GetEpoll());
  bool timeout = false;
  TcpSocket *pSocket = this;
  time_t endpoint = curtime() + seconds;
  ctx->GetEpoll()->Go([&tmpChan, &timeout, &pSocket, endpoint](GoContext &ctx) {
    while (true) {
      if (pSocket == nullptr) {
        tmpChan.Wake();
        return;
      }
      time_t now = curtime();
      if (now >= endpoint) {
        timeout = true;
        pSocket->Close();
        return;
      }
      ctx.Sleep(1);
    }
  });
  auto err = doConnect(ctx, addr, len);
  if (timeout) {
    return ETIMEDOUT;
  }
  pSocket = nullptr;
  tmpChan.Wait(ctx);
  return err;
}

void TcpSocket::Write(const void *buf, size_t nbytes) {
  if (m_sendFail) {
    return;
  }
  if (nbytes == 0 || buf == nullptr) {
    return;
  }
  auto len = m_writeBuffer.size();
  if (len != 0) {
    m_writeBuffer.resize(len + nbytes);
    memcpy(&(m_writeBuffer[len]), buf, nbytes);
    return;
  }
  size_t total = 0;
  while (total != nbytes) {
    auto isend = send(m_fd, (uint8_t *)buf + total, nbytes - total, 0);
    if (isend >= 0) {
      total += isend;
      continue;
    }
    if (errno != EAGAIN) {
      m_sendFail = true;
      return;
    }
    break;
  }
  if (total == nbytes) {
    return;
  }
  m_writeBuffer.resize(nbytes - total);
  memcpy(&(m_writeBuffer[0]), (uint8_t *)buf + total, nbytes - total);
}

std::tuple<size_t, ErrNo> TcpSocket::Read(GoContext *ctx, void *buf,
                                          size_t nbytes) {
  while (true) {
    auto irecv = recv(m_fd, buf, nbytes, 0);
    if (irecv >= 0) {
      return std::make_tuple<size_t, ErrNo>(size_t(irecv), 0);
    }
    int err = errno;
    if (err != EAGAIN) {
      return std::make_tuple<size_t, ErrNo>(size_t(0), ErrNo(err));
    }
    m_inWait = ctx;
    ctx->Out();
  }
}

void TcpSocket::Close() {
  if (-1 == m_fd) {
    return;
  }
  m_epoll->del(m_fd, this);
  if (close(m_fd) != 0) {
    ErrorInfo(errno);
  }
  m_fd = -1;
  m_sendFail = false;
  m_writeBuffer.clear();
  if (m_connWait != nullptr) {
    GoContext *tmpWait = m_connWait;
    m_connWait = nullptr;
    m_epoll->push([tmpWait]() { tmpWait->In(); });
  }
  if (m_inWait != nullptr) {
    GoContext *tmpWait = m_inWait;
    m_inWait = nullptr;
    m_epoll->push([tmpWait]() { tmpWait->In(); });
  }
}

void TcpSocket::OnIn() {
  if (m_connWait != nullptr) {
    GoContext *tmpWait = m_connWait;
    m_connWait = nullptr;
    tmpWait->In();
    return;
  }
  if (m_inWait != nullptr) {
    GoContext *tmpWait = m_inWait;
    m_inWait = nullptr;
    tmpWait->In();
  }
}

void TcpSocket::OnOut() {
  if (m_connWait != nullptr) {
    GoContext *tmpWait = m_connWait;
    m_connWait = nullptr;
    tmpWait->In();
    return;
  }
  auto len = m_writeBuffer.size();
  if (len == 0) {
    return;
  }
  size_t total = 0;
  while (total != len) {
    auto isend = send(m_fd, &(m_writeBuffer[total]), len - total, 0);
    if (isend >= 0) {
      total += isend;
      continue;
    }
    if (errno != EAGAIN) {
      m_sendFail = true;
      m_writeBuffer.clear();
      return;
    }
    break;
  }
  if (total == len) {
    m_writeBuffer.clear();
    return;
  }
  memcpy(&(m_writeBuffer[0]), &(m_writeBuffer[total]), len - total);
  m_writeBuffer.resize(len - total);
}

UdpSocket::UdpSocket(Epoll *e) {
  m_epoll = e;
  m_inWait = nullptr;
  m_fd = -1;
}

UdpSocket::~UdpSocket() { Close(); }

ErrNo UdpSocket::Open() {
  if (m_fd != -1) {
    return EEXIST;
  }
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd == -1) {
    return errno;
  }
  int iset = SetNoblock(fd);
  if (iset != 0) {
    if (close(fd) != 0) {
      ErrorInfo(errno);
    }
    return iset;
  }
  int iadd = m_epoll->add(fd, this);
  if (iadd != 0) {
    if (close(fd) != 0) {
      ErrorInfo(errno);
    }
    return iadd;
  }
  m_fd = fd;
  return 0;
}

ErrNo UdpSocket::Bind(const char *szip, uint16_t port) {
  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(szip);
  addr.sin_port = htons(port);
  int ibind = ::bind(m_fd, (const sockaddr *)(&addr), sizeof(addr));
  if (-1 == ibind) {
    return errno;
  }
  return 0;
}

std::tuple<size_t, ErrNo> UdpSocket::Recvfrom(GoContext *ctx, void *buf,
                                              size_t len,
                                              sockaddr_in &srcAddr) {
  while (true) {
    socklen_t addrLen = sizeof(srcAddr);
    auto irecv = recvfrom(m_fd, buf, len, 0, (sockaddr *)(&srcAddr), &addrLen);
    if (irecv >= 0) {
      return std::make_tuple<size_t, ErrNo>(size_t(irecv), 0);
    }
    int err = errno;
    if (err != EAGAIN) {
      return std::make_tuple<size_t, ErrNo>(size_t(0), ErrNo(err));
    }
    m_inWait = ctx;
    ctx->Out();
  }
}
void UdpSocket::Sendto(const void *buf, size_t len, sockaddr_in &dstAddr) {
  if (len == 0 || buf == nullptr) {
    return;
  }
  auto size = m_writeBuffer.size();
  if (size != 0) {
    m_writeBuffer.resize(size + sizeof(dstAddr) + sizeof(len) + len);
    *((sockaddr_in *)(&(m_writeBuffer[len]))) = dstAddr;
    *((size_t *)(&(m_writeBuffer[size + sizeof(dstAddr)]))) = len;
    memcpy(&(m_writeBuffer[size + sizeof(dstAddr) + sizeof(len)]), buf, len);
    return;
  }
  auto isend = sendto(m_fd, buf, len, 0, (sockaddr *)(&dstAddr),
                      socklen_t(sizeof(dstAddr)));
  if (isend != -1) {
    return;
  }
  if (errno != EAGAIN) {
    return;
  }
  m_writeBuffer.resize(sizeof(dstAddr) + sizeof(len) + len);
  *((sockaddr_in *)(&(m_writeBuffer[0]))) = dstAddr;
  *((size_t *)(&(m_writeBuffer[sizeof(dstAddr)]))) = len;
  memcpy(&(m_writeBuffer[sizeof(dstAddr) + sizeof(len)]), buf, len);
}
void UdpSocket::Close() {
  if (-1 == m_fd) {
    return;
  }
  m_epoll->del(m_fd, this);
  if (close(m_fd) != 0) {
    ErrorInfo(errno);
  }
  m_fd = -1;
  m_writeBuffer.clear();
  if (m_inWait == nullptr) {
    return;
  }
  GoContext *tmpWait = m_inWait;
  m_inWait = nullptr;
  m_epoll->push([tmpWait]() { tmpWait->In(); });
}

void UdpSocket::OnIn() {
  if (m_inWait == nullptr) {
    return;
  }
  GoContext *tmpWait = m_inWait;
  m_inWait = nullptr;
  tmpWait->In();
}

void UdpSocket::OnOut() {
  auto size = m_writeBuffer.size();
  if (size == 0) {
    return;
  }
  size_t total = 0;
  while (total != size) {
    sockaddr_in *paddr = (sockaddr_in *)(&(m_writeBuffer[total]));
    size_t *plen = (size_t *)(paddr + 1);
    void *pdata = plen + 1;
    auto isend = sendto(m_fd, pdata, *plen, 0, (const sockaddr *)paddr,
                        (socklen_t)(sizeof(*paddr)));
    if (isend == -1 && errno == EAGAIN) {
      break;
    }
    total += sizeof(*paddr) + sizeof(*plen) + *plen;
  }
  if (total == size) {
    m_writeBuffer.clear();
    return;
  }
  memcpy(&(m_writeBuffer[0]), &(m_writeBuffer[total]), size - total);
  m_writeBuffer.resize(size - total);
}
