/// @file gen_uart.h
/// @author DrCoolZic
/// @brief  generic uart class declaration

#pragma once
#include <bitset>
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace gen_uart {

// here we indicate if we want to include the auto tests (recommended)
#define AUTOTEST_COMPONENT

/// @brief size of the ring buffer
constexpr size_t RING_BUFFER_SIZE = 128;
///////////////////////////////////////////////////////////////////////////////
/// @brief This is an helper class that provides a simple ring buffers
/// that implements a FIFO function
///
/// This ring buffer is used to buffer the exchanges between the receive HW fifo
/// and the client. Without this buffer for reading one character usually, you
/// first check if bytes were received and if so you read one character.
/// This is fine if the registers are located on the chip but with a
/// device like the wk2132 these registers are remote and therefore accessing
/// them requires transactions on the I²C bus which is relatively slow. One
/// solution would be for the client to check the number of bytes available
/// and to read all of them using the read_array() method. Unfortunately
/// most client I have reviewed are reading one character at a time in a
/// while loop. Therefore the solution I have chosen to implement is to
/// store received bytes locally in a buffer as soon as they arrive. With
/// this solution the bytes are stored locally and therefore accessible
/// very quickly
///////////////////////////////////////////////////////////////////////////////
template<typename T, int SIZE> class RingBuffer {
 public:
  /// @brief pushes an item at the tail of the fifo
  /// @param item item to push
  /// @return true if item has been pushed, false il item was not pushed (buffer full)
  bool push(const T item) {
    if (is_full())
      return false;
    rb_[head_] = item;
    head_ = (head_ + 1) % SIZE;
    count_++;
    return true;
  }

  /// @brief return and remove the item at head of the fifo
  /// @param item item read
  /// @return true if item has been retrieved, false il no item was found (buffer empty)
  bool pop(T &item) {
    if (is_empty())
      return false;
    item = rb_[tail_];
    tail_ = (tail_ + 1) % SIZE;
    count_--;
    return true;
  }

  /// @brief return the value of the item at fifo's head without removing it
  /// @param item pointer to item to return
  /// @return true if item has been retrieved, false il no item was found (buffer empty)
  bool peek(T &item) {
    if (is_empty())
      return false;
    item = rb_[tail_];
    return true;
  }

  /// @brief test is the Ring Buffer is empty ?
  /// @return true if empty
  bool is_empty() { return (count_ == 0); }

  /// @brief test is the ring buffer is full ?
  /// @return true if full
  bool is_full() { return (count_ == SIZE); }

  /// @brief return the number of item in the ring buffer
  /// @return the count
  size_t count() { return count_; }

  /// @brief returns the free positions in the buffer
  /// @return how many item can still be added
  size_t free() { return SIZE - count_; }

 private:
  std::array<T, SIZE> rb_{0};
  int head_{0};      // points to the next element to write
  int tail_{0};      // points to the next element to read
  size_t count_{0};  // count number of element in the buffer
};

///////////////////////////////////////////////////////////////////////////////
/// @brief Describes a generic UART component
///
/// This class is created to avoid code duplication for external UART components.
/// It implements all the virtual methods of the factory class UARTComponent at
/// one level of abstraction above the hardware level. The children derived class
/// will deal with access to component registers. At this level of abstraction
/// we do not care about the physical details and therefore it should work for
/// any kind of bus implementation (I²C, SPI, UART, ...) between the micro-
/// controller and the component.
/// So the hierarchy of classes will look like this
/// UARTComponent
/// - GenericUART
///   + wk2132 i2c
///   + wk2132 spi
///   + sc16is75x i2c
///   + sc16is75x spi
///   + ...
///////////////////////////////////////////////////////////////////////////////
class GenericUART : public uart::UARTComponent {
 public:
  /// @brief Writes a specified number of bytes from a buffer to a serial port
  /// @param buffer pointer to the buffer
  /// @param len number of bytes to write
  ///
  /// This method sends 'len' characters from the buffer to the serial line.
  /// Unfortunately (unlike the Arduino equivalent) this method
  /// does not return any indicator and therefore it is not possible
  /// to know if any/all bytes have been transmitted correctly. Another problem
  /// is that it is not possible to know ahead of time how many bytes we
  /// can safely send as there is no tx_available() method provided!
  /// To avoid overrun when using the write method you can use the flush()
  /// method to wait until the transmit fifo is empty.
  ///
  /// Typical usage could be:
  /// @code
  ///   // ...
  ///   uint8_t buffer[128];
  ///   // ...
  ///   write_array(&buffer, len);
  ///   flush();
  ///   // ...
  /// @endcode
  void write_array(const uint8_t *buffer, size_t len) override;

  /// @brief Reads a specified number of bytes from a serial port to a buffer
  /// @param buffer pointer to the buffer
  /// @param len number of bytes to read
  /// @return true if succeed, false otherwise
  ///
  /// Typical usage:
  /// @code
  ///   // ...
  ///   auto len = available();
  ///   uint8_t buffer[64];
  ///   if (len > 0) {
  ///     auto status = read_array(&buffer, len)
  ///   }
  ///   // test status ...
  /// @endcode
  bool read_array(uint8_t *buffer, size_t len) override;

  /// @brief Reads first byte in FIFO without removing it
  /// @param buffer pointer to the byte
  /// @return true if succeed reading one byte, false if no character available
  ///
  /// This method returns the next byte from receiving buffer without
  /// removing it from the internal fifo. It returns true if a character
  /// is available and has been read, false otherwise.\n
  bool peek_byte(uint8_t *buffer) override { return this->receive_buffer_.peek(*buffer); }

  /// @brief Returns the number of bytes in the receive buffer
  /// @return the number of bytes available in the receiver fifo
  int available() override { return this->receive_buffer_.count(); }

  /// @brief Flush the output fifo.
  ///
  /// If we refer to Serial.flush() in Arduino it says: ** Waits for the transmission
  /// of outgoing serial data to complete. (Prior to Arduino 1.0, this the method was
  /// removing any buffered incoming serial data.). **
  /// The method tries to wait until all characters inside the fifo have been sent.
  /// It timeout after 100 ms and therefore at very low speed you can't be sure that
  /// all the characters have correctly been sent
  void flush() override;

 protected:
  friend class WK2132Component;

  /// @brief cannot happen with external uart
  void check_logger_conflict() override {}

  //
  // below are the virtual methods that derived class must implement
  //

  /// @brief Returns the number of bytes in the receive fifo
  /// @return the number of bytes in the fifo
  virtual size_t rx_in_fifo_() = 0;

  /// @brief Returns the number of bytes in the transmit fifo
  /// @return the number of bytes in the fifo
  virtual size_t tx_in_fifo_() = 0;

  /// @brief Reads data from the receive fifo to a buffer
  /// @param buffer the buffer
  /// @param len the number of bytes we want to read
  /// @return true if succeed false otherwise
  virtual bool read_data_(uint8_t *buffer, size_t len) = 0;

  /// @brief Writes data from a buffer to the transmit fifo
  /// @param buffer the buffer
  /// @param len the number of bytes we want to write
  /// @return true if succeed false otherwise
  virtual bool write_data_(const uint8_t *buffer, size_t len) = 0;

  virtual size_t max_size_() = 0;

  ///
  /// below are our private attributes / methods
  ///

  /// @brief we transfer the bytes in the rx_fifo to the ring buffer
  void rx_fifo_to_ring_();

  /// @brief the buffer where we store temporarily the bytes received
  RingBuffer<uint8_t, RING_BUFFER_SIZE> receive_buffer_;
  /// @brief the buffer where we store temporarily the bytes to transmit
  RingBuffer<uint8_t, RING_BUFFER_SIZE> transmit_buffer_;

#ifdef AUTOTEST_COMPONENT
  /// @brief Test UART in loop mode
  /// @param preamble info to print about the uart address and channel
  void uart_send_test_(char *preamble);
  void uart_receive_test_(char *preamble, bool print_buf = true);
#endif
};

}  // namespace gen_uart
}  // namespace esphome
