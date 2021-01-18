#include "server.h"

#include <cstring>
#include <iostream>

#include "tcpsocket.h"

void NewConnect(GoContext& ctx, int s) {
  std::shared_ptr<TcpSocket> ptcp = std::make_shared<TcpSocket>(ctx.GetEpoll());
  if (auto err = ptcp->Open(s)) {
    std::cout << strerror(err) << std::endl;
    return;
  }
  uint8_t pbuffer[1024];
  size_t nread = 0;
  ErrNo err = 0;
  std::tie(nread, err) = ptcp->Read(&ctx, pbuffer, sizeof(pbuffer));
  if (err != 0) {
    ptcp->Close();
    std::cout << strerror(err) << std::endl;
    return;
  }
  std::cout << nread << std::endl;
  ptcp->Write(pbuffer, nread);
  ptcp->Close();
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

void server::Start() {
  m_epoll.Create();
  m_epoll.Go(accept);
  while (true) {
    m_epoll.Wait(100);
  }
}
