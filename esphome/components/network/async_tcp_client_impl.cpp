/*
 * async_tcp_client_impl.cpp
 *
 *  Created on: Mar 5, 2020
 *      Author: Oleksandr Omelchuk
 */


#include "async_tcp_client_impl.h"
#include "esphome/core/log.h"
#ifdef ARDUINO_ARCH_ESP32
#include <AsyncTCP.h>
#endif
#ifdef ARDUINO_ARCH_ESP8266
#include <ESPAsyncTCP.h>
#endif

namespace esphome {
namespace network {

AsyncTcpClientImpl::AsyncTcpClientImpl(::AsyncClient *wrapped)
	: AsyncClient()
	, impl_(wrapped)
{
}


AsyncTcpClientImpl::~AsyncTcpClientImpl() {

}

bool AsyncTcpClientImpl::connect(const char* host, uint16_t port) {
	return impl_->connect(host, port);
}

void AsyncTcpClientImpl::close(bool now) {
	impl_->close(now);
}

void AsyncTcpClientImpl::stop() {
	impl_->stop();
}

int8_t AsyncTcpClientImpl::abort() {
	return abort();
}

bool AsyncTcpClientImpl::free() {
	return impl_->free();
}

bool AsyncTcpClientImpl::canSend() const {
	return impl_->canSend();
}

size_t AsyncTcpClientImpl::space()const  {
	return impl_->space();
}

size_t AsyncTcpClientImpl::add(const char* data, size_t size) {
	return impl_->add(data, size);
}

bool AsyncTcpClientImpl::send() {
	return impl_->send();
}

size_t AsyncTcpClientImpl::write(const char* data) {
	return impl_->write(data);
}

size_t AsyncTcpClientImpl::write(const char* data, size_t size) {
	return impl_->write(data, size);
}

bool AsyncTcpClientImpl::connecting() const {
	return impl_->connecting();
}

bool AsyncTcpClientImpl::connected() const {
	return impl_->connected();
}

bool AsyncTcpClientImpl::disconnecting() const {
	return impl_->disconnecting();
}

bool AsyncTcpClientImpl::disconnected() const {
	return impl_->disconnected();
}

bool AsyncTcpClientImpl::freeable() const {
	return impl_->freeable();
}
//disconnected or disconnecting

void AsyncTcpClientImpl::setNoDelay(bool nodelay) {
	impl_->setNoDelay(nodelay);
}

bool AsyncTcpClientImpl::getNoDelay() const {
	return getNoDelay();
}


IPAddress AsyncTcpClientImpl::remoteIP()
{
	return impl_->remoteIP();
}

// Set Callbacks

void AsyncTcpClientImpl::onConnect(asynctcp::AcConnectHandler cb, void* arg) {
	impl_->onConnect([cb, this](void* arg, ::AsyncClient*client){
		cb(arg, this);
	}, arg);
}

void AsyncTcpClientImpl::onDisconnect(asynctcp::AcConnectHandler cb, void* arg) {
	impl_->onDisconnect([cb, this](void* arg, ::AsyncClient*client){
		cb(arg, this);
	}, arg);
}

void AsyncTcpClientImpl::onAck(asynctcp::AcAckHandler cb, void* arg) {
	impl_->onAck([cb, this](void *arg, ::AsyncClient*cli, size_t len, uint32_t time){
		cb(arg, this, len, time);
	}, arg);
}

void AsyncTcpClientImpl::onError(asynctcp::AcErrorHandler cb, void* arg) {
	impl_->onError([cb, this](void* arg, ::AsyncClient*client, int8_t error){
		cb(arg, this, error);
	}, arg);
}

void AsyncTcpClientImpl::onData(asynctcp::AcDataHandler cb, void* arg) {
	impl_->onData([cb, this](void* arg, ::AsyncClient*client, void *data, size_t len){
		cb(arg, this, data, len);
	}, arg);
}

void AsyncTcpClientImpl::onPacket(asynctcp::AcPacketHandler cb, void* arg) {
	impl_->onPacket([cb, this](void* arg, ::AsyncClient*client, struct pbuf *pb){
		cb(arg, this, pb);
	}, arg);
}

void AsyncTcpClientImpl::onTimeout(asynctcp::AcTimeoutHandler cb, void* arg) {
	impl_->onTimeout([cb, this](void* arg, ::AsyncClient*client, uint32_t time){
		cb(arg, this, time);
	}, arg);
}

void AsyncTcpClientImpl::onPoll(asynctcp::AcConnectHandler cb, void* arg) {
	impl_->onPoll([cb, this](void* arg, ::AsyncClient*client){
		cb(arg, this);
	}, arg);
}

} /* namespace network */
} /* namespace esphome */
