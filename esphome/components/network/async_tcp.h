/*
 * async_tcp.h
 *
 * Async Not Only TCP
 *
 *  Created on: Mar 5, 2020
 *      Author: Oleksandr Omelchuk
 */

#pragma once

#include "IPAddress.h"
#include <functional>

struct pbuf; // packet bufers from LwIp

namespace asynctcp {

class AsyncClient;

typedef std::function<void(void*, AsyncClient*)> AcConnectHandler;
typedef std::function<void(void*, AsyncClient*, size_t len, uint32_t time)> AcAckHandler;
typedef std::function<void(void*, AsyncClient*, int8_t error)> AcErrorHandler;
typedef std::function<void(void*, AsyncClient*, void *data, size_t len)> AcDataHandler;
typedef std::function<void(void*, AsyncClient*, pbuf *pb)> AcPacketHandler;
typedef std::function<void(void*, AsyncClient*, uint32_t time)> AcTimeoutHandler;

/**
 * Interface for dealing with network connections
 */
class AsyncClient {
  public:
    AsyncClient();
    virtual ~AsyncClient();

    virtual bool connect(const char* host, uint16_t port) = 0;
    virtual void close(bool now = false) = 0;
    virtual void stop() = 0;
    virtual int8_t abort() = 0;
    virtual bool free() = 0;


    virtual bool canSend() const = 0;
    virtual size_t space()const  = 0;                     //space available in the TCP window
    virtual size_t add(const char* data, size_t size) = 0;//add for sending
    virtual bool send() = 0;                              //send all data added with the method above

    virtual size_t write(const char* data) = 0;
    virtual size_t write(const char* data, size_t size) = 0; //only when canSend() == true

    virtual bool connecting() const = 0;
    virtual bool connected() const = 0;
    virtual bool disconnecting() const = 0;
    virtual bool disconnected() const = 0;
    virtual bool freeable() const = 0; //disconnected or disconnecting

    virtual void setNoDelay(bool nodelay) = 0;
    virtual bool getNoDelay() const = 0;

    virtual IPAddress remoteIP() = 0;

    // Set Callbacks
	virtual void onConnect(asynctcp::AcConnectHandler cb, void* arg) = 0;     //on successful connect
	virtual void onDisconnect(asynctcp::AcConnectHandler cb, void* arg) = 0;  //disconnected
	virtual void onAck(asynctcp::AcAckHandler cb, void* arg) = 0;             //ack received
	virtual void onError(asynctcp::AcErrorHandler cb, void* arg) = 0;         //unsuccessful connect or error
	virtual void onData(asynctcp::AcDataHandler cb, void* arg) = 0;           //data received (called if onPacket is not used)
	virtual void onPacket(asynctcp::AcPacketHandler cb, void* arg) = 0;       //data received
	virtual void onTimeout(asynctcp::AcTimeoutHandler cb, void* arg) = 0;     //ack timeout
	virtual void onPoll(asynctcp::AcConnectHandler cb, void* arg) = 0;        //every 125ms when connected

  protected:
    AsyncClient(const AsyncClient &other) {};

};

/**
 * Interface for network server
 */
class AsyncServer {
  public:
    virtual ~AsyncServer() {};

    virtual void onClient(AcConnectHandler cb, void* arg) = 0;
    virtual void begin() = 0;
    virtual void end() = 0;
    virtual void setNoDelay(bool nodelay) = 0;
    virtual bool getNoDelay() const = 0;
};

} // namespace asynctcp
