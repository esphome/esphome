#ifdef USE_HOST
#include "uart_component_host.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#if !(defined(__linux__) || defined(__APPLE__))
#error This HostUartComponent implementation is not supported on this host OS
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

namespace {

speed_t get_baud(int baud) {
#ifdef __APPLE__
  return baud;
#else
  switch (baud) {
    case 50:
      return B50;
    case 75:
      return B75;
    case 110:
      return B110;
    case 134:
      return B134;
    case 150:
      return B150;
    case 200:
      return B200;
    case 300:
      return B300;
    case 600:
      return B600;
    case 1200:
      return B1200;
    case 1800:
      return B1800;
    case 2400:
      return B2400;
    case 4800:
      return B4800;
    case 9600:
      return B9600;
    case 19200:
      return B19200;
    case 38400:
      return B38400;
    case 57600:
      return B57600;
    case 115200:
      return B115200;
    case 230400:
      return B230400;
    case 460800:
      return B460800;
    case 500000:
      return B500000;
    case 576000:
      return B576000;
    case 921600:
      return B921600;
    case 1000000:
      return B1000000;
    case 1152000:
      return B1152000;
    case 1500000:
      return B1500000;
    case 2000000:
      return B2000000;
    case 2500000:
      return B2500000;
    case 3000000:
      return B3000000;
    case 3500000:
      return B3500000;
    case 4000000:
      return B4000000;
    default:
      return B0;
  }
#endif
}

}  // namespace

namespace esphome {
namespace uart {

static const char *const TAG = "uart.host";

HostUartComponent::~HostUartComponent() {
  if (this->file_descriptor_ != -1) {
    close(this->file_descriptor_);
    this->file_descriptor_ = -1;
  }
}

void HostUartComponent::setup() {
  ESP_LOGCONFIG(TAG, "Opening UART port...");
  speed_t baud = get_baud(this->baud_rate_);
  if (baud == B0) {
    ESP_LOGE(TAG, "Unsupported baud rate: %d", this->baud_rate_);
    this->mark_failed();
    return;
  }
  this->file_descriptor_ = ::open(this->port_name_.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
  if (this->file_descriptor_ == -1) {
    this->update_error_(strerror(errno));
    this->mark_failed();
    return;
  }
  fcntl(this->file_descriptor_, F_SETFL, 0);
  struct termios options;
  tcgetattr(this->file_descriptor_, &options);
  options.c_cflag &= ~CRTSCTS;
  options.c_cflag |= CREAD | CLOCAL;
  options.c_lflag &= ~ICANON;
  options.c_lflag &= ~ECHO;
  options.c_lflag &= ~ECHOE;
  options.c_lflag &= ~ECHONL;
  options.c_lflag &= ~ISIG;
  options.c_iflag &= ~(IXON | IXOFF | IXANY);
  options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
  options.c_oflag &= ~OPOST;
  options.c_oflag &= ~ONLCR;
  // Set data bits
  options.c_cflag &= ~CSIZE;  // Mask the character size bits
  switch (this->data_bits_) {
    case 5:
      options.c_cflag |= CS5;
      break;
    case 6:
      options.c_cflag |= CS6;
      break;
    case 7:
      options.c_cflag |= CS7;
      break;
    case 8:
    default:
      options.c_cflag |= CS8;
      break;
  }
  // Set parity
  switch (this->parity_) {
    case UART_CONFIG_PARITY_NONE:
      options.c_cflag &= ~PARENB;
      break;
    case UART_CONFIG_PARITY_EVEN:
      options.c_cflag |= PARENB;
      options.c_cflag &= ~PARODD;
      break;
    case UART_CONFIG_PARITY_ODD:
      options.c_cflag |= PARENB;
      options.c_cflag |= PARODD;
      break;
  };
  // Set stop bits
  if (this->stop_bits_ == 2) {
    options.c_cflag |= CSTOPB;
  } else {
    options.c_cflag &= ~CSTOPB;
  }
  cfsetispeed(&options, baud);
  cfsetospeed(&options, baud);
  tcsetattr(this->file_descriptor_, TCSANOW, &options);
}

void HostUartComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "UART:");
  ESP_LOGCONFIG(TAG, "  Port: %s", this->port_name_.c_str());
  if (this->file_descriptor_ == -1) {
    ESP_LOGCONFIG(TAG, "  Port status: Not opened");
    if (!this->first_error_.empty()) {
      ESP_LOGCONFIG(TAG, "  Error: %s", this->first_error_.c_str());
    }
    return;
  }
  ESP_LOGCONFIG(TAG, "  Port status: opened");
  ESP_LOGCONFIG(TAG, "  Baud Rate: %d", this->baud_rate_);
  ESP_LOGCONFIG(TAG, "  Data Bits: %d", this->data_bits_);
  ESP_LOGCONFIG(TAG, "  Parity: %s",
                this->parity_ == UART_CONFIG_PARITY_NONE   ? "None"
                : this->parity_ == UART_CONFIG_PARITY_EVEN ? "Even"
                                                           : "Odd");
  ESP_LOGCONFIG(TAG, "  Stop Bits: %d", this->stop_bits_);
  this->check_logger_conflict();
}

void HostUartComponent::write_array(const uint8_t *data, size_t len) {
  if (this->file_descriptor_ == -1) {
    return;
  }
  size_t written = ::write(this->file_descriptor_, data, len);
  if (written != len) {
    this->update_error_(strerror(errno));
    return;
  }
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(UART_DIRECTION_TX, data[i]);
  }
#endif
  return;
}

bool HostUartComponent::peek_byte(uint8_t *data) {
  if (this->file_descriptor_ == -1) {
    return false;
  }
  if (!this->has_peek_) {
    if (!this->check_read_timeout_()) {
      return false;
    }
    if (::read(this->file_descriptor_, &this->peek_byte_, 1) != 1) {
      this->update_error_(strerror(errno));
      return false;
    }
    this->has_peek_ = true;
  }
  *data = this->peek_byte_;
  return true;
}

bool HostUartComponent::read_array(uint8_t *data, size_t len) {
  if ((this->file_descriptor_ == -1) || (len == 0)) {
    return false;
  }
  if (!this->check_read_timeout_(len))
    return false;
  uint8_t *data_ptr = data;
  size_t length_to_read = len;
  if (this->has_peek_) {
    length_to_read--;
    *data_ptr = this->peek_byte_;
    data_ptr++;
    this->has_peek_ = false;
  }
  if (length_to_read > 0) {
    int sz = ::read(this->file_descriptor_, data_ptr, length_to_read);
    if (sz == -1) {
      this->update_error_(strerror(errno));
      return false;
    }
  }
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(UART_DIRECTION_RX, data[i]);
  }
#endif
  return true;
}

int HostUartComponent::available() {
  if (this->file_descriptor_ == -1) {
    return 0;
  }
  int available;
  int res = ioctl(this->file_descriptor_, FIONREAD, &available);
  if (res == -1) {
    this->update_error_(strerror(errno));
    return 0;
  }
  if (this->has_peek_)
    available++;
  return available;
};

void HostUartComponent::flush() {
  if (this->file_descriptor_ == -1) {
    return;
  }
  tcflush(this->file_descriptor_, TCIOFLUSH);
  ESP_LOGV(TAG, "    Flushing...");
}

void HostUartComponent::update_error_(const std::string &error) {
  if (this->first_error_.empty()) {
    this->first_error_ = error;
  }
  ESP_LOGE(TAG, "Port error: %s", error.c_str());
}

}  // namespace uart
}  // namespace esphome

#endif  // USE_HOST
