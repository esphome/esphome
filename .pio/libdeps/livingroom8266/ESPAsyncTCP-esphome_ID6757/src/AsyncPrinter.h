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

#ifndef ASYNCPRINTER_H_
#define ASYNCPRINTER_H_

#include "Arduino.h"
#include "ESPAsyncTCP.h"
#include "cbuf.h"

class AsyncPrinter;

typedef std::function<void(void*, AsyncPrinter*, uint8_t*, size_t)> ApDataHandler;
typedef std::function<void(void*, AsyncPrinter*)> ApCloseHandler;

class AsyncPrinter: public Print {
  private:
    AsyncClient *_client;
    ApDataHandler _data_cb;
    void *_data_arg;
    ApCloseHandler _close_cb;
    void *_close_arg;
    cbuf *_tx_buffer;
    size_t _tx_buffer_size;

    void _onConnect(AsyncClient *c);
  public:
    AsyncPrinter *next;

    AsyncPrinter();
    AsyncPrinter(AsyncClient *client, size_t txBufLen = TCP_MSS);
    virtual ~AsyncPrinter();

    int connect(IPAddress ip, uint16_t port);
    int connect(const char *host, uint16_t port);

    void onData(ApDataHandler cb, void *arg);
    void onClose(ApCloseHandler cb, void *arg);

    operator bool();
    AsyncPrinter & operator=(const AsyncPrinter &other);

    size_t write(uint8_t data);
    size_t write(const uint8_t *data, size_t len);

    bool connected();
    void close();

    size_t _sendBuffer();
    void _onData(void *data, size_t len);
    void _on_close();
    void _attachCallbacks();
};

#endif /* ASYNCPRINTER_H_ */
