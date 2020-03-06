/*
 * async_tcp_client_impl.h
 *
 *  Created on: Mar 5, 2020
 *      Author: Oleksandr Omelchuk
 */

#pragma once

#include <memory>
#include "async_tcp.h"

class AsyncClient;

namespace esphome {
namespace network {


class AsyncTcpClientImpl: public asynctcp::AsyncClient {
public:
	AsyncTcpClientImpl(::AsyncClient *wrapped);
	virtual ~AsyncTcpClientImpl();

	virtual bool connect(const char* host, uint16_t port) override;
	virtual void close(bool now = false) override;
	virtual void stop() override;
	virtual int8_t abort() override;
	virtual bool free() override;


	virtual bool canSend() const override;
	virtual size_t space()const  override; //space available in the TCP window
	virtual size_t add(const char* data, size_t size) override;//add for sending
	virtual bool send() override;//send all data added with the method above

	virtual size_t write(const char* data) override;
	virtual size_t write(const char* data, size_t size) override; //only when canSend() == true

	virtual bool connecting() const override;
	virtual bool connected() const override;
	virtual bool disconnecting() const override;
	virtual bool disconnected() const override;
	virtual bool freeable() const override;//disconnected or disconnecting

	virtual void setNoDelay(bool nodelay) override;
	virtual bool getNoDelay() const override;

	virtual IPAddress remoteIP();

	// Set Callbacks
	virtual void onConnect(asynctcp::AcConnectHandler cb, void* arg) override;     //on successful connect
	virtual void onDisconnect(asynctcp::AcConnectHandler cb, void* arg) override;  //disconnected
	virtual void onAck(asynctcp::AcAckHandler cb, void* arg) override;             //ack received
	virtual void onError(asynctcp::AcErrorHandler cb, void* arg) override;         //unsuccessful connect or error
	virtual void onData(asynctcp::AcDataHandler cb, void* arg) override;           //data received (called if onPacket is not used)
	virtual void onPacket(asynctcp::AcPacketHandler cb, void* arg) override;       //data received
	virtual void onTimeout(asynctcp::AcTimeoutHandler cb, void* arg) override;     //ack timeout
	virtual void onPoll(asynctcp::AcConnectHandler cb, void* arg) override;        //every 125ms when connected

private:
	std::unique_ptr<::AsyncClient> impl_;
};

} /* namespace network */
} /* namespace esphome */

