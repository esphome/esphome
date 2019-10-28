#pragma once

#include <IPAddress.h>
#include <cstring>
#include <string>
#include <array>
#include <vector>
#include "esphome/core/optional.h"
#include <memory>

namespace esphome {
namespace tcp {

class ReadableStream {
 public:
  /** Return the number of bytes available to be read without blocking in the stream.
   *
   * Note: This can return a value greater than 0 even though the stream is closed if not all
   * bytes have been read.
   *
   * This method is guaranteed to return 0 if the end of the stream has been reached.
   *
   * @return The number of bytes that can be read from this stream without blocking.
   */
  virtual size_t available() = 0;
  /** Read size bytes from the stream.
   *
   * Note: You are required to call available() first to check if the amount of bytes to read
   * are readily available!
   *
   * As long as size < available, this function is guaranteed to work.
   *
   * @param buffer The buffer to read data into, must not be null.
   * @param size The number of bytes to read.
   */
  virtual void read(uint8_t *buffer, size_t size) = 0;
  virtual void skip(size_t size) = 0;
  virtual bool is_readable() = 0;

  template<size_t N> std::array<uint8_t, N> read_array() {
    std::array<uint8_t, N> res;
    this->read(res.data(), N);
    return res;
  }
  uint8_t read_byte() {
    uint8_t ret;
    this->read(&ret, 1);
    return ret;
  }
};

class WritableStream {
 public:
  virtual void write(const uint8_t *buffer, size_t size) = 0;
  /** Flushes all data in the output buffer and forces any buffered output to be written out.
   *
   * Note that this method will block if the number of buffered bytes is greater than zero.
   */
  virtual void flush() = 0;
  virtual bool is_writable() = 0;

  void write_byte(uint8_t data) { this->write(&data, 1); }
  void write_str(const char *str) { this->write(reinterpret_cast<const uint8_t *>(str), strlen(str)); }
  void write_str(const std::string &str) { this->write(reinterpret_cast<const uint8_t *>(str.data()), str.size()); }
  void write_vector(const std::vector<uint8_t> &data) { this->write(data.data(), data.size()); }
  template<size_t N> void write_array(const std::array<uint8_t, N> &data) { this->write(data.data(), data.size()); }
};

class BufferedWritableStream : public WritableStream {
 public:
  /** Check how many bytes this stream is currently able to write.
   *
   * It is guaranteed that any call to write_* with at most the number of bytes returned
   * by this function will not block.
   *
   * Note: The actual number of bytes that can be sent might be higher than this function reports
   * (it is a lower bound value). Two calls to available_for_write() with a write_* call in between
   * may even return the same value.
   *
   * @return The number of bytes this stream can write.
   */
  virtual size_t available_for_write() = 0;
  /** Make sure that if this stream's buffers are empty, it can send a packet that at has at least the given size.
   *
   * Note: This function does not guarantee that a payload of the given size can be written *immediately*,
   * it just guarantees that it can sink at least size number of bytes at some later point in time.
   *
   * This is especially useful if you need to transmit large packets and want to make sure not to
   * block forever because available_for_write never returns enough bytes.
   */
  virtual void reserve_at_least(size_t size) = 0;

  /** Ensure that the given amount of bytes can be written right now without blocking.
   *
   * Note: You should not use this function. If there's not enough space this will allocate a buffer
   * of the given size immediately.
   */
  virtual void ensure_capacity(size_t size) = 0;
};

class Connection : public ReadableStream, public BufferedWritableStream {
 public:
  virtual ~Connection() = default;

  virtual bool is_connected() = 0;
  virtual bool is_closed() = 0;

  /** Close this connection and release any system resources associated with it.
   *
   * Once close has been called, this connection is no longer available for any
   * operations. A new connection must be constructed.
   */
  virtual void close(bool force) = 0;
  void close() { this->close(false); };

  virtual void loop() = 0;
};

/** A platform-independent version of a TCP socket.
 *
 * This abstract class is intended to eventually replace all uses of WiFiClient in the ESPHome codebase.
 */
class TCPSocket : public Connection {
 public:
  TCPSocket() = default;
  TCPSocket(const TCPSocket &) = delete;
  enum State {
    /** The socket is initialized, but no call to `connect()` has been made yet.
     */
    STATE_INITIALIZED = 0,
    /** The socket is open (through connect()), but has not established a connection yet.
     */
    STATE_CONNECTING = 1,
    /** The socket is connected and fully operational.
     */
    STATE_CONNECTED = 2,
    /** The socket is currently closing after a close request has been issued through close(force=false).
     */
    STATE_CLOSING = 3,
    /** The socket has closed and all system resources associated with it have been released.
     * Any further read or write operations will result in undefined behaior.
     */
    STATE_CLOSED = 4,
  };

  virtual State state() = 0;

  virtual IPAddress get_remote_address() = 0;
  virtual uint16_t get_remote_port() = 0;
  virtual IPAddress get_local_address() = 0;
  virtual uint16_t get_local_port() = 0;
  virtual std::string get_host() = 0;

  virtual void set_no_delay(bool no_delay) = 0;
  virtual void set_timeout(uint32_t timeout_ms) = 0;

  bool is_writable() override { return this->state() == STATE_CONNECTED; }
  bool is_readable() override {
    switch (this->state()) {
      case STATE_CONNECTED:
      case STATE_CLOSING:
      case STATE_CLOSED:
        // socket can still be read after close()
        return true;
      default:
        return false;
    }
  }
  bool is_connected() override { return this->state() == STATE_CONNECTED; }
  bool is_closed() override { return this->state() == STATE_CLOSED; }
};

class TCPClient : public TCPSocket {
 public:
  virtual bool connect(IPAddress ip, uint16_t port) = 0;
  virtual bool connect(const std::string &host, uint16_t port) = 0;
};

std::unique_ptr<TCPSocket> make_socket();

class TCPServer {
 public:
  virtual ~TCPServer() = default;
  virtual bool bind(uint16_t port) = 0;
  virtual std::unique_ptr<TCPSocket> accept() = 0;
  void close() { this->close(false); }
  virtual void close(bool force) = 0;
};

std::unique_ptr<TCPServer> make_server();

class SSLTCPSocket : public TCPSocket {
 public:
  /// Set this SSL socket to accept *any* certificate. Note: VERY INSECURE!
  virtual void set_insecure() = 0;
  /** Set this SSL socket to match with a SHA1 fingerprint of the certificate.
   *
   * This is not very secure but at least better than accepting any connection.
   */
  virtual void set_fingerprint(const uint8_t fingerprint[20]) = 0;
  /** Set this SSL socket to match against a X509 certificate authority certificate (root CA).
   *
   * For example, for outbound services this might be digicert's CA key, or for a local network
   * it might be the certificate of the self-signed certificate authority (or any lower element
   * in the X509 chain)
   *
   * @param ca_cert A C-style string to the X509 certificate.
   */
  virtual void set_certificate_authority(const char *ca_cert) = 0;
};

}  // namespace tcp
}  // namespace esphome
