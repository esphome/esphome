#pragma once
#include "esphome/core/application.h"

extern "C" {
#include "spi_flash.h"
}
namespace esphome {
namespace waveshare_epaper {

void wavesh_log(const char*, ...);

template<unsigned int id, size_t N, size_t buffer_size_bytes=SPI_FLASH_SEC_SIZE>
class BufferedFlashArray{
public:
  using aligned_type = uint32_t;
  static_assert(
    N % sizeof(aligned_type)==0,
    "array size should be a multiple of sizof(aligned_type)");
  static_assert(
    buffer_size_bytes % sizeof(aligned_type)==0,
    "small buffer size should be a multiple of sizof(aligned_type)");
  BufferedFlashArray(){
    fetch_new_start(0);
  }

  struct Access{
    BufferedFlashArray& arr;
    int index;
    HOT Access& operator=(const uint8_t& v){
      arr.write(index, v);
      return *this;
    }
    HOT Access& operator&=(const uint8_t& v){
      return operator=(arr.read(index) & v);
    }
    HOT Access& operator|=(const uint8_t& v){
      return operator=(arr.read(index) | v);
    }
    operator uint8_t() {
      return arr.read(index);
    }
  };
  Access operator[](int i){
    return {*this, i};
  }
  void fill(uint8_t v){
    for(size_t i=0; i<N; i+=buffer_size_bytes){
      fetch_new_start(i);
      memset(small_buffer, buffer_size_bytes, v);
      App.feed_wdt();
    }
  }
  uint8_t read(int i);
  void write(uint32_t i, const uint8_t& v);
  void commit();
private:
  int fetch_new_start(int i);
  int cached_start(uint32_t i){
    constexpr auto buffer_size_bytes_bytes = buffer_size_bytes;
    const auto start = reinterpret_cast<int>(small_buffer_start) - reinterpret_cast<int>(arr);
    const auto end = start + buffer_size_bytes_bytes;
    if(i >=start && i < end)
      return start;
    // wavesh_log("idx %x not in (%x, %x)", i, start, end);
    return fetch_new_start(i);
  }
  uint8_t* small_buffer_start{arr};
  uint8_t small_buffer[buffer_size_bytes];
  bool dirty{false};
  static constexpr uint8_t* arr= reinterpret_cast<uint8_t*>(0x100000 + id); //[N/sizeof(aligned_type)];
  static_assert(
    (reinterpret_cast<aligned_type>(arr) % SPI_FLASH_SEC_SIZE) == 0,
    "arr should be aligned to sector");
};

template<unsigned int id, size_t N, size_t buffer_size_bytes>
void
BufferedFlashArray<id, N, buffer_size_bytes>::commit(){
  while(dirty){
    // wavesh_log(" dirty %x from %x", (aligned_type)(arr), (aligned_type)(small_buffer_start) );
    noInterrupts();
    auto res = spi_flash_erase_sector(
      reinterpret_cast<aligned_type>(small_buffer_start)/SPI_FLASH_SEC_SIZE);
    if(res == SPI_FLASH_RESULT_OK){
      res = spi_flash_write(
        reinterpret_cast<aligned_type>(small_buffer_start),
        reinterpret_cast<uint32_t*>(small_buffer), buffer_size_bytes
      );
      if(res == SPI_FLASH_RESULT_OK){
        dirty = false;
        interrupts();
        break;
      }
    }
    // wavesh_log(" fail write %d", res);
    interrupts();
  }
  // wavesh_log(  " ok");
}

template<unsigned int id, size_t N, size_t buffer_size_bytes>
HOT uint8_t
BufferedFlashArray<id, N, buffer_size_bytes>::read(int i){
  //wavesh_log(  "[%d]", i);
  if(i > N || i<0)  return 0;
  return ((uint8_t*)small_buffer)[i - cached_start(i)];
}

template<unsigned int id, size_t N, size_t buffer_size_bytes>
HOT void
BufferedFlashArray<id, N, buffer_size_bytes>::write(uint32_t i, const uint8_t& v){
  //wavesh_log(  "[0x%x] = %02x ", i, (uint32_t)v);
  if(i > N || i<0)  return;
  ((uint8_t*)small_buffer)[i - cached_start(i)] = v;
  dirty = true;
}

template<unsigned int id, size_t N, size_t buffer_size_bytes>
int
BufferedFlashArray<id, N, buffer_size_bytes>::fetch_new_start(int i){
  commit();
  small_buffer_start = reinterpret_cast<uint8_t*>(
    reinterpret_cast<aligned_type>(arr) + ((i/buffer_size_bytes)*buffer_size_bytes));
  while(true){
    noInterrupts();
    auto res = spi_flash_read(
      reinterpret_cast<aligned_type>(small_buffer_start),
      reinterpret_cast<uint32_t*>(small_buffer), buffer_size_bytes);
    interrupts();
    if(res==SPI_FLASH_RESULT_OK)break;
  };
  // wavesh_log("fetch_buffer_start@0x%x for 0x%x ",
  //  reinterpret_cast<aligned_type>(small_buffer_start), i);
  return (small_buffer_start - arr);
}

} // namespace waveshare_epaper
} // namespace esphome
