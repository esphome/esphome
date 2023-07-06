#include "esphome/components/socket/socket.h"
#include "esphome/components/socket_shd/socket.h"

std::unique_ptr<socket_shd::Socket> sock_udp;
struct sockaddr dest_bcast;
struct sockaddr_storage srv_udp;
uint8_t buff_udb_recv[1024];

void test_udp_read() {
  if(!sock_udp) return;
  
  std::string addr;
  ssize_t len = sock_udp->recvfrom(buff_udb_recv, sizeof(buff_udb_recv), 0, addr);
  if (len <= 0) return;

  ESP_LOGD("udp_recv", "From: %s", addr.c_str());
  ESP_LOGD("udp_recv", "Data: %s", std::string((const char *) buff_udb_recv, len).c_str());
}

void test_udp_send(const void *data, size_t len) {
	if (!sock_udp) return;
	sock_udp->sendto(data, len, 0, &dest_bcast, sizeof(dest_bcast));
}

void test_udp_setup() {
  socket::set_sockaddr(&dest_bcast, sizeof(dest_bcast), "255.255.255.255", 25577);
  socket::set_sockaddr_any((struct sockaddr *) &srv_udp, sizeof(srv_udp), 25577);
  
  int enable = 1;
  sock_udp = socket_shd::socket_ip(SOCK_DGRAM, IPPROTO_IP);
  sock_udp->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  sock_udp->setsockopt(SOL_SOCKET, SO_BROADCAST, &enable, sizeof(int));
  sock_udp->setblocking(false);
  
  sock_udp->bind((struct sockaddr *) &srv_udp, sizeof(srv_udp));
}
