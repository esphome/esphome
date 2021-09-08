#pragma once
#include <cstdint>
#include <vector>
#include <deque>

#include "esphome/core/defines.h"

#include "esphome/components/socket/socket.h"

namespace esphome {
namespace api {

struct ReadPacketBuffer {
  std::vector<uint8_t> container;
  uint16_t type;
  size_t data_offset;
  size_t data_len;
};

struct PacketBuffer {
  const std::vector<uint8_t> container;
  uint16_t type;
  uint8_t data_offset;
  uint8_t data_len;
};

enum class APIError : int {
  OK = 0,
  WOULD_BLOCK = 1001,
  BAD_INDICATOR = 1003,
  BAD_DATA_PACKET = 1004,
  TCP_NODELAY_FAILED = 1005,
  TCP_NONBLOCKING_FAILED = 1006,
  CLOSE_FAILED = 1007,
  SHUTDOWN_FAILED = 1008,
  BAD_STATE = 1009,
  BAD_ARG = 1010,
  SOCKET_READ_FAILED = 1011,
  SOCKET_WRITE_FAILED = 1012,
  OUT_OF_MEMORY = 1018,
};

class APIFrameHelper {
 public:
  virtual APIError init() = 0;
  virtual APIError loop() = 0;
  virtual APIError read_packet(ReadPacketBuffer *buffer) = 0;
  virtual bool can_write_without_blocking() = 0;
  virtual APIError write_packet(uint16_t type, const uint8_t *data, size_t len) = 0;
  virtual std::string getpeername() = 0;
  virtual APIError close() = 0;
  virtual APIError shutdown(int how) = 0;
  // Give this helper a name for logging
  virtual void set_log_info(std::string info) = 0;
};
class APIPlaintextFrameHelper : public APIFrameHelper {
 public:
  APIPlaintextFrameHelper(std::unique_ptr<socket::Socket> socket) : socket_(std::move(socket)) {}
  ~APIPlaintextFrameHelper() = default;
  APIError init() override;
  APIError loop() override;
  APIError read_packet(ReadPacketBuffer *buffer) override;
  bool can_write_without_blocking() override;
  APIError write_packet(uint16_t type, const uint8_t *payload, size_t len) override;
  std::string getpeername() override { return socket_->getpeername(); }
  APIError close() override;
  APIError shutdown(int how) override;
  // Give this helper a name for logging
  void set_log_info(std::string info) override { info_ = std::move(info); }

 protected:
  struct ParsedFrame {
    std::vector<uint8_t> msg;
  };

  APIError try_read_frame_(ParsedFrame *frame);
  APIError try_send_tx_buf_();
  APIError write_frame_(const uint8_t *data, size_t len);
  APIError write_raw_(const uint8_t *data, size_t len);

  std::unique_ptr<socket::Socket> socket_;

  std::string info_;
  std::vector<uint8_t> rx_header_buf_;
  bool rx_header_parsed_ = false;
  uint32_t rx_header_parsed_type_ = 0;
  uint32_t rx_header_parsed_len_ = 0;

  std::vector<uint8_t> rx_buf_;
  size_t rx_buf_len_ = 0;

  std::vector<uint8_t> tx_buf_;

  enum class State {
    INITIALIZE = 1,
    DATA = 2,
    CLOSED = 3,
    FAILED = 4,
  } state_ = State::INITIALIZE;
};

}  // namespace api
}  // namespace esphome
