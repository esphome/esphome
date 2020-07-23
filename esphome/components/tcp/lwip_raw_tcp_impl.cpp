#include "esphome/core/defines.h"
#ifdef USE_TCP_LWIP_RAW_TCP

#define LWIP_INTERNAL
extern "C" {
  #include "include/wl_definitions.h"
}

#include "lwip_raw_tcp_impl.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cerrno>

#include "lwip/ip.h"
#include "lwip/tcp.h"
#include "lwip/inet.h"
#include "lwip/netif.h"
#include "lwip/dns.h"

namespace esphome {
namespace tcp {

static const char *TAG = "lwip_tcp";
static const char *TAG_SERVER = "lwip_server";

#define SOCKET_LOGVV(format, ...) ESP_LOGVV(TAG, "%s: " format, this->host_.c_str(), ##__VA_ARGS__)
#define SOCKET_LOGV(format, ...) ESP_LOGV(TAG, "%s: " format, this->host_.c_str(), ##__VA_ARGS__)
#define SOCKET_LOG(format, ...) ESP_LOGD(TAG, "%s: " format, this->host_.c_str(), ##__VA_ARGS__)
#define SOCKET_SERVER_LOG(format, ...) ESP_LOGD(TAG_SERVER, format, ##__VA_ARGS__)
#define SOCKET_LOG_EVENT_LOOP(format, ...) ESP_LOGV(TAG, "%s: " format, this->host_.c_str(), ##__VA_ARGS__)

void pbuf_free_chain(struct pbuf *pb) {
  while (pb != nullptr) {
    auto *next = pb->next;
    pbuf_free(pb);
    pb = next;
  }
}

// Connect to the remote ip, port tuple.
bool LWIPRawTCPImpl::connect(IPAddress ip, uint16_t port) {
  this->host_ = ip.toString().c_str();
  this->port_ = port;
  return this->connect_(ip, port);
}
bool LWIPRawTCPImpl::connect_(IPAddress ip, uint16_t port) {
  SOCKET_LOGV("connect(ip=%s port=%u)", ip.toString().c_str(), port);
  this->pcb_ = tcp_new();
  if (this->pcb_ == nullptr) {
    SOCKET_LOG("tcp_new failed: %d", errno);
    return false;
  }

  // Set the local port for the outbound connection
  const uint16_t min_local_port = 32768;
  const uint16_t max_local_port = 61000;
  uint32_t rand = random_uint32();
  uint16_t local_port = (rand % (max_local_port - min_local_port)) + min_local_port;

  this->pcb_->local_port = local_port;
  tcp_setprio(this->pcb_, TCP_PRIO_MIN);
  this->setup_callbacks_();

  // convert IP address to LWIP format
  ip_addr_t ipa;
  ipa.addr = ip;

  // Call tcp_connect, this call is asynchronous
  err_t err = tcp_connect(this->pcb_, &ipa, port, &on_tcp_connected_static);
  if (err != ERR_OK) {
    SOCKET_LOG("tcp_connect failed: %d", errno);
    return false;
  }
  this->pending_connect_ = true;
  this->initialized_ = false;

  // Note: is not fully connected yet. Wait for state is_connected()
  return true;
}
void LWIPRawTCPImpl::abort() {
  SOCKET_LOGV("abort()");
  if (this->pcb_ == nullptr)
    return;
  if (this->rx_buffer_ != nullptr) {
    // Clear and free RX buffer
    tcp_recved(this->pcb_, this->rx_buffer_->tot_len);
    pbuf_free_chain(this->rx_buffer_);
    this->rx_buffer_ = nullptr;
  }

  this->remove_callbacks_();
  // "Aborts the connection by sending a RST (reset) segment to the remote host.
  // The pcb is deallocated. This function never fails."
  // so no need to call tcp_close() afterwards
  tcp_abort(this->pcb_);
  this->pcb_ = nullptr;
}
void LWIPRawTCPImpl::close(bool force) {
  SOCKET_LOGV("close(%s)", YESNO(force));
  if (this->pcb_ == nullptr)
    return;

  if (force) {
    this->abort();
    return;
  }

  this->remove_callbacks_();
  /* tcp_close:
      "Closes the connection. The function may return ERR_MEM if no memory
       was available for closing the connection. If so, the application
       should wait and try again either by using the acknowledgment
       callback or the polling functionality. If the close succeeds, the
       function returns ERR_OK.""
  */
  err_t err = tcp_close(this->pcb_);
  if (err != ERR_OK) {
    // Close failed, instead of waiting we just deallocate forcefully with tcp_abort
    SOCKET_LOG("close() failed: %d", err);
    tcp_abort(this->pcb_);
  }
  this->pcb_ = nullptr;
}

// Set no-delay (NAGLE) on TCP buffer
void LWIPRawTCPImpl::set_no_delay(bool no_delay) {
  if (this->pcb_ == nullptr)
    return;

  if (no_delay) {
    tcp_nagle_disable(this->pcb_);
  } else {
    tcp_nagle_enable(this->pcb_);
  }
}
// Helper functions
IPAddress LWIPRawTCPImpl::get_remote_address() {
  if (this->pcb_ == nullptr)
    return {};
  return {reinterpret_cast<const uint8_t *>(&this->pcb_->remote_ip)};
}
uint16_t LWIPRawTCPImpl::get_remote_port() {
  if (this->pcb_ == nullptr)
    return 0;
  return this->pcb_->remote_port;
}
IPAddress LWIPRawTCPImpl::get_local_address() {
  if (this->pcb_ == nullptr)
    return {};
  return {reinterpret_cast<const uint8_t *>(&this->pcb_->local_ip)};
}
uint16_t LWIPRawTCPImpl::get_local_port() {
  if (this->pcb_ == nullptr)
    return 0;
  return this->pcb_->local_port;
}

size_t LWIPRawTCPImpl::available() {
  // Check how many bytes of data are available to consume (read)
  if (this->rx_buffer_ == nullptr)
    return 0;

  assert(this->rx_buffer_->tot_len >= this->rx_buffer_offset_);
  size_t available = this->rx_buffer_->tot_len - this->rx_buffer_offset_;
  // SOCKET_LOGVV("available() -> %u", available);
  return available;
}
bool LWIPRawTCPImpl::read(uint8_t *destination_buffer, size_t size) {
  if (this->rx_buffer_ == nullptr)
    return size == 0;

  size_t available = this->available();
  if (size > available) {
    SOCKET_LOG("Requested read() with size %u when only %u bytes are available! This call will now block!",
               size, available);
  }
  SOCKET_LOGVV("read(dest=%p, size=%u) avail=%u", destination_buffer, size, available);

  size_t to_read = size;
  while (to_read != 0) {
    while (this->available() == 0) {
      // Waiting for data, blocking call!
      if (!this->is_readable()) {
        SOCKET_LOG("read() Socket closed while reading blocking data!");
        return false;
      }
      yield();
    }

    assert(this->rx_buffer_ != nullptr);
    assert(this->rx_buffer_offset_ < this->rx_buffer_->len);

    // Calculate number of bytes to copy (minimum from number of bytes in current chunk and
    // number of bytes to read)
    const size_t chunk_left = this->rx_buffer_->len - this->rx_buffer_offset_;
    const size_t copy_size = std::min(chunk_left, to_read);

    if (destination_buffer != nullptr) {
      pbuf_copy_partial(this->rx_buffer_, destination_buffer, copy_size, this->rx_buffer_offset_);
      destination_buffer += copy_size;
    }
    to_read -= copy_size;

    // Notify lwip of how much we consumed
    if (this->pcb_ != nullptr) {
      // Socket may be closed already and we're reading remaining data
      // SOCKET_LOGVV("  tcp_recved(%u)", copy_size);
      tcp_recved(this->pcb_, copy_size);
    }

    // Consume chunk
    if (copy_size == chunk_left) {
      if (this->rx_buffer_->next == nullptr) {
        // no next chunk
        // SOCKET_LOGVV("  pbuf_free(%p)", this->rx_buffer_);
        pbuf_free(this->rx_buffer_);
        this->rx_buffer_ = nullptr;
        this->rx_buffer_offset_ = 0;
      } else {
        // advance chunk
        pbuf *old_head = this->rx_buffer_;
        this->rx_buffer_ = this->rx_buffer_->next;
        this->rx_buffer_offset_ = 0;
        // SOCKET_LOGVV("  pbuf_ref(%p)", this->rx_buffer_);
        pbuf_ref(this->rx_buffer_);
        // SOCKET_LOGVV("  pbuf_free(%p)", old_head);
        pbuf_free(old_head);
      }
    } else {
      // still some data left in this chunk
      this->rx_buffer_offset_ += copy_size;
    }
  }

  return true;
}
size_t LWIPRawTCPImpl::sendbuf_size() {
  if (this->pcb_ == nullptr)
    return 0;
  return tcp_sndbuf(this->pcb_);
}
size_t LWIPRawTCPImpl::available_for_write() { return this->sendbuf_size() + this->reserve_buffer_.capacity(); }
bool LWIPRawTCPImpl::write(const uint8_t *buffer, size_t size) {
  if (buffer == nullptr) {
    SOCKET_LOG("write() called with null buffer!");
    return false;
  }
  if (this->pcb_ == nullptr) {
    SOCKET_LOG("write() called on unintialized socket!");
    return false;
  }

  this->drain_reserve_buffer_();
  if (size > this->available_for_write()) {
    SOCKET_LOG("write: Attempted to write more than allocated! size=%u available_for_write=%u",
               size, this->available_for_write());
    // push_back will handle auto-resizing
  }

  SOCKET_LOGVV("write(%p, %u)", buffer, size);

  if (this->reserve_buffer_.empty()) {
    // reserve buffer is empty, write directly to TCP
    size_t sendbuf_size = this->sendbuf_size();
    size_t to_write = std::min(sendbuf_size, size);
    bool has_more = to_write != size;
    size_t written = this->write_internal_(buffer, to_write, !has_more);
    if (written != to_write) {
      // writing failed, overwrite to_write to instead put it in the reserve buffer
      to_write = written;
    }

    if (has_more) {
      // write rest in reserve buffer
      SOCKET_LOG("  reserve_buffer.push_back(%p, %u)", buffer + to_write, size - to_write);
      this->reserve_buffer_.push_back(buffer + to_write, size - to_write);
    }
  } else {
    // reserve buffer has data, write to reserve buffer
    SOCKET_LOG("  reserve_buffer.push_back(%p, %u)", buffer, size);
    this->reserve_buffer_.push_back(buffer, size);
  }

  return true;
}
void LWIPRawTCPImpl::setup_callbacks_() {
  SOCKET_LOGVV("setup_callbacks_(%p)", this->pcb_);
  // argument for all callbacks will be pointer to this class
  tcp_arg(this->pcb_, this);
  tcp_recv(this->pcb_, &on_tcp_recv_static);
  tcp_sent(this->pcb_, &on_tcp_sent_static);
  tcp_err(this->pcb_, &on_tcp_err_static);
}
void LWIPRawTCPImpl::remove_callbacks_() {
  SOCKET_LOGVV("remove_callbacks_(%p)", this->pcb_);
  if (this->pcb_ == nullptr)
    return;
  tcp_sent(this->pcb_, nullptr);
  tcp_recv(this->pcb_, nullptr);
  tcp_err(this->pcb_, nullptr);
  tcp_arg(this->pcb_, nullptr);
}
/// Internal function called to write `buffer` to the tcp layer.
size_t LWIPRawTCPImpl::write_internal_(const uint8_t *buffer, size_t size, bool psh_flag) {
  SOCKET_LOGVV("  write_internal_(%p, size=%u, PSH=%s)", buffer, size, YESNO(psh_flag));
  assert(this->pcb_ != nullptr);
  if (size == 0)
    return;
  const size_t sendbuf = this->sendbuf_size();
  assert(size <= sendbuf);
  // SOCKET_LOGVV("  sendbuf: %u", sendbuf);

  // Data should be copied to internal TCP buffer. It just complicates
  // the code too much to keep track of what data has been ACKed already.
  uint8_t flags = TCP_WRITE_FLAG_COPY;
  // - TCP_WRITE_FLAG_COPY: indicates whether the new memory should be allocated
  // for the data to be copied into. If this flag is not given, no new memory
  // should be allocated and the data should only be referenced by pointer. This
  // also means that the memory behind dataptr must not change until the data is
  // ACKed by the remote host

  if (!psh_flag) {
    flags |= TCP_WRITE_FLAG_MORE;
    // - TCP_WRITE_FLAG_MORE: indicates that more data follows. If this is omitted,
    // the PSH flag is set in the last segment created by this call to tcp_write.
    // If this flag is given, the PSH flag is not set.
  }


  err_t err = tcp_write(this->pcb_, buffer, size, flags);
  if (err != ERR_OK) {
    // Most likely ERR_MEM (but should not happen in practice because we check sndbuf size)
    SOCKET_LOG("tcp_write failed: %d", err);
    return 0;
  }

  // tcp_output: "Find out what we can send and send it"
  err = tcp_output(this->pcb_);
  if (err != ERR_OK) {
    // "Returns ERR_OK if data has been sent or nothing to send another err_t on error"
    SOCKET_LOG("tcp_output failed: %d", err);
    // data has already been written, use normal return
  }

  return size;
}
// We have a reserve buffer for data that could not fit in the TCP
// sendbuffer anymore and was buffered.
// This function checks if anything can be sent now, and if so sends it.
// Returns whether the reserve buffer is empty.
bool LWIPRawTCPImpl::drain_reserve_buffer_() {
  while (!this->reserve_buffer_.empty()) {
    // Loop until sendbuffer is full
    size_t sendbuf_size = this->sendbuf_size();
    if (sendbuf_size == 0)
      return false;
    SOCKET_LOGVV("drain_reserve_buffer_ sendbuf=%u size=%u", sendbuf_size, this->reserve_buffer_.size());
    auto pair = this->reserve_buffer_.peek_front_linear(sendbuf_size);
    size_t ret = this->write_internal_(pair.first, pair.second, !this->reserve_buffer_.empty());
    this->reserve_buffer_.pop_front_linear(ret);
    if (ret != pair.second) {
      // Writing failed, stop draining.
      return false;
    }
  }
  return true;
}
err_t LWIPRawTCPImpl::on_tcp_recv_(tcp_pcb *pcb, pbuf *packet_buffer, err_t err) {
  // tcp_recv:
  // The callback function will be passed a NULL pbuf to
  // indicate that the remote host has closed the connection. If
  // there are no errors and the callback function is to return
  // ERR_OK, then it must free the pbuf. Otherwise, it must not
  // free the pbuf so that lwIP core code can store it.
  //
  // If the callback function returns ERR_OK or ERR_ABRT it must have freed the pbuf,
  // otherwise it must not have freed it.

  SOCKET_LOG_EVENT_LOOP("on_tcp_recv_(pcb=%p pb=%p err=%d)", pcb, packet_buffer, err);
  if (packet_buffer == nullptr) {
    // connection closed
    SOCKET_LOG_EVENT_LOOP("  -> connection closed!");

    this->pending_connect_ = false;
    this->pending_error_ = true;
    this->remove_callbacks_();

    // pbuf is nullptr, nothing to free

    // If we return ERR_ABRT or ERR_OK, the packet buffer is now under our control
    // and we are responsible for freeing it.
    // https://www.nongnu.org/lwip/2_1_x/group__tcp__raw.html#ga8afd0b316a87a5eeff4726dc95006ed0
    return ERR_ABRT;
  }

  if (this->rx_buffer_ != nullptr) {
    SOCKET_LOG_EVENT_LOOP("  rx_buf(%d) += %d", this->rx_buffer_->tot_len, packet_buffer->tot_len);
    // pbuf_cat: Concatenate two pbufs (each may be a pbuf chain) and take over the caller's reference of the tail pbuf.
    //
    // This means we don't need to call pbuf_free.
    pbuf_cat(this->rx_buffer_, packet_buffer);
  } else {
    SOCKET_LOG_EVENT_LOOP("  rx_buf(0) += %d", packet_buffer->tot_len);
    this->rx_buffer_ = packet_buffer;
  }

  // TODO: lwIP docs say the pbuf should be freed when returning OK, but we don't do that here.
  // ClientContext.h in Arduino framework also does that, but is it correct?

  return ERR_OK;
}
err_t LWIPRawTCPImpl::on_tcp_sent_(tcp_pcb *pcb, uint16_t len) {
  SOCKET_LOG_EVENT_LOOP("on_tcp_sent_(pcb=%p len=%u)", pcb, len);
  // data has been acked, which means more space is available in the reserve buffer
  // so now is a good time to output any data that we have in the reserve buffer.
  if (this->is_writable())
    this->drain_reserve_buffer_();
  return ERR_OK;
}
void LWIPRawTCPImpl::on_tcp_err_(err_t err) {
  // this->pcb_ will already be freed!
  SOCKET_LOG_EVENT_LOOP("on_tcp_err_(err=%d)", err);
  tcp_sent(this->pcb_, nullptr);
  tcp_recv(this->pcb_, nullptr);
  tcp_err(this->pcb_, nullptr);
  tcp_arg(this->pcb_, nullptr);
  this->pcb_ = nullptr;
}
err_t LWIPRawTCPImpl::on_tcp_connected_(tcp_pcb *pcb, err_t err) {
  SOCKET_LOG_EVENT_LOOP("on_tcp_connected_(pcb=%p, err=%d)", pcb, err);
  assert(pcb == this->pcb_);
  this->pending_connect_ = false;
  return ERR_OK;
}
bool LWIPRawTCPImpl::flush() {
  if (this->pcb_ == nullptr)
    return false;

  SOCKET_LOGVV("flush()");
  uint32_t start_time = millis();
  while (true) {
    uint32_t now = millis();
    if (now - start_time > 50)
      // wait a maximum of 50ms (could be done better)
      return false;

    tcp_output(this->pcb_);
    this->drain_reserve_buffer_();
    size_t sendbuf = this->sendbuf_size();
    // This check appears to work and be used in other places as well
    // (ClientContext in eps8266 Arduino).
    if (sendbuf == TCP_SND_BUF)
      return true;
  }
}

// Callback when looking up a host finishes
void LWIPRawTCPImpl::on_dns_found_(const char *name, const ip_addr_t *ipaddr) {
  if (ipaddr == nullptr) {
    this->dns_resolved_error_ = true;
  } else {
    this->dns_callback_result_ = IPAddress(ipaddr->addr);
    this->dns_resolved_success_ = true;
  }
}
bool LWIPRawTCPImpl::connect(const std::string &host, uint16_t port) {
  IPAddress result;
  if (result.fromString(host.c_str())) {
    return this->connect(result, port);
  }

  SOCKET_LOG("connect(host=%s, port=%u)", host.c_str(), port);

  this->host_ = host;
  this->port_ = port;
  ip_addr_t addr;

  // look of host with DNS, if can't finish immediately, callback will be called later
  // and ERR_INPROGRESS is returned.
  err_t err = dns_gethostbyname(host.c_str(), reinterpret_cast<ip_addr_t *>(&result),
                                &on_dns_found_static_, this);

  if (err == ERR_OK) {
    // lookup finished immediately
    result = addr.addr;
    return this->connect(result, port);
  } else if (err == ERR_INPROGRESS) {
    // lookup will finish in future
    this->pending_dns_result_ = true;
    return true;
  } else {
    SOCKET_LOG("dns_gethostbyname failed! %d", err);
    return false;
  }
}
void LWIPRawTCPImpl::set_timeout(uint32_t timeout_ms) {
  // TODO
}
void LWIPRawTCPImpl::reserve_at_least(size_t size) {
  if (size <= TCP_SND_BUF)
    return;
  if (size - TCP_SND_BUF > this->reserve_buffer_.capacity()) {
    SOCKET_LOGVV("reserve_at_least(%u) capacity=%u", size, this->reserve_buffer_.capacity());
  }
  this->reserve_buffer_.reserve(size - TCP_SND_BUF);
}
void LWIPRawTCPImpl::ensure_capacity(size_t size) {
  size_t avail = this->available_for_write();
  size_t sndbuf = this->sendbuf_size();
  if (size <= avail || size <= sndbuf)
    return;

  if (size - sndbuf > this->reserve_buffer_.capacity()) {
    SOCKET_LOGVV("ensure_capacity(%u) avail=%u capacity=%u", size, avail, this->reserve_buffer_.capacity());
  }
  this->reserve_buffer_.reserve(size - sndbuf);
}
void LWIPRawTCPImpl::loop() {
  if (this->pending_error_) {
    this->abort();
    this->pending_error_ = false;
    return;
  }
  if (this->pending_dns_result_) {
    if (this->dns_resolved_error_) {
      SOCKET_LOG("dns lookup failed - can't connect.");
      this->abort();
      return;
    }
    if (this->dns_resolved_success_) {
      SOCKET_LOGV("dns lookup successful: %s", this->dns_callback_result_.toString().c_str());
      this->connect_(this->dns_callback_result_, this->port_);
    }
  }
  if (this->is_writable())
    this->drain_reserve_buffer_();
}
TCPSocket::State LWIPRawTCPImpl::state() {
  if (this->initialized_)
    return TCPSocket::STATE_INITIALIZED;

  if (this->pcb_ == nullptr)
    return TCPSocket::STATE_CLOSED;

  if (this->pending_connect_)
    return TCPSocket::STATE_CONNECTING;

  switch (this->pcb_->state) {
    case CLOSED:
      return TCPSocket::STATE_CLOSED;
    case LISTEN:
    case SYN_SENT:
    case SYN_RCVD:
      return TCPSocket::STATE_CONNECTING;
    case ESTABLISHED:
      return TCPSocket::STATE_CONNECTED;
    case FIN_WAIT_1:
    case FIN_WAIT_2:
    case CLOSE_WAIT:
    case CLOSING:
    case LAST_ACK:
    case TIME_WAIT:
      return TCPSocket::STATE_CLOSING;
    default:
      SOCKET_LOGV("Unknown state %d for socket!", this->pcb_->state);
      return TCPSocket::STATE_CLOSED;
  }
}
std::string LWIPRawTCPImpl::get_host() {
  return this->host_;
}
LWIPRawTCPImpl::~LWIPRawTCPImpl() {
  if (this->pcb_ != nullptr) {
    abort();
  }
  pbuf_free_chain(this->rx_buffer_);
  this->rx_buffer_ = nullptr;
}
LWIPRawTCPImpl::LWIPRawTCPImpl(tcp_pcb *pcb) {
  this->pcb_ = pcb;
  this->initialized_ = false;
  this->host_ = this->get_remote_address().toString().c_str();
  SOCKET_LOGVV("LWIPRawTCPImpl(%p)", pcb);
  tcp_setprio(this->pcb_, TCP_PRIO_MIN);
  this->setup_callbacks_();
}

// ================== Server ==================
bool LWIPRawTCPServerImpl::bind(uint16_t port) {
  tcp_pcb *bind_pcb = tcp_new();
  if (bind_pcb == nullptr) {
    SOCKET_SERVER_LOG("server tcp_new failed! %d", errno);
    return false;
  }
  bind_pcb->so_options |= SOF_REUSEADDR;

  ip_addr_t ipa;
  ipa.addr = IPAddress(0, 0, 0, 0);
  err_t err = tcp_bind(bind_pcb, &ipa, port);
  if (err != ERR_OK) {
    SOCKET_SERVER_LOG("server tcp_bind failed! %d", err);
    tcp_close(bind_pcb);
    return false;
  }

  this->pcb_ = tcp_listen(bind_pcb);
  if (this->pcb_ == nullptr) {
    SOCKET_SERVER_LOG("server tcp_listen failed! %d", errno);
    tcp_close(bind_pcb);
    return false;
  }

  tcp_accept(this->pcb_, &on_tcp_accept_static);
  tcp_arg(this->pcb_, this);
  return true;
}
std::unique_ptr<TCPSocket> LWIPRawTCPServerImpl::accept() {
  if (this->accepted_.empty())
    return std::unique_ptr<TCPSocket>();

  auto *socket = this->accepted_.front();
  this->accepted_.pop();
  return std::unique_ptr<TCPSocket>(socket);
}
void LWIPRawTCPServerImpl::close(bool force) {
  if (this->pcb_ == nullptr)
    return;
  tcp_close(this->pcb_);
  this->pcb_ = nullptr;
}
err_t LWIPRawTCPServerImpl::on_tcp_accept_(tcp_pcb *new_pcb, err_t err) {
  this->accepted_.push(new LWIPRawTCPImpl(new_pcb));
  tcp_accepted(this->pcb_);
  return ERR_OK;
}

}  // namespace tcp
}  // namespace esphome

#endif  // USE_TCP_LWIP_RAW_TCP
