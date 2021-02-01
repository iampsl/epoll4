#include "server.h"

#include <cstring>
#include <iostream>

#include "wrapsocket.h"

const uint16_t port = 8888;

void NewConnect(GoContext &ctx, int s) {
  TcpSocket ptcp(ctx.GetEpoll());
  if (auto err = ptcp.Open(s)) {
    std::cout << strerror(err) << std::endl;
    return;
  }
  uint8_t pbuffer[1024];
  size_t nread = 0;
  ErrNo err = 0;
  while (true) {
    std::tie(nread, err) = ptcp.Read(&ctx, pbuffer, sizeof(pbuffer));
    if (err != 0) {
      std::cout << strerror(err) << std::endl;
      return;
    }
    ptcp.Write(pbuffer, nread);
  }
}

void Accept(GoContext &ctx) {
  std::shared_ptr<AcceptSocket> paccept =
      std::make_shared<AcceptSocket>(ctx.GetEpoll());
  ErrNo err = paccept->Open();
  if (err) {
    std::cout << strerror(err) << std::endl;
    return;
  }
  err = paccept->Bind("0.0.0.0", port);
  if (err) {
    std::cout << strerror(err) << std::endl;
    return;
  }
  err = paccept->Listen(1024);
  if (err) {
    std::cout << strerror(err) << std::endl;
    return;
  }
  int newsocket;
  while (true) {
    std::tie(newsocket, err) = paccept->Accept(&ctx);
    if (err == EINVAL) {
      std::cout << strerror(err) << std::endl;
      break;
    }
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
  const char *pstr = "hello world";
  char readBuffer[1024];
  while (true) {
    // ctx.Sleep(1);
    tcps.Write(pstr, strlen(pstr));
    size_t nread = 0;
    ErrNo err;
    std::tie(nread, err) = tcps.Read(&ctx, readBuffer, sizeof(readBuffer));
    if (err) {
      std::cout << strerror(err) << std::endl;
      break;
    }
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

void server::Start() {
  m_epoll.Create();
  m_epoll.Go(udpserver);
  for (int i = 0; i < 3000; i++) {
    m_epoll.Go(udpclient);
  }
  while (true) {
    m_epoll.Wait(1000);
  }
}
