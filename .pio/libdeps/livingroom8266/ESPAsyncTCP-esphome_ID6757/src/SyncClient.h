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

#ifndef SYNCCLIENT_H_
#define SYNCCLIENT_H_

#include "Client.h"
// Needed for Arduino core releases prior to 2.5.0, because of changes
// made to accommodate Arduino core 2.5.0
// CONST was 1st defined in Core 2.5.0 in IPAddress.h
#ifndef CONST
#define CONST
#endif
#include <async_config.h>
class cbuf;
class AsyncClient;

class SyncClient: public Client {
  private:
    AsyncClient *_client;
    cbuf *_tx_buffer;
    size_t _tx_buffer_size;
    cbuf *_rx_buffer;
    int *_ref;

    size_t _sendBuffer();
    void _onData(void *data, size_t len);
    void _onConnect(AsyncClient *c);
    void _onDisconnect();
    void _attachCallbacks();
    void _attachCallbacks_Disconnect();
    void _attachCallbacks_AfterConnected();
    void _release();

  public:
    SyncClient(size_t txBufLen = TCP_MSS);
    SyncClient(AsyncClient *client, size_t txBufLen = TCP_MSS);
    virtual ~SyncClient();

    int ref();
    int unref();
    operator bool(){ return connected(); }
    SyncClient & operator=(const SyncClient &other);

#if ASYNC_TCP_SSL_ENABLED
    int _connect(const IPAddress& ip, uint16_t port, bool secure);
    int connect(CONST IPAddress& ip, uint16_t port, bool secure){
      return _connect(ip, port, secure);
    }
    int connect(IPAddress ip, uint16_t port, bool secure){
      return _connect(reinterpret_cast<const IPAddress&>(ip), port, secure);
    }
    int connect(const char *host, uint16_t port, bool secure);
    int connect(CONST IPAddress& ip, uint16_t port){
      return _connect(ip, port, false);
    }
    int connect(IPAddress ip, uint16_t port){
      return _connect(reinterpret_cast<const IPAddress&>(ip), port, false);
    }
    int connect(const char *host, uint16_t port){
      return connect(host, port, false);
    }
#else
    int _connect(const IPAddress& ip, uint16_t port);
    int connect(CONST IPAddress& ip, uint16_t port){
      return _connect(ip, port);
    }
    int connect(IPAddress ip, uint16_t port){
      return _connect(reinterpret_cast<const IPAddress&>(ip), port);
    }
    int connect(const char *host, uint16_t port);
#endif
    void setTimeout(uint32_t seconds);

    uint8_t status();
    uint8_t connected();

    bool stop(unsigned int maxWaitMs);
    bool flush(unsigned int maxWaitMs);
    void stop() { (void)stop(0);}
    void flush() { (void)flush(0);}
    size_t write(uint8_t data);
    size_t write(const uint8_t *data, size_t len);

    int available();
    int peek();
    int read();
    int read(uint8_t *data, size_t len);
};

#endif /* SYNCCLIENT_H_ */
