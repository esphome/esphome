#include "esphome/components/socket/socket.h"
#include "esphome/components/socket_shd/socket.h"

// NB!!!
// A (VERY!!) simple tcp server!
// connections will be closed and removed (I think) 10s after last received message
// ... if test_tcp_read() is being called often

struct tcp_client {
  std::unique_ptr<socket_shd::Socket> sock;
  uint32_t msec;
};

std::unique_ptr<socket_shd::Socket> sock_tcp;
struct sockaddr_storage srv_tcp;
uint8_t buff_tcp_recv[1024];
std::vector<std::unique_ptr<struct tcp_client>> tcp_clients;

void test_tcp_read() {
  if(!sock_tcp) return;

  // Partition clients into remove and active
  auto new_end = std::partition(
    tcp_clients.begin(), tcp_clients.end(),
    [](const std::unique_ptr<struct tcp_client> &conn) {
      bool close = (millis() - conn->msec) < 10000;
      return close;
    }
  );
  // print disconnection messages
  for (auto it = new_end; it != tcp_clients.end(); ++it) {
    ESP_LOGD("tcp_serv", "Removing connection to %s", (*it)->sock->getpeername().c_str());
  }
  // resize vector
  tcp_clients.erase(new_end, tcp_clients.end());

  for (auto &client : tcp_clients) {
    std::string addr;
    ssize_t len;

    len = client->sock->recvfrom(buff_tcp_recv, 3, MSG_PEEK, addr);
    if (len <= 0) continue;

    client->msec = millis();
    ESP_LOGD("tcp_recv", "From: %s", addr.c_str());
    ESP_LOGD("tcp_recv", "Data: %s", std::string((const char *) buff_tcp_recv, len).c_str());

    len = client->sock->read(buff_tcp_recv, sizeof(buff_tcp_recv));
    if (len <= 0) continue;

    ESP_LOGD("tcp_recv", "Data: %s", std::string((const char *) buff_tcp_recv, len).c_str());
  }
}

void test_tcp_accept() {
  if(!sock_tcp) return;

  while (true) {
    struct sockaddr_storage source_addr;
    socklen_t addr_len = sizeof(source_addr);
    auto sock = sock_tcp->accept((struct sockaddr *) &source_addr, &addr_len);
    if (!sock) break;
    sock->setblocking(false);
    ESP_LOGD("tcp_serv", "Accepted %s", sock->getpeername().c_str());

    auto *client = new tcp_client {std::move(sock), millis()};
    tcp_clients.emplace_back(client);

    std::string str = "hello client";
    client->sock->write(str.data(), str.size());
  }
}

void test_tcp_setup() {
  if(sock_tcp) return;

  socket::set_sockaddr_any((struct sockaddr *) &srv_tcp, sizeof(srv_tcp), 25588);
  
  int enable = 1;
  sock_tcp = socket_shd::socket_ip(SOCK_STREAM, IPPROTO_IP);
  sock_tcp->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  sock_tcp->setblocking(false);

  sock_tcp->bind((struct sockaddr *) &srv_tcp, sizeof(srv_tcp));
  sock_tcp->listen(4);
}
