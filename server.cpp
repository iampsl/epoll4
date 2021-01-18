#include "server.h"

#include <cstring>
#include <iostream>

#include "tcpsocket.h"

void NewConnect(GoContext& ctx, int s) {
  TcpSocket ptcp(ctx.GetEpoll());
  if (auto err = ptcp.Open(s)) {
    std::cout << strerror(err) << std::endl;
    return;
  }
  uint8_t pbuffer[1024];
  size_t nread = 0;
  ErrNo err = 0;
  std::tie(nread, err) = ptcp.Read(&ctx, pbuffer, sizeof(pbuffer));
  if (err != 0) {
    std::cout << strerror(err) << std::endl;
    return;
  }
  std::cout << nread << std::endl;
  ptcp.Write(pbuffer, nread);
}

void accept(GoContext& ctx) {
  std::shared_ptr<AcceptSocket> paccept =
      std::make_shared<AcceptSocket>(ctx.GetEpoll());
  paccept->Open();
  paccept->Bind("0.0.0.0", 35555);
  paccept->Listen(0);
  int newsocket;
  ErrNo err;
  while (true) {
    std::tie(newsocket, err) = paccept->Accept(&ctx);
    if (err == EINVAL) {
      break;
    }
    if (err != 0) {
      std::cout << strerror(err) << std::endl;
      continue;
    }
    ctx.GetEpoll()->Go(std::bind(NewConnect, std::placeholders::_1, newsocket));
  }
}

void timer(GoContext& ctx) {
  while (true) {
    ctx.Sleep(1);
  }
}

void server::Start() {
  m_epoll.Create();
  m_epoll.Go(accept);
  for (unsigned int i = 0; i < 200000; i++) {
    m_epoll.Go(timer);
  }
  while (true) {
    m_epoll.Wait(100);
  }
}
