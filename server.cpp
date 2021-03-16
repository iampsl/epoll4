#include "server.h"

#include <sys/time.h>

#include <cstring>
#include <iostream>

#include "wrapsocket.h"

const uint16_t port = 8888;

uint64_t count = 0;

void NewConnect(GoContext &ctx, int s) {
  TcpSocket ptcp(ctx.GetEpoll());
  if (auto err = ptcp.Open(s)) {
    std::cout << strerror(err) << std::endl;
    return;
  }
  uint8_t pbuffer[1024];
  size_t size = 0;
  while (true) {
    ErrNo err = 0;
    size_t nread = 0;
    std::tie(nread, err) =
        ptcp.Read(&ctx, pbuffer + size, sizeof(pbuffer) - size);
    if (err != 0) {
      std::cout << strerror(err) << std::endl;
      return;
    }
    if (nread == 0) {
      return;
    }
    size += nread;
    if (size == sizeof(pbuffer)) {
      ptcp.Write(pbuffer, sizeof(pbuffer));
      size = 0;
    }
  }
}

void Accept(GoContext &ctx) {
  AcceptSocket paccept(ctx.GetEpoll());
  ErrNo err = paccept.Open();
  if (err) {
    std::cout << strerror(err) << std::endl;
    return;
  }
  err = paccept.Bind("0.0.0.0", port);
  if (err) {
    std::cout << strerror(err) << std::endl;
    return;
  }
  err = paccept.Listen(1024);
  if (err) {
    std::cout << strerror(err) << std::endl;
    return;
  }
  int newsocket;
  while (true) {
    std::tie(newsocket, err) = paccept.Accept(&ctx);
    if (err != 0) {
      std::cout << strerror(err) << std::endl;
      continue;
    }
    ctx.GetEpoll()->Go(std::bind(NewConnect, std::placeholders::_1, newsocket));
  }
}

void timer(GoContext &ctx) {
  while (true) {
    ctx.Sleep(1);
  }
}

void client(GoContext &ctx) {
  TcpSocket tcps(ctx.GetEpoll());
  ErrNo err = tcps.Open();
  if (err) {
    std::cout << strerror(err) << std::endl;
    return;
  }
  err = tcps.Connect(&ctx, "127.0.0.1", port);
  if (err) {
    std::cout << strerror(err) << std::endl;
    return;
  }
  char readBuffer[1024];
  while (true) {
    tcps.Write(readBuffer, sizeof(readBuffer));
    size_t size = 0;
    while (size != sizeof(readBuffer)) {
      size_t nread = 0;
      ErrNo err;
      std::tie(nread, err) = tcps.Read(&ctx, readBuffer, sizeof(readBuffer));
      if (err) {
        std::cout << strerror(err) << std::endl;
        return;
      }
      if (nread == 0) {
        return;
      }
      size += nread;
    }
    ++count;
  }
}

void udpserver(GoContext &ctx) {
  UdpSocket s(ctx.GetEpoll());
  s.Open();
  s.Bind("192.168.181.128", 7777);
  uint8_t buffer[1024];
  sockaddr_in addr;
  while (true) {
    size_t recvBytes = 0;
    ErrNo err = 0;
    std::tie(recvBytes, err) = s.Recvfrom(&ctx, buffer, sizeof(buffer), addr);
    if (err) {
      std::cout << strerror(err) << std::endl;
    } else {
      // std::cout << "recvfrom:" << recvBytes << std::endl;
    }
  }
}

void udpclient(GoContext &ctx) {
  UdpSocket s(ctx.GetEpoll());
  s.Open();
  sockaddr_in dst;
  SockAddr(dst, "192.168.181.128", 7777);
  uint8_t buffer[512];
  while (true) {
    s.Sendto(buffer, sizeof(buffer), dst);
    ctx.Sleep(1);
  }
}

double sub(timespec *endTime, timespec *begTime) {
  return (endTime->tv_sec - begTime->tv_sec) * 1.0 +
         (endTime->tv_nsec - begTime->tv_nsec) / 1000000000.0;
}

void Stat(GoContext &ctx) {
  ctx.Sleep(2);
  uint64_t beg = count;
  timespec begTime;
  clock_gettime(CLOCK_REALTIME, &begTime);
  while (true) {
    ctx.Sleep(5);
    uint64_t end = count;
    timespec endTime;
    clock_gettime(CLOCK_REALTIME, &endTime);
    printf("%lf\n", double(end - beg) / sub(&endTime, &begTime));
    beg = end;
    begTime = endTime;
  }
}

void testConnect(GoContext &ctx) {
  TcpSocket connectSocket(ctx.GetEpoll());
  connectSocket.Open();
  auto err = connectSocket.ConnectWithTimeOut(&ctx, "14.215.177.38", 80, 4);
  if (err != 0) {
    printf("connect failed:%s\n", strerror(err));
  } else {
    printf("connect success\n");
  }
}

void server::Start(int num) {
  printf("num=%d\n", num);
  m_epoll.Create();
  m_goclient.Start(&m_epoll, "127.0.0.1", 80);
  while (true) {
    m_epoll.Wait(1000);
  }
}
