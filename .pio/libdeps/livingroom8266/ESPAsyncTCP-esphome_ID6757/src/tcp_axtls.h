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

#ifndef LWIPR_COMPAT_H
#define LWIPR_COMPAT_H

#include <async_config.h>

#if ASYNC_TCP_SSL_ENABLED

#include "lwipopts.h"
/*
 * All those functions will run only if LWIP tcp raw mode is used
 */
#if LWIP_RAW==1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "include/ssl.h"

#define ERR_TCP_SSL_INVALID_SSL           -101
#define ERR_TCP_SSL_INVALID_TCP           -102
#define ERR_TCP_SSL_INVALID_CLIENTFD      -103
#define ERR_TCP_SSL_INVALID_CLIENTFD_DATA -104
#define ERR_TCP_SSL_INVALID_DATA          -105

#define TCP_SSL_TYPE_CLIENT 0
#define TCP_SSL_TYPE_SERVER 1

#define tcp_ssl_ssl_write(A, B, C) tcp_ssl_write(A, B, C)
#define tcp_ssl_ssl_read(A, B) tcp_ssl_read(A, B)

typedef void (* tcp_ssl_data_cb_t)(void *arg, struct tcp_pcb *tcp, uint8_t * data, size_t len);
typedef void (* tcp_ssl_handshake_cb_t)(void *arg, struct tcp_pcb *tcp, SSL *ssl);
typedef void (* tcp_ssl_error_cb_t)(void *arg, struct tcp_pcb *tcp, int8_t error);
typedef int (* tcp_ssl_file_cb_t)(void *arg, const char *filename, uint8_t **buf);

uint8_t tcp_ssl_has_client();

int tcp_ssl_new_client(struct tcp_pcb *tcp);

SSL_CTX * tcp_ssl_new_server_ctx(const char *cert, const char *private_key_file, const char *password);
int tcp_ssl_new_server(struct tcp_pcb *tcp, SSL_CTX* ssl_ctx);
int tcp_ssl_is_server(struct tcp_pcb *tcp);

int tcp_ssl_free(struct tcp_pcb *tcp);
int tcp_ssl_read(struct tcp_pcb *tcp, struct pbuf *p);

#ifdef AXTLS_2_0_0_SNDBUF
int tcp_ssl_sndbuf(struct tcp_pcb *tcp);
#endif

int tcp_ssl_write(struct tcp_pcb *tcp, uint8_t *data, size_t len);

void tcp_ssl_file(tcp_ssl_file_cb_t cb, void * arg);

void tcp_ssl_arg(struct tcp_pcb *tcp, void * arg);
void tcp_ssl_data(struct tcp_pcb *tcp, tcp_ssl_data_cb_t arg);
void tcp_ssl_handshake(struct tcp_pcb *tcp, tcp_ssl_handshake_cb_t arg);
void tcp_ssl_err(struct tcp_pcb *tcp, tcp_ssl_error_cb_t arg);

SSL * tcp_ssl_get_ssl(struct tcp_pcb *tcp);
bool tcp_ssl_has(struct tcp_pcb *tcp);

#ifdef __cplusplus
}
#endif

#endif /* LWIP_RAW==1 */

#endif /* ASYNC_TCP_SSL_ENABLED */

#endif /* LWIPR_COMPAT_H */
