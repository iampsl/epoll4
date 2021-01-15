#include "server.h"

#include <string.h>

#include <iostream>

#include "tcpsocket.h"

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
    std::cout << "accept a new socket" << std::endl;
    close(newsocket);
  }
}

void server::Start() {
  m_epoll.Create();
  m_epoll.Go(accept);
  while (true) {
    m_epoll.Wait(100);
  }
}
