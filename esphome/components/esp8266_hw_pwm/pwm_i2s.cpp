#ifdef USE_ESP8266

#include <Arduino.h>
#include <i2s_reg.h>
#include "pwm_i2s.h"

extern "C" {

#define BUF_MAX_LEN (1024)  // Longest possible single buffer is 4095 bytes, but we have resolution up to bit level,
                            // so 1024 byte buffer means 13 bit PWM resolution. Either way it is used in full only
                            // for low frequencies (<5kHz)

typedef struct slc_queue_item {
  uint32_t                blocksize : 12;
  uint32_t                datalen   : 12;
  uint32_t                unused    :  5;
  uint32_t                sub_sof   :  1;
  uint32_t                eof       :  1;
  volatile uint32_t       owner     :  1; // DMA can change this value
  uint8_t *               buf_ptr;
  struct slc_queue_item * next_link_ptr;
} slc_queue_item_t;

static uint32_t buf_len;

static slc_queue_item_t *slc_item = NULL;
static slc_queue_item_t *slc_dummy_item = NULL;

uint8_t sbd_div;
uint8_t scd_div;

#define I2SO_DATA 3
// TODO: don't use other pins
#define I2SO_BCK  15
#define I2SO_WS   2

static bool _alloc() {
  slc_item = (slc_queue_item_t*)calloc(1, sizeof(*slc_item));
  if (!slc_item) {
    return false;
  }
  slc_dummy_item = (slc_queue_item_t*)calloc(1, sizeof(*slc_dummy_item));
  if (!slc_dummy_item) {
    free(slc_item);
    return false;
  }
  slc_item->buf_ptr = (uint8_t *)malloc(BUF_MAX_LEN);
  if (!slc_item->buf_ptr) {
    free(slc_item);
    free(slc_dummy_item);
    return false;
  }
  memset(slc_item->buf_ptr, 0, BUF_MAX_LEN);
  slc_item->unused = 0;
  slc_item->owner = 1;
  slc_item->eof = 1;
  slc_item->sub_sof = 0;
  //slc_item->datalen = buf_len;
  slc_item->blocksize = BUF_MAX_LEN;
  slc_item->next_link_ptr = slc_item;
  return true;
}

static void i2s_slc_begin() {
  ETS_SLC_INTR_DISABLE();
  SLCC0 |= SLCRXLR | SLCTXLR;
  SLCC0 &= ~(SLCRXLR | SLCTXLR);
  SLCIC = 0xFFFFFFFF;

  // Configure DMA
  SLCC0 &= ~(SLCMM << SLCM); // Clear DMA MODE
  SLCC0 |= (1 << SLCM); // Set DMA MODE to 1
  SLCRXDC |= SLCBINR | SLCBTNR; // Enable INFOR_NO_REPLACE and TOKEN_NO_REPLACE
  SLCRXDC &= ~(SLCBRXFE | SLCBRXEM | SLCBRXFM); // Disable RX_FILL, RX_EOF_MODE and RX_FILL_MODE

  //Feed DMA buffer desc addr
  //To send data to the I2S subsystem, counter-intuitively we use the RXLINK part, not the TXLINK as you might
  //expect. The TXLINK part still needs a valid DMA descriptor, even if it's unused: the DMA engine will throw
  //an error at us otherwise. Just feed it any random descriptor.
  SLCTXL &= ~(SLCTXLAM << SLCTXLA); // clear TX descriptor address
  SLCRXL &= ~(SLCRXLAM << SLCRXLA); // clear RX descriptor address
  SLCTXL |= (uint32)slc_dummy_item << SLCTXLA; // Set fake (unused) RX descriptor address
  SLCRXL |= (uint32)slc_item << SLCRXLA; // Set real TX address
  
  ETS_SLC_INTR_ENABLE();

  // Start transmission
  SLCTXL |= SLCTXLS;
  SLCRXL |= SLCRXLS;
}

void i2s_set_dividers(uint8_t div1, uint8_t div2) {
  // Ensure dividers fit in bit fields
  div1 &= I2SBDM;
  div2 &= I2SCDM;

  /*
  Following this post: https://github.com/esp8266/Arduino/issues/2590
  We should reset the transmitter while changing the configuration bits to avoid random distortion.
  */
 
  uint32_t i2sc_temp = I2SC;
  i2sc_temp |= (I2STXR); // Hold transmitter in reset
  I2SC = i2sc_temp;

  // trans master(active low), recv master(active_low), !bits mod(==16 bits/channel), clear clock dividers
  i2sc_temp &= ~(I2STSM | I2SRSM | (I2SBMM << I2SBM) | (I2SBDM << I2SBD) | (I2SCDM << I2SCD));

  // I2SRF = Send/recv right channel first (? may be swapped form I2S spec of WS=0 => left)
  // I2SMR = MSB recv/xmit first
  // I2SRMS, I2STMS = 1-bit delay from WS to MSB (I2S format)
  // div1, div2 = Set I2S WS clock frequency.  BCLK seems to be generated from 32x this
  i2sc_temp |= I2SRF | I2SMR | I2SRMS | I2STMS | (div1 << I2SBD) | (div2 << I2SCD);

  I2SC = i2sc_temp;
  
  i2sc_temp &= ~(I2STXR);  // Release reset
  I2SC = i2sc_temp;
  sbd_div = div1;
  scd_div = div2;
}

float actual_rate(uint8_t a_sbd_div, uint8_t a_scd_div) {
    return (float)I2SBASEFREQ / a_sbd_div / a_scd_div / buf_len / 8;
}

float pwm_get_rate() {
    return actual_rate(sbd_div, scd_div);
}

void pwm_set_rate(uint32_t rate) {  // Rate in Hz
  buf_len = BUF_MAX_LEN;
  uint8_t sbd_div_best=2;
  uint8_t scd_div_best=2;
  float lowest_error = rate - actual_rate(sbd_div_best, scd_div_best);
  if (lowest_error < 0) {  // Requested frequency is lower than the fastest possible with full resolution - we can slow down
    for (uint8_t i=2; i<64; i++) {
      for (uint8_t j=i; j<64; j++) {
        float new_error = rate - actual_rate(i, j);
        // We want closest rate lower than requested (lowest positive error) - we'll speed it up by shortening buffer.
        // If we cannot find lower, we keep lowest of the higher ones.
        if ((lowest_error < 0 && lowest_error < new_error) || (0 < new_error && new_error < lowest_error)) {
            lowest_error = new_error;
            sbd_div_best = i;
            scd_div_best = j;
        }
      }
    }
  }

  i2s_set_dividers(sbd_div_best, scd_div_best);

  // Now we have best frequency lower than the requested one while keeping full resolution.
  // Let's try to improve frequency fit by decreasing resolution.
  buf_len = round(I2SBASEFREQ / sbd_div_best / scd_div_best / rate / 8);
  if (buf_len < 1) {
    buf_len = 1;
  }
  if (buf_len > BUF_MAX_LEN) {
    buf_len = BUF_MAX_LEN;
  }
  slc_item->datalen = buf_len;
}

bool pwm_begin(uint32_t rate)  {
  pinMode(I2SO_DATA, FUNCTION_1);
  //TODO: only data needed
  pinMode(I2SO_WS, FUNCTION_1);
  pinMode(I2SO_BCK, FUNCTION_1);

  if (!_alloc()) {
    return false;
  }

  i2s_slc_begin();
  
  I2S_CLK_ENABLE();
  I2SIC = 0x3F;
  I2SIE = 0;
  
  // Reset I2S
  I2SC &= ~(I2SRST);
  I2SC |= I2SRST;
  I2SC &= ~(I2SRST);
  
  // I2STXFMM, I2SRXFMM=0 => 16-bit, dual channel data shifted in/out
  I2SFC &= ~(I2SDE | (I2STXFMM << I2STXFM) | (I2SRXFMM << I2SRXFM)); // Set RX/TX FIFO_MOD=0 and disable DMA (FIFO only)
  I2SFC |= I2SDE; // Enable DMA

  // I2STXCMM, I2SRXCMM=0 => Dual channel mode
  I2SCC &= ~((I2STXCMM << I2STXCM) | (I2SRXCMM << I2SRXCM)); // Set RX/TX CHAN_MOD=0

  pwm_set_rate(rate);

  I2SC |= I2STXS; // Start transmission

  return true;
}

void pwm_set_level(float level) {
  uint32_t last_on = (uint32_t)(level * buf_len);
  memset(slc_item->buf_ptr, 0, buf_len);
  memset(slc_item->buf_ptr, 0xFF, last_on);
  uint32_t last_on_bit = (uint32_t)(level * buf_len * 8);
  uint8_t bits_remaining = last_on_bit - 8 * last_on;
  if (bits_remaining > 0) {
    slc_item->buf_ptr[last_on] = 0xFF << (8 - bits_remaining);
  }
}

uint32_t pwm_levels() {
  return (buf_len * 8) + 1;
}

};

#endif  // USE_ESP8266
