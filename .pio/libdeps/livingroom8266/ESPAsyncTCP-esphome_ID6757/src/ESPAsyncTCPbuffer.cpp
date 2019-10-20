/**
 * @file  ESPAsyncTCPbuffer.cpp
 * @date  22.01.2016
 * @author Markus Sattler
 *
 * Copyright (c) 2015 Markus Sattler. All rights reserved.
 * This file is part of the Asynv TCP for ESP.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include <Arduino.h>
#include <debug.h>

#include "ESPAsyncTCPbuffer.h"


AsyncTCPbuffer::AsyncTCPbuffer(AsyncClient* client) {
    if(client == NULL) {
        DEBUG_ASYNC_TCP("[A-TCP] client is null!!!\n");
        panic();
    }

    _client = client;
    _TXbufferWrite = new (std::nothrow) cbuf(TCP_MSS);
    _TXbufferRead = _TXbufferWrite;
    _RXbuffer = new (std::nothrow) cbuf(100);
    _RXmode = ATB_RX_MODE_FREE;
    _rxSize = 0;
    _rxTerminator = 0x00;
    _rxReadBytesPtr = NULL;
    _rxReadStringPtr = NULL;
    _cbDisconnect = NULL;

    _cbRX = NULL;
    _cbDone = NULL;
    _attachCallbacks();
}

AsyncTCPbuffer::~AsyncTCPbuffer() {
    if(_client) {
        _client->close();
    }

    if(_RXbuffer) {
        delete _RXbuffer;
        _RXbuffer = NULL;
    }

    if(_TXbufferWrite) {
        // will be deleted in _TXbufferRead chain
        _TXbufferWrite = NULL;
    }

    if(_TXbufferRead) {
        cbuf * next = _TXbufferRead->next;
        delete _TXbufferRead;
        while(next != NULL) {
            _TXbufferRead = next;
            next = _TXbufferRead->next;
            delete _TXbufferRead;
        }
        _TXbufferRead = NULL;
    }
}

size_t AsyncTCPbuffer::write(String & data) {
    return write(data.c_str(), data.length());
}

size_t AsyncTCPbuffer::write(uint8_t data) {
    return write(&data, 1);
}

size_t AsyncTCPbuffer::write(const char* data) {
    return write((const uint8_t *) data, strlen(data));
}

size_t AsyncTCPbuffer::write(const char *data, size_t len) {
    return write((const uint8_t *) data, len);
}

/**
 * write data in to buffer and try to send the data
 * @param data
 * @param len
 * @return
 */
size_t AsyncTCPbuffer::write(const uint8_t *data, size_t len) {
    if(_TXbufferWrite == NULL || _client == NULL || !_client->connected() || data == NULL || len == 0) {
        return 0;
    }

    size_t bytesLeft = len;
    while(bytesLeft) {
        size_t w = _TXbufferWrite->write((const char*) data, bytesLeft);
        bytesLeft -= w;
        data += w;
        _sendBuffer();

        // add new buffer since we have more data
        if(_TXbufferWrite->full() && bytesLeft > 0) {

            // to less ram!!!
            if(ESP.getFreeHeap() < 4096) {
                DEBUG_ASYNC_TCP("[A-TCP] run out of Heap can not send all Data!\n");
                return (len - bytesLeft);
            }

            cbuf * next = new (std::nothrow) cbuf(TCP_MSS);
            if(next == NULL) {
                DEBUG_ASYNC_TCP("[A-TCP] run out of Heap!\n");
                panic();
            } else {
                DEBUG_ASYNC_TCP("[A-TCP] new cbuf\n");
            }

            // add new buffer to chain (current cbuf)
            _TXbufferWrite->next = next;

            // move ptr for next data
            _TXbufferWrite = next;
        }
    }

    return len;

}

/**
 * wait until all data has send out
 */
void AsyncTCPbuffer::flush() {
    while(!_TXbufferWrite->empty()) {
        while(connected() && !_client->canSend()) {
          delay(0);
        }
        if(!connected())
          return;
        _sendBuffer();
    }
}

void AsyncTCPbuffer::noCallback() {
    _RXmode = ATB_RX_MODE_NONE;
}

void AsyncTCPbuffer::readStringUntil(char terminator, String * str, AsyncTCPbufferDoneCb done) {
    if(_client == NULL) {
        return;
    }
    DEBUG_ASYNC_TCP("[A-TCP] readStringUntil terminator: %02X\n", terminator);
    _RXmode = ATB_RX_MODE_NONE;
    _cbDone = done;
    _rxReadStringPtr = str;
    _rxTerminator = terminator;
    _rxSize = 0;
    _RXmode = ATB_RX_MODE_TERMINATOR_STRING;
}

/*
 void AsyncTCPbuffer::readBytesUntil(char terminator, char *buffer, size_t length, AsyncTCPbufferDoneCb done) {
 _RXmode = ATB_RX_MODE_NONE;
 _cbDone = done;
 _rxReadBytesPtr = (uint8_t *) buffer;
 _rxTerminator = terminator;
 _rxSize = length;
 _RXmode = ATB_RX_MODE_TERMINATOR;
 _handleRxBuffer(NULL, 0);
 }

 void AsyncTCPbuffer::readBytesUntil(char terminator, uint8_t *buffer, size_t length, AsyncTCPbufferDoneCb done) {
 readBytesUntil(terminator, (char *) buffer, length, done);
 }
 */

void AsyncTCPbuffer::readBytes(char *buffer, size_t length, AsyncTCPbufferDoneCb done) {
    if(_client == NULL) {
        return;
    }
    DEBUG_ASYNC_TCP("[A-TCP] readBytes length: %d\n", length);
    _RXmode = ATB_RX_MODE_NONE;
    _cbDone = done;
    _rxReadBytesPtr = (uint8_t *) buffer;
    _rxSize = length;
    _RXmode = ATB_RX_MODE_READ_BYTES;
}

void AsyncTCPbuffer::readBytes(uint8_t *buffer, size_t length, AsyncTCPbufferDoneCb done) {
    readBytes((char *) buffer, length, done);
}

void AsyncTCPbuffer::onData(AsyncTCPbufferDataCb cb) {
    if(_client == NULL) {
        return;
    }
    DEBUG_ASYNC_TCP("[A-TCP] onData\n");
    _RXmode = ATB_RX_MODE_NONE;
    _cbDone = NULL;
    _cbRX = cb;
    _RXmode = ATB_RX_MODE_FREE;
}

void AsyncTCPbuffer::onDisconnect(AsyncTCPbufferDisconnectCb cb) {
    _cbDisconnect = cb;
}

IPAddress AsyncTCPbuffer::remoteIP() {
    if(!_client) {
        return IPAddress(0U);
    }
    return _client->remoteIP();
}

uint16_t AsyncTCPbuffer::remotePort() {
    if(!_client) {
        return 0;
    }
    return _client->remotePort();
}

bool AsyncTCPbuffer::connected() {
    if(!_client) {
        return false;
    }
    return _client->connected();
}

void AsyncTCPbuffer::stop() {

    if(!_client) {
        return;
    }
    _client->stop();
    _client = NULL;

    if(_cbDone) {
        switch(_RXmode) {
            case ATB_RX_MODE_READ_BYTES:
            case ATB_RX_MODE_TERMINATOR:
            case ATB_RX_MODE_TERMINATOR_STRING:
                _RXmode = ATB_RX_MODE_NONE;
                _cbDone(false, NULL);
                break;
            default:
              break;
        }
    }
    _RXmode = ATB_RX_MODE_NONE;
}

void AsyncTCPbuffer::close() {
    stop();
}


///--------------------------------

/**
 * attachCallbacks to AsyncClient class
 */
void AsyncTCPbuffer::_attachCallbacks() {
    if(!_client) {
        return;
    }
    DEBUG_ASYNC_TCP("[A-TCP] attachCallbacks\n");

    _client->onPoll([](void *obj, AsyncClient* c) {
        (void)c;
        AsyncTCPbuffer* b = ((AsyncTCPbuffer*)(obj));
        if((b->_TXbufferRead != NULL) && !b->_TXbufferRead->empty()) {
            b->_sendBuffer();
        }
        //    if(!b->_RXbuffer->empty()) {
        //       b->_handleRxBuffer(NULL, 0);
        //   }
    }, this);

    _client->onAck([](void *obj, AsyncClient* c, size_t len, uint32_t time) {
        (void)c;
        (void)len;
        (void)time;
        DEBUG_ASYNC_TCP("[A-TCP] onAck\n");
        ((AsyncTCPbuffer*)(obj))->_sendBuffer();
    }, this);

    _client->onDisconnect([](void *obj, AsyncClient* c) {
        DEBUG_ASYNC_TCP("[A-TCP] onDisconnect\n");
        AsyncTCPbuffer* b = ((AsyncTCPbuffer*)(obj));
        b->_client = NULL;
        bool del = true;
        if(b->_cbDisconnect) {
            del = b->_cbDisconnect(b);
        }
        delete c;
        if(del) {
            delete b;
        }
    }, this);

    _client->onData([](void *obj, AsyncClient* c, void *buf, size_t len) {
        (void)c;
        AsyncTCPbuffer* b = ((AsyncTCPbuffer*)(obj));
        b->_rxData((uint8_t *)buf, len);
    }, this);

    _client->onTimeout([](void *obj, AsyncClient* c, uint32_t time){
        (void)obj;
        (void)time;
        DEBUG_ASYNC_TCP("[A-TCP] onTimeout\n");
        c->close();
    }, this);

    DEBUG_ASYNC_TCP("[A-TCP] attachCallbacks Done.\n");
}

/**
 * send TX buffer if possible
 */
void AsyncTCPbuffer::_sendBuffer() {
    //DEBUG_ASYNC_TCP("[A-TCP] _sendBuffer...\n");
    size_t available = _TXbufferRead->available();
    if(available == 0 || _client == NULL || !_client->connected() || !_client->canSend()) {
        return;
    }

    while(connected() && (_client->space() > 0) && (_TXbufferRead->available() > 0) && _client->canSend()) {

        available = _TXbufferRead->available();

        if(available > _client->space()) {
            available = _client->space();
        }

        char *out = new (std::nothrow) char[available];
        if(out == NULL) {
            DEBUG_ASYNC_TCP("[A-TCP] to less heap, try later.\n");
            return;
        }

        // read data from buffer
        _TXbufferRead->peek(out, available);

        // send data
        size_t send = _client->write((const char*) out, available);
        if(send != available) {
            DEBUG_ASYNC_TCP("[A-TCP] write failed send: %d available: %d \n", send, available);
            if(!connected()) {
                DEBUG_ASYNC_TCP("[A-TCP] incomplete transfer, connection lost.\n");
            }
        }

        // remove really send data from buffer
        _TXbufferRead->remove(send);

        // if buffer is empty and there is a other buffer in chain delete the empty one
        if(_TXbufferRead->available() == 0 && _TXbufferRead->next != NULL) {
            cbuf * old = _TXbufferRead;
            _TXbufferRead = _TXbufferRead->next;
            delete old;
            DEBUG_ASYNC_TCP("[A-TCP] delete cbuf\n");
        }

        delete out;
    }

}

/**
 * called on incoming data
 * @param buf
 * @param len
 */
void AsyncTCPbuffer::_rxData(uint8_t *buf, size_t len) {
    if(!_client || !_client->connected()) {
        DEBUG_ASYNC_TCP("[A-TCP] not connected!\n");
        return;
    }
    if(!_RXbuffer) {
        DEBUG_ASYNC_TCP("[A-TCP] _rxData no _RXbuffer!\n");
        return;
    }
    DEBUG_ASYNC_TCP("[A-TCP] _rxData len: %d RXmode: %d\n", len, _RXmode);

    size_t handled = 0;

    if(_RXmode != ATB_RX_MODE_NONE) {
        handled = _handleRxBuffer((uint8_t *) buf, len);
        buf += handled;
        len -= handled;

        // handle as much as possible before using the buffer
        if(_RXbuffer->empty()) {
            while(_RXmode != ATB_RX_MODE_NONE && handled != 0 && len > 0) {
                handled = _handleRxBuffer(buf, len);
                buf += handled;
                len -= handled;
            }
        }
    }

    if(len > 0) {

        if(_RXbuffer->room() < len) {
            // to less space
            DEBUG_ASYNC_TCP("[A-TCP] _rxData buffer full try resize\n");
            _RXbuffer->resizeAdd((len + _RXbuffer->room()));

            if(_RXbuffer->room() < len) {
                DEBUG_ASYNC_TCP("[A-TCP] _rxData buffer to full can only handle %d!!!\n", _RXbuffer->room());
            }
        }

        _RXbuffer->write((const char *) (buf), len);
    }

    if(!_RXbuffer->empty() && _RXmode != ATB_RX_MODE_NONE) {
        // handle as much as possible data in buffer
        handled = _handleRxBuffer(NULL, 0);
        while(_RXmode != ATB_RX_MODE_NONE && handled != 0) {
            handled = _handleRxBuffer(NULL, 0);
        }
    }

    // clean up ram
    if(_RXbuffer->empty() && _RXbuffer->room() != 100) {
        _RXbuffer->resize(100);
    }

}

/**
 *
 */
size_t AsyncTCPbuffer::_handleRxBuffer(uint8_t *buf, size_t len) {
    if(!_client || !_client->connected() || _RXbuffer == NULL) {
        return 0;
    }

    DEBUG_ASYNC_TCP("[A-TCP] _handleRxBuffer len: %d RXmode: %d\n", len, _RXmode);

    size_t BufferAvailable = _RXbuffer->available();
    size_t r = 0;

    if(_RXmode == ATB_RX_MODE_NONE) {
        return 0;
    } else if(_RXmode == ATB_RX_MODE_FREE) {
        if(_cbRX == NULL) {
            return 0;
        }

        if(BufferAvailable > 0) {
            uint8_t * b = new (std::nothrow) uint8_t[BufferAvailable];
            if(b == NULL){
              panic(); //TODO: What action should this be ?
            }
            _RXbuffer->peek((char *) b, BufferAvailable);
            r = _cbRX(b, BufferAvailable);
            _RXbuffer->remove(r);
        }

        if(r == BufferAvailable && buf && (len > 0)) {
            return _cbRX(buf, len);
        } else {
            return 0;
        }

    } else if(_RXmode == ATB_RX_MODE_READ_BYTES) {
        if(_rxReadBytesPtr == NULL || _cbDone == NULL) {
            return 0;
        }

        size_t newReadCount = 0;

        if(BufferAvailable) {
            r = _RXbuffer->read((char *) _rxReadBytesPtr, _rxSize);
            _rxSize -= r;
            _rxReadBytesPtr += r;
        }

        if(_RXbuffer->empty() && (len > 0) && buf) {
            r = len;
            if(r > _rxSize) {
                r = _rxSize;
            }
            memcpy(_rxReadBytesPtr, buf, r);
            _rxReadBytesPtr += r;
            _rxSize -= r;
            newReadCount += r;
        }

        if(_rxSize == 0) {
            _RXmode = ATB_RX_MODE_NONE;
            _cbDone(true, NULL);
        }

        // add left over bytes to Buffer
        return newReadCount;

    } else if(_RXmode == ATB_RX_MODE_TERMINATOR) {
        // TODO implement read terminator non string

    } else if(_RXmode == ATB_RX_MODE_TERMINATOR_STRING) {
        if(_rxReadStringPtr == NULL || _cbDone == NULL) {
            return 0;
        }

        // handle Buffer
        if(BufferAvailable > 0) {
            while(!_RXbuffer->empty()) {
                char c = _RXbuffer->read();
                if(c == _rxTerminator || c == 0x00) {
                    _RXmode = ATB_RX_MODE_NONE;
                    _cbDone(true, _rxReadStringPtr);
                    return 0;
                } else {
                    (*_rxReadStringPtr) += c;
                }
            }
        }

        if(_RXbuffer->empty() && (len > 0) && buf) {
            size_t newReadCount = 0;
            while(newReadCount < len) {
                char c = (char) *buf;
                buf++;
                newReadCount++;
                if(c == _rxTerminator || c == 0x00) {
                    _RXmode = ATB_RX_MODE_NONE;
                    _cbDone(true, _rxReadStringPtr);
                    return newReadCount;
                } else {
                    (*_rxReadStringPtr) += c;
                }
            }
            return newReadCount;
        }
    }

    return 0;
}
