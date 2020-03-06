/*
 * async_tcp_server_impl.cpp
 *
 *  Created on: Mar 5, 2020
 *      Author: Oleksandr Omelchuk
 */

#include "async_tcp_server_impl.h"
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

AsyncTcpServerImpl::AsyncTcpServerImpl(uint16_t port)
	:impl_(new ::AsyncServer(port))
{

}

AsyncTcpServerImpl::~AsyncTcpServerImpl() {
	impl_->end();
}

void AsyncTcpServerImpl::onClient(asynctcp::AcConnectHandler cb, void* arg) {
	impl_->onClient([cb](void* arg, ::AsyncClient *connection) {
		AsyncTcpClientImpl *client_wrapper = new AsyncTcpClientImpl(connection);
		cb(arg, client_wrapper);
	}, arg);
}

void AsyncTcpServerImpl::begin() {
	impl_->begin();
}

void AsyncTcpServerImpl::end() {
	impl_->end();
}

void AsyncTcpServerImpl::setNoDelay(bool nodelay) {
	impl_->setNoDelay(nodelay);
}

bool AsyncTcpServerImpl::getNoDelay() const {
	return impl_->getNoDelay();
}

} /* namespace network */
} /* namespace esphome */
