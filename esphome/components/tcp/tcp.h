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
   * @param buffer The buffer to read data into, must not be null.
   * @param size The number of bytes to read.
   * @return True if the operation was successful. If false, there was an error and the stream
   *   should be closed.
   */
  virtual bool read(uint8_t *buffer, size_t size) = 0;
  /// Check if the stream is currently readable (does not check if data is available).
  virtual bool is_readable() = 0;

  /// Read into a std::array object with a template function.
  template<size_t N> std::array<uint8_t, N> read_array() {
    std::array<uint8_t, N> res;
    this->read(res.data(), N);
    return res;
  }
};

/** A writable stream.
 *
 * All writable streams are buffered.
 */
class WritableStream {
 public:
  /** Write `size` bytes from `buffer` to the stream.
   *
   * Note: The data may be buffered internally.
   *
   * @return If the operation was successful. If false, there was an unrecoverable error and the
   *   stream should be closed.
   */
  virtual bool write(const uint8_t *buffer, size_t size) = 0;
  /** Flushes all data in the output buffer and forces any buffered output to be written out.
   *
   * Note that this method will block if the number of buffered bytes is greater than zero.
   *
   * Note: This does not guarantee the remote actually received the data, only that the data
   * has been successfully passed on to the low-level stack.
   *
   * @return If the operation was successful.
   */
  virtual bool flush() = 0;
  /// Return if the stream is in a state where data can be written to it.
  virtual bool is_writable() = 0;

  /// Write a single byte to the stream.
  bool write_byte(uint8_t data) { return this->write(&data, 1); }
  /// Write a string to the stream.
  bool write_str(const char *str) { return this->write(reinterpret_cast<const uint8_t *>(str), strlen(str)); }
  /// Write a string to the stream.
  bool write_str(const std::string &str) { return this->write(reinterpret_cast<const uint8_t *>(str.data()), str.size()); }
  /// Write a vector of data to the stream
  bool write_vector(const std::vector<uint8_t> &data) { return this->write(data.data(), data.size()); }
  template<size_t N>
  bool write_array(const std::array<uint8_t, N> &data) { return this->write(data.data(), data.size()); }

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

class Connection : public ReadableStream, public WritableStream {
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

const char *socket_state_to_string(TCPSocket::State state);

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

}  // namespace tcp
}  // namespace esphome
