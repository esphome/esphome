/*
  Asynchronous TCP library for Espressif MCUs

  Copyright (c) 2016 Hristo Gochkov. All rights reserved.
  This file is part of the esp8266 core for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
/*
 * Compatibility for AxTLS with LWIP raw tcp mode (http://lwip.wikia.com/wiki/Raw/TCP)
 * Original Code and Inspiration: Slavey Karadzhov
 */
#include <async_config.h>
#if ASYNC_TCP_SSL_ENABLED

#include "lwip/opt.h"
#include "lwip/tcp.h"
#include "lwip/inet.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <tcp_axtls.h>

uint8_t * default_private_key = NULL;
uint16_t default_private_key_len = 0;

uint8_t * default_certificate = NULL;
uint16_t default_certificate_len = 0;

static uint8_t _tcp_ssl_has_client = 0;

SSL_CTX * tcp_ssl_new_server_ctx(const char *cert, const char *private_key_file, const char *password){
  uint32_t options = SSL_CONNECT_IN_PARTS;
  SSL_CTX *ssl_ctx;

  if(private_key_file){
    options |= SSL_NO_DEFAULT_KEY;
  }

  if ((ssl_ctx = ssl_ctx_new(options, SSL_DEFAULT_SVR_SESS)) == NULL){
    TCP_SSL_DEBUG("tcp_ssl_new_server_ctx: failed to allocate context\n");
    return NULL;
  }

  if (private_key_file){
    int obj_type = SSL_OBJ_RSA_KEY;
    if (strstr(private_key_file, ".p8"))
      obj_type = SSL_OBJ_PKCS8;
    else if (strstr(private_key_file, ".p12"))
      obj_type = SSL_OBJ_PKCS12;

    if (ssl_obj_load(ssl_ctx, obj_type, private_key_file, password)){
      TCP_SSL_DEBUG("tcp_ssl_new_server_ctx: load private key '%s' failed\n", private_key_file);
      return NULL;
    }
  }

  if (cert){
    if (ssl_obj_load(ssl_ctx, SSL_OBJ_X509_CERT, cert, NULL)){
      TCP_SSL_DEBUG("tcp_ssl_new_server_ctx: load certificate '%s' failed\n", cert);
      return NULL;
    }
  }
  return ssl_ctx;
}

struct tcp_ssl_pcb {
  struct tcp_pcb *tcp;
  int fd;
  SSL_CTX* ssl_ctx;
  SSL *ssl;
  uint8_t type;
  int handshake;
  void * arg;
  tcp_ssl_data_cb_t on_data;
  tcp_ssl_handshake_cb_t on_handshake;
  tcp_ssl_error_cb_t on_error;
  int last_wr;
  struct pbuf *tcp_pbuf;
  int pbuf_offset;
  struct tcp_ssl_pcb * next;
};

typedef struct tcp_ssl_pcb tcp_ssl_t;

static tcp_ssl_t * tcp_ssl_array = NULL;
static int tcp_ssl_next_fd = 0;

uint8_t tcp_ssl_has_client(){
  return _tcp_ssl_has_client;
}

tcp_ssl_t * tcp_ssl_new(struct tcp_pcb *tcp) {

  if(tcp_ssl_next_fd < 0){
    tcp_ssl_next_fd = 0;//overflow
  }

  tcp_ssl_t * new_item = (tcp_ssl_t*)malloc(sizeof(tcp_ssl_t));
  if(!new_item){
    TCP_SSL_DEBUG("tcp_ssl_new: failed to allocate tcp_ssl\n");
    return NULL;
  }

  new_item->tcp = tcp;
  new_item->handshake = SSL_NOT_OK;
  new_item->arg = NULL;
  new_item->on_data = NULL;
  new_item->on_handshake = NULL;
  new_item->on_error = NULL;
  new_item->tcp_pbuf = NULL;
  new_item->pbuf_offset = 0;
  new_item->next = NULL;
  new_item->ssl_ctx = NULL;
  new_item->ssl = NULL;
  new_item->type = TCP_SSL_TYPE_CLIENT;
  new_item->fd = tcp_ssl_next_fd++;

  if(tcp_ssl_array == NULL){
    tcp_ssl_array = new_item;
  } else {
    tcp_ssl_t * item = tcp_ssl_array;
    while(item->next != NULL)
      item = item->next;
    item->next = new_item;
  }

  TCP_SSL_DEBUG("tcp_ssl_new: %d\n", new_item->fd);
  return new_item;
}

tcp_ssl_t* tcp_ssl_get(struct tcp_pcb *tcp) {
  if(tcp == NULL) {
    return NULL;
  }
  tcp_ssl_t * item = tcp_ssl_array;
  while(item && item->tcp != tcp){
    item = item->next;
  }
  return item;
}

int tcp_ssl_new_client(struct tcp_pcb *tcp){
  SSL_CTX* ssl_ctx;
  tcp_ssl_t * tcp_ssl;

  if(tcp == NULL) {
    return -1;
  }

  if(tcp_ssl_get(tcp) != NULL){
    TCP_SSL_DEBUG("tcp_ssl_new_client: tcp_ssl already exists\n");
    return -1;
  }

  ssl_ctx = ssl_ctx_new(SSL_CONNECT_IN_PARTS | SSL_SERVER_VERIFY_LATER, 1);
  if(ssl_ctx == NULL){
    TCP_SSL_DEBUG("tcp_ssl_new_client: failed to allocate ssl context\n");
    return -1;
  }

  tcp_ssl = tcp_ssl_new(tcp);
  if(tcp_ssl == NULL){
    ssl_ctx_free(ssl_ctx);
    return -1;
  }

  tcp_ssl->ssl_ctx = ssl_ctx;

  tcp_ssl->ssl = ssl_client_new(ssl_ctx, tcp_ssl->fd, NULL, 0, NULL);
  if(tcp_ssl->ssl == NULL){
    TCP_SSL_DEBUG("tcp_ssl_new_client: failed to allocate ssl\n");
    tcp_ssl_free(tcp);
    return -1;
  }

  return tcp_ssl->fd;
}

int tcp_ssl_new_server(struct tcp_pcb *tcp, SSL_CTX* ssl_ctx){
  tcp_ssl_t * tcp_ssl;

  if(tcp == NULL) {
    return -1;
  }

  if(ssl_ctx == NULL){
    return -1;
  }

  if(tcp_ssl_get(tcp) != NULL){
    TCP_SSL_DEBUG("tcp_ssl_new_server: tcp_ssl already exists\n");
    return -1;
  }

  tcp_ssl = tcp_ssl_new(tcp);
  if(tcp_ssl == NULL){
    return -1;
  }

  tcp_ssl->type = TCP_SSL_TYPE_SERVER;
  tcp_ssl->ssl_ctx = ssl_ctx;

  _tcp_ssl_has_client = 1;
  tcp_ssl->ssl = ssl_server_new(ssl_ctx, tcp_ssl->fd);
  if(tcp_ssl->ssl == NULL){
    TCP_SSL_DEBUG("tcp_ssl_new_server: failed to allocate ssl\n");
    tcp_ssl_free(tcp);
    return -1;
  }

  return tcp_ssl->fd;
}

int tcp_ssl_free(struct tcp_pcb *tcp) {

  if(tcp == NULL) {
    return -1;
  }

  tcp_ssl_t * item = tcp_ssl_array;

  if(item->tcp == tcp){
    tcp_ssl_array = tcp_ssl_array->next;
    if(item->tcp_pbuf != NULL){
      pbuf_free(item->tcp_pbuf);
    }
    TCP_SSL_DEBUG("tcp_ssl_free: %d\n", item->fd);
    if(item->ssl)
      ssl_free(item->ssl);
    if(item->type == TCP_SSL_TYPE_CLIENT && item->ssl_ctx)
      ssl_ctx_free(item->ssl_ctx);
    if(item->type == TCP_SSL_TYPE_SERVER)
      _tcp_ssl_has_client = 0;
    free(item);
    return 0;
  }

  while(item->next && item->next->tcp != tcp)
    item = item->next;

  if(item->next == NULL){
    return ERR_TCP_SSL_INVALID_CLIENTFD_DATA;//item not found
  }

  tcp_ssl_t * i = item->next;
  item->next = i->next;
  if(i->tcp_pbuf != NULL){
    pbuf_free(i->tcp_pbuf);
  }
  TCP_SSL_DEBUG("tcp_ssl_free: %d\n", i->fd);
  if(i->ssl)
    ssl_free(i->ssl);
  if(i->type == TCP_SSL_TYPE_CLIENT && i->ssl_ctx)
    ssl_ctx_free(i->ssl_ctx);
  if(i->type == TCP_SSL_TYPE_SERVER)
    _tcp_ssl_has_client = 0;
  free(i);
  return 0;
}

#ifdef AXTLS_2_0_0_SNDBUF
int tcp_ssl_sndbuf(struct tcp_pcb *tcp){
  int expected;
  int available;
  int result = -1;

  if(tcp == NULL) {
    return result;
  }
  tcp_ssl_t * tcp_ssl = tcp_ssl_get(tcp);
  if(!tcp_ssl){
    TCP_SSL_DEBUG("tcp_ssl_sndbuf: tcp_ssl is NULL\n");
    return result;
  }
  available = tcp_sndbuf(tcp);
  if(!available){
    TCP_SSL_DEBUG("tcp_ssl_sndbuf: tcp_sndbuf is zero\n");
    return 0;
  }
  result = available;
  while((expected = ssl_calculate_write_length(tcp_ssl->ssl, result)) > available){
    result -= (expected - available) + 4;
  }

  if(expected > 0){
    //TCP_SSL_DEBUG("tcp_ssl_sndbuf: tcp_sndbuf is %d from %d\n", result, available);
    return result;
  }

  return 0;
}
#endif

int tcp_ssl_write(struct tcp_pcb *tcp, uint8_t *data, size_t len) {
  if(tcp == NULL) {
    return -1;
  }
  tcp_ssl_t * tcp_ssl = tcp_ssl_get(tcp);
  if(!tcp_ssl){
    TCP_SSL_DEBUG("tcp_ssl_write: tcp_ssl is NULL\n");
    return 0;
  }
  tcp_ssl->last_wr = 0;

#ifdef AXTLS_2_0_0_SNDBUF
  int expected_len = ssl_calculate_write_length(tcp_ssl->ssl, len);
  int available_len = tcp_sndbuf(tcp);
  if(expected_len < 0 || expected_len > available_len){
    TCP_SSL_DEBUG("tcp_ssl_write: data will not fit! %u < %d(%u)\r\n", available_len, expected_len, len);
    return -1;
  }
#endif

  int rc = ssl_write(tcp_ssl->ssl, data, len);

  //TCP_SSL_DEBUG("tcp_ssl_write: %u -> %d (%d)\r\n", len, tcp_ssl->last_wr, rc);

  if (rc < 0){
    if(rc != SSL_CLOSE_NOTIFY) {
      TCP_SSL_DEBUG("tcp_ssl_write error: %d\r\n", rc);
    }
    return rc;
  }

  return tcp_ssl->last_wr;
}

/**
 * Reads data from the SSL over TCP stream. Returns decrypted data.
 * @param tcp_pcb *tcp - pointer to the raw tcp object
 * @param pbuf *p - pointer to the buffer with the TCP packet data
 *
 * @return int
 *      0 - when everything is fine but there are no symbols to process yet
 *      < 0 - when there is an error
 *      > 0 - the length of the clear text characters that were read
 */
int tcp_ssl_read(struct tcp_pcb *tcp, struct pbuf *p) {
  if(tcp == NULL) {
    return -1;
  }
  tcp_ssl_t* fd_data = NULL;

  int read_bytes = 0;
  int total_bytes = 0;
  uint8_t *read_buf;

  fd_data = tcp_ssl_get(tcp);
  if(fd_data == NULL) {
    TCP_SSL_DEBUG("tcp_ssl_read: tcp_ssl is NULL\n");
    return ERR_TCP_SSL_INVALID_CLIENTFD_DATA;
  }

  if(p == NULL) {
    TCP_SSL_DEBUG("tcp_ssl_read:p == NULL\n");
    return ERR_TCP_SSL_INVALID_DATA;
  }

  //TCP_SSL_DEBUG("READY TO READ SOME DATA\n");

  fd_data->tcp_pbuf = p;
  fd_data->pbuf_offset = 0;

  do {
    read_bytes = ssl_read(fd_data->ssl, &read_buf);
    //TCP_SSL_DEBUG("tcp_ssl_ssl_read: %d\n", read_bytes);
    if(read_bytes < SSL_OK) {
      if(read_bytes != SSL_CLOSE_NOTIFY) {
        TCP_SSL_DEBUG("tcp_ssl_read: read error: %d\n", read_bytes);
      }
      total_bytes = read_bytes;
      break;
    } else if(read_bytes > 0){
      if(fd_data->on_data){
        fd_data->on_data(fd_data->arg, tcp, read_buf, read_bytes);
      }
      total_bytes+= read_bytes;
    } else {
      if(fd_data->handshake != SSL_OK) {
        fd_data->handshake = ssl_handshake_status(fd_data->ssl);
        if(fd_data->handshake == SSL_OK){
          //TCP_SSL_DEBUG("tcp_ssl_read: handshake OK\n");
          if(fd_data->on_handshake)
            fd_data->on_handshake(fd_data->arg, fd_data->tcp, fd_data->ssl);
        } else if(fd_data->handshake != SSL_NOT_OK){
          TCP_SSL_DEBUG("tcp_ssl_read: handshake error: %d\n", fd_data->handshake);
          if(fd_data->on_error)
            fd_data->on_error(fd_data->arg, fd_data->tcp, fd_data->handshake);
          return fd_data->handshake;
        }
      }
    }
  } while (p->tot_len - fd_data->pbuf_offset > 0);

  tcp_recved(tcp, p->tot_len);
  fd_data->tcp_pbuf = NULL;
  pbuf_free(p);

  return total_bytes;
}

SSL * tcp_ssl_get_ssl(struct tcp_pcb *tcp){
  tcp_ssl_t * tcp_ssl = tcp_ssl_get(tcp);
  if(tcp_ssl){
    return tcp_ssl->ssl;
  }
  return NULL;
}

bool tcp_ssl_has(struct tcp_pcb *tcp){
  return tcp_ssl_get(tcp) != NULL;
}

int tcp_ssl_is_server(struct tcp_pcb *tcp){
  tcp_ssl_t * tcp_ssl = tcp_ssl_get(tcp);
  if(tcp_ssl){
    return tcp_ssl->type;
  }
  return -1;
}

void tcp_ssl_arg(struct tcp_pcb *tcp, void * arg){
  tcp_ssl_t * item = tcp_ssl_get(tcp);
  if(item) {
    item->arg = arg;
  }
}

void tcp_ssl_data(struct tcp_pcb *tcp, tcp_ssl_data_cb_t arg){
  tcp_ssl_t * item = tcp_ssl_get(tcp);
  if(item) {
    item->on_data = arg;
  }
}

void tcp_ssl_handshake(struct tcp_pcb *tcp, tcp_ssl_handshake_cb_t arg){
  tcp_ssl_t * item = tcp_ssl_get(tcp);
  if(item) {
    item->on_handshake = arg;
  }
}

void tcp_ssl_err(struct tcp_pcb *tcp, tcp_ssl_error_cb_t arg){
  tcp_ssl_t * item = tcp_ssl_get(tcp);
  if(item) {
    item->on_error = arg;
  }
}

static tcp_ssl_file_cb_t _tcp_ssl_file_cb = NULL;
static void * _tcp_ssl_file_arg = NULL;

void tcp_ssl_file(tcp_ssl_file_cb_t cb, void * arg){
  _tcp_ssl_file_cb = cb;
  _tcp_ssl_file_arg = arg;
}

int ax_get_file(const char *filename, uint8_t **buf) {
    //TCP_SSL_DEBUG("ax_get_file: %s\n", filename);
    if(_tcp_ssl_file_cb){
      return _tcp_ssl_file_cb(_tcp_ssl_file_arg, filename, buf);
    }
    *buf = 0;
    return 0;
}

tcp_ssl_t* tcp_ssl_get_by_fd(int fd) {
  tcp_ssl_t * item = tcp_ssl_array;
  while(item && item->fd != fd){
    item = item->next;
  }
  return item;
}
/*
 * The LWIP tcp raw version of the SOCKET_WRITE(A, B, C)
 */
int ax_port_write(int fd, uint8_t *data, uint16_t len) {
  tcp_ssl_t *fd_data = NULL;
  int tcp_len = 0;
  err_t err = ERR_OK;

  //TCP_SSL_DEBUG("ax_port_write: %d, %d\n", fd, len);

  fd_data = tcp_ssl_get_by_fd(fd);
  if(fd_data == NULL) {
    //TCP_SSL_DEBUG("ax_port_write: tcp_ssl[%d] is NULL\n", fd);
    return ERR_MEM;
  }

  if (data == NULL || len == 0) {
    return 0;
  }

  if (tcp_sndbuf(fd_data->tcp) < len) {
    tcp_len = tcp_sndbuf(fd_data->tcp);
    if(tcp_len == 0) {
      TCP_SSL_DEBUG("ax_port_write: tcp_sndbuf is zero: %d\n", len);
      return ERR_MEM;
    }
  } else {
    tcp_len = len;
  }

  if (tcp_len > 2 * fd_data->tcp->mss) {
    tcp_len = 2 * fd_data->tcp->mss;
  }

  err = tcp_write(fd_data->tcp, data, tcp_len, TCP_WRITE_FLAG_COPY);
  if(err < ERR_OK) {
    if (err == ERR_MEM) {
      TCP_SSL_DEBUG("ax_port_write: No memory %d (%d)\n", tcp_len, len);
      return err;
    }
    TCP_SSL_DEBUG("ax_port_write: tcp_write error: %d\n", err);
    return err;
  } else if (err == ERR_OK) {
    //TCP_SSL_DEBUG("ax_port_write: tcp_output: %d / %d\n", tcp_len, len);
    err = tcp_output(fd_data->tcp);
    if(err != ERR_OK) {
      TCP_SSL_DEBUG("ax_port_write: tcp_output err: %d\n", err);
      return err;
    }
  }

  fd_data->last_wr += tcp_len;

  return tcp_len;
}

/*
 * The LWIP tcp raw version of the SOCKET_READ(A, B, C)
 */
int ax_port_read(int fd, uint8_t *data, int len) {
  tcp_ssl_t *fd_data = NULL;
  uint8_t *read_buf = NULL;
  uint8_t *pread_buf = NULL;
  u16_t recv_len = 0;

  //TCP_SSL_DEBUG("ax_port_read: %d, %d\n", fd, len);

  fd_data = tcp_ssl_get_by_fd(fd);
  if (fd_data == NULL) {
    TCP_SSL_DEBUG("ax_port_read: tcp_ssl[%d] is NULL\n", fd);
    return ERR_TCP_SSL_INVALID_CLIENTFD_DATA;
  }

  if(fd_data->tcp_pbuf == NULL || fd_data->tcp_pbuf->tot_len == 0) {
    return 0;
  }

  read_buf =(uint8_t*)calloc(fd_data->tcp_pbuf->len + 1, sizeof(uint8_t));
  pread_buf = read_buf;
  if (pread_buf != NULL){
    recv_len = pbuf_copy_partial(fd_data->tcp_pbuf, read_buf, len, fd_data->pbuf_offset);
    fd_data->pbuf_offset += recv_len;
  }

  if (recv_len != 0) {
    memcpy(data, read_buf, recv_len);
  }

  if(len < recv_len) {
    TCP_SSL_DEBUG("ax_port_read: got %d bytes more than expected\n", recv_len - len);
  }

  free(pread_buf);
  pread_buf = NULL;

  return recv_len;
}

void ax_wdt_feed() {}

#endif
