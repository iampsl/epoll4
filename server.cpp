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
  uint8_t pbuffer[10240];
  while (true) {
    ErrNo err = 0;
    size_t nread = 0;
    std::tie(nread, err) = ptcp.Read(&ctx, pbuffer, sizeof(pbuffer));
    if (err != 0) {
      std::cout << strerror(err) << std::endl;
      return;
    }
    if (nread == 0) {
      return;
    }
    ptcp.Write(pbuffer, nread);
  }
}

void Accept(GoContext &ctx) {
  AcceptSocket paccept(ctx.GetEpoll());
  auto err = paccept.Listen("/root/epoll4/release/test.sock");
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
    std::cout << "accept a new connect" << std::endl;
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
  auto err = tcps.Connect(&ctx, "127.0.0.1", port);
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

void TestRpc(GoContext &ctx) {
  GoRPC goclient;
  goclient.Start(ctx.GetEpoll(), "/root/epoll4/release/test.sock");
  std::string username("iampsl");
  timespec begTime;
  clock_gettime(CLOCK_REALTIME, &begTime);
  for (unsigned int i = 0; i < 10000000; i++) {
    goclient.QueryUserInfo(&ctx, username);
  }
  timespec endTime;
  clock_gettime(CLOCK_REALTIME, &endTime);
  printf("time:%f\n", sub(&endTime, &begTime));
}

void server::Start(int num) {
  m_epoll.Create();
  m_epoll.Go(Accept);
  m_epoll.Go(TestRpc);
  while (true) {
    m_epoll.Wait(1000);
  }
}
