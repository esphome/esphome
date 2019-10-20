/*
 * I2S Driver
 *
 * Copyright (c) 2019 Yves Bazin
 * Copyright (c) 2019 Samuel Z. Guyer
 * Derived from lots of code examples from other people.
 *
 * The I2S implementation can drive up to 24 strips in parallel, but
 * with the following limitation: all the strips must have the same
 * timing (i.e., they must all use the same chip).
 *
 * To enable the I2S driver, add the following line *before* including
 * FastLED.h (no other changes are necessary):
 *
 * #define FASTLED_ESP32_I2S true
 *
 * The overall strategy is to use the parallel mode of the I2S "audio"
 * peripheral to send up to 24 bits in parallel to 24 different pins.
 * Unlike the RMT peripheral the I2S system cannot send bits of
 * different lengths. Instead, we set the I2S data clock fairly high
 * and then encode a signal as a series of bits. 
 *
 * For example, with a clock divider of 10 the data clock will be
 * 8MHz, so each bit is 125ns. The WS2812 expects a "1" bit to be
 * encoded as a HIGH signal for around 875ns, followed by LOW for
 * 375ns. Sending the following pattern results in the right shape
 * signal:
 *
 *    1111111000        WS2812 "1" bit encoded as 10 125ns pulses
 *
 * The I2S peripheral expects the bits for all 24 outputs to be packed
 * into a single 32-bit word. The complete signal is a series of these
 * 32-bit values -- one for each bit for each strip. The pixel data,
 * however, is stored "serially" as a series of RGB values separately
 * for each strip. To prepare the data we need to do three things: (1)
 * take 1 pixel from each strip, and (2) tranpose the bits so that
 * they are in the parallel form, (3) translate each data bit into the
 * bit pattern that encodes the signal for that bit. This code is in
 * the fillBuffer() method:
 *
 *   1. Read 1 pixel from each strip into an array; store this data by
 *      color channel (e.g., all the red bytes, then all the green
 *      bytes, then all the blue bytes). For three color channels, the
 *      array is 3 X 24 X 8 bits.
 *
 *   2. Tranpose the array so that it is 3 X 8 X 24 bits. The hardware
 *      wants the data in 32-bit chunks, so the actual form is 3 X 8 X
 *      32, with the low 8 bits unused.
 *
 *   3. Take each group of 24 parallel bits and "expand" them into a
 *      pattern according to the encoding. For example, with a 8MHz
 *      data clock, each data bit turns into 10 I2s pulses, so 24
 *      parallel data bits turn into 10 X 24 pulses.
 *
 * We send data to the I2S peripheral using the DMA interface. We use
 * two DMA buffers, so that we can fill one buffer while the other
 * buffer is being sent. Each DMA buffer holds the fully-expanded
 * pulse pattern for one pixel on up to 24 strips. The exact amount of
 * memory required depends on the number of color channels and the
 * number of pulses used to encode each bit.
 *
 * We get an interrupt each time a buffer is sent; we then fill that
 * buffer while the next one is being sent. The DMA interface allows
 * us to configure the buffers as a circularly linked list, so that it
 * can automatically start on the next buffer.
 */
/*
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#pragma message "NOTE: ESP32 support using I2S parallel driver. All strips must use the same chipset"

FASTLED_NAMESPACE_BEGIN

#ifdef __cplusplus
extern "C" {
#endif
    
#include "esp_heap_caps.h"
#include "soc/soc.h"
#include "soc/gpio_sig_map.h"
#include "soc/i2s_reg.h"
#include "soc/i2s_struct.h"
#include "soc/io_mux_reg.h"
#include "driver/gpio.h"
#include "driver/periph_ctrl.h"
#include "rom/lldesc.h"
#include "esp_intr.h"
#include "esp_log.h"
    
#ifdef __cplusplus
}
#endif

__attribute__ ((always_inline)) inline static uint32_t __clock_cycles() {
    uint32_t cyc;
    __asm__ __volatile__ ("rsr %0,ccount":"=a" (cyc));
    return cyc;
}

#define FASTLED_HAS_CLOCKLESS 1
#define NUM_COLOR_CHANNELS 3

// -- Choose which I2S device to use
#ifndef I2S_DEVICE
#define I2S_DEVICE 0
#endif

// -- Max number of controllers we can support
#ifndef FASTLED_I2S_MAX_CONTROLLERS
#define FASTLED_I2S_MAX_CONTROLLERS 24
#endif

// -- I2S clock
#define I2S_BASE_CLK (80000000L)
#define I2S_MAX_CLK (20000000L) //more tha a certain speed and the I2s looses some bits
#define I2S_MAX_PULSE_PER_BIT 20 //put it higher to get more accuracy but it could decrease the refresh rate without real improvement
// -- Convert ESP32 cycles back into nanoseconds
#define ESPCLKS_TO_NS(_CLKS) (((long)(_CLKS) * 1000L) / F_CPU_MHZ)

// -- Array of all controllers
static CLEDController * gControllers[FASTLED_I2S_MAX_CONTROLLERS];
static int gNumControllers = 0;
static int gNumStarted = 0;

// -- Global semaphore for the whole show process
//    Semaphore is not given until all data has been sent
static xSemaphoreHandle gTX_sem = NULL;

// -- One-time I2S initialization
static bool gInitialized = false;

// -- Interrupt handler
static intr_handle_t gI2S_intr_handle = NULL;

// -- A pointer to the memory-mapped structure: I2S0 or I2S1
static i2s_dev_t * i2s;

// -- I2S goes to these pins until we remap them using the GPIO matrix
static int i2s_base_pin_index;

// --- I2S DMA buffers
struct DMABuffer {
    lldesc_t descriptor;
    uint8_t * buffer;
};

#define NUM_DMA_BUFFERS 2
static DMABuffer * dmaBuffers[NUM_DMA_BUFFERS];

// -- Bit patterns
//    For now, we require all strips to be the same chipset, so these
//    are global variables.

static int      gPulsesPerBit = 0;
static uint32_t gOneBit[40] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static uint32_t gZeroBit[40]  = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

// -- Counters to track progress
static int gCurBuffer = 0;
static bool gDoneFilling = false;
static int ones_for_one;
static int ones_for_zero;

// -- Temp buffers for pixels and bits being formatted for DMA
static uint8_t gPixelRow[NUM_COLOR_CHANNELS][32];
static uint8_t gPixelBits[NUM_COLOR_CHANNELS][8][4];
static int CLOCK_DIVIDER_N;
static int CLOCK_DIVIDER_A;
static int CLOCK_DIVIDER_B;

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessController : public CPixelLEDController<RGB_ORDER>
{
    // -- Store the GPIO pin
    gpio_num_t     mPin;
    
    // -- This instantiation forces a check on the pin choice
    FastPin<DATA_PIN> mFastPin;
    
    // -- Save the pixel controller
    PixelController<RGB_ORDER> * mPixels;
    
public:

    void init()
    {
        i2sInit();
        
        // -- Allocate space to save the pixel controller
        //    during parallel output
        mPixels = (PixelController<RGB_ORDER> *) malloc(sizeof(PixelController<RGB_ORDER>));
        
        gControllers[gNumControllers] = this;
        int my_index = gNumControllers;
        gNumControllers++;
        
        // -- Set up the pin We have to do two things: configure the
        //    actual GPIO pin, and route the output from the default
        //    pin (determined by the I2S device) to the pin we
        //    want. We compute the default pin using the index of this
        //    controller in the array. This order is crucial because
        //    the bits must go into the DMA buffer in the same order.
        mPin = gpio_num_t(DATA_PIN);
        
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[DATA_PIN], PIN_FUNC_GPIO);
        gpio_set_direction(mPin, (gpio_mode_t)GPIO_MODE_DEF_OUTPUT);
        pinMode(mPin,OUTPUT);
        gpio_matrix_out(mPin, i2s_base_pin_index + my_index, false, false);
    }
    
    virtual uint16_t getMaxRefreshRate() const { return 400; }
    
protected:
   
   static int pgcd(int smallest,int precision,int a,int b,int c)
    {
        int pgc_=1;
        for( int i=smallest;i>0;i--)
        {
            
            if( a%i<=precision && b%i<=precision && c%i<=precision)
            {
                pgc_=i;
                break;
            }
        }
        return pgc_;
    }
    
    /** Compute pules/bit patterns
     *
     *  This is Yves Bazin's mad code for computing the pulse pattern
     *  and clock timing given the target signal given by T1, T2, and
     *  T3. In general, these parameters are interpreted as follows:
     *
     *  a "1" bit is encoded by setting the pin HIGH to T1+T2 ns, then LOW for T3 ns
     *  a "0" bit is encoded by setting the pin HIGH to T1 ns, then LOW for T2+T3 ns
     *
     */
    static void initBitPatterns()
    {
        // Precompute the bit patterns based on the I2S sample rate
        // Serial.println("Setting up fastled using I2S");

        // -- First, convert back to ns from CPU clocks
        uint32_t T1ns = ESPCLKS_TO_NS(T1);
        uint32_t T2ns = ESPCLKS_TO_NS(T2);
        uint32_t T3ns = ESPCLKS_TO_NS(T3);
        
        // Serial.print("T1 = "); Serial.print(T1); Serial.print(" ns "); Serial.println(T1ns);
        // Serial.print("T2 = "); Serial.print(T2); Serial.print(" ns "); Serial.println(T2ns);
        // Serial.print("T3 = "); Serial.print(T3); Serial.print(" ns "); Serial.println(T3ns);
        
        /*
         We calculate the best pcgd to the timing
         ie
         WS2811 77 77 154 => 1  1 2 => nb pulses= 4
         WS2812 60 150 90 => 2 5 3 => nb pulses=10
         */
        int smallest=0;
        if (T1>T2)
            smallest=T2;
        else
            smallest=T1;
        if(smallest>T3)
            smallest=T3;
        double freq=(double)1/(double)(T1ns + T2ns + T3ns);
        // Serial.printf("chipset frequency:%f Khz\n", 1000000L*freq);
       // Serial.printf("smallest %d\n",smallest);
        int pgc_=1;
        int precision=0;
        pgc_=pgcd(smallest,precision,T1,T2,T3);
        //Serial.printf("%f\n",I2S_MAX_CLK/(1000000000L*freq));
        while(pgc_==1 ||  (T1/pgc_ +T2/pgc_ +T3/pgc_)>I2S_MAX_PULSE_PER_BIT) //while(pgc_==1 ||  (T1/pgc_ +T2/pgc_ +T3/pgc_)>I2S_MAX_CLK/(1000000000L*freq))
        {
            precision++;
            pgc_=pgcd(smallest,precision,T1,T2,T3);
            //Serial.printf("%d %d\n",pgc_,(a+b+c)/pgc_);
        }
        pgc_=pgcd(smallest,precision,T1,T2,T3);
        // Serial.printf("pgcd %d precision:%d\n",pgc_,precision);
        // Serial.printf("nb pulse per bit:%d\n",T1/pgc_ +T2/pgc_ +T3/pgc_);
        gPulsesPerBit=(int)T1/pgc_ +(int)T2/pgc_ +(int)T3/pgc_;
        /*
         we calculate the duration of one pulse nd htre base frequency of the led
         ie WS2812B F=1/(250+625+375)=800kHz or 1250ns
         as we need 10 pulses each pulse is 125ns => frequency 800Khz*10=8MHz
         WS2811 T=320+320+641=1281ns qnd we need 4 pulses => pulse duration 320.25ns =>frequency 3.1225605Mhz
         
         */

        freq=1000000000L*freq*gPulsesPerBit;
        // Serial.printf("needed frequency (nbpiulse per bit)*(chispset frequency):%f Mhz\n",freq/1000000);
        
        /*
         we do calculate the needed N a and b
         as f=basefred/(N+b/a);
         as a is max 63 the precision for the decimal is 1/63
         
         */
        
         CLOCK_DIVIDER_N=(int)((double)I2S_BASE_CLK/freq);
        double v=I2S_BASE_CLK/freq-CLOCK_DIVIDER_N;

        double prec=(double)1/63;
        int a=1;
        int b=0;
        CLOCK_DIVIDER_A=1;
        CLOCK_DIVIDER_B=0;
        for(a=1;a<64;a++)
        {
            for(b=0;b<a;b++)
            {
                //printf("%d %d %f %f %f\n",b,a,v,(double)v*(double)a,fabsf(v-(double)b/a));
                if(fabsf(v-(double)b/a) <= prec/2)
                    break;
            }
            if(fabsf(v-(double)b/a) ==0)
            {
                CLOCK_DIVIDER_A=a;
                CLOCK_DIVIDER_B=b;
                break;
            }
            if(fabsf(v-(double)b/a) < prec/2)
            {
                if (fabsf(v-(double)b/a) <fabsf(v-(double)CLOCK_DIVIDER_B/CLOCK_DIVIDER_A))
                {
                    CLOCK_DIVIDER_A=a;
                    CLOCK_DIVIDER_B=b;
                }
                
            }
        }
        //top take care of an issue with double 0.9999999999
        if(CLOCK_DIVIDER_A==CLOCK_DIVIDER_B)
        {
            CLOCK_DIVIDER_A=1;
            CLOCK_DIVIDER_B=0;
            CLOCK_DIVIDER_N++;
        }
        
        //printf("%d %d %f %f %d\n",CLOCK_DIVIDER_B,CLOCK_DIVIDER_A,(double)CLOCK_DIVIDER_B/CLOCK_DIVIDER_A,v,CLOCK_DIVIDER_N);
        //Serial.printf("freq %f %f\n",freq,I2S_BASE_CLK/(CLOCK_DIVIDER_N+(double)CLOCK_DIVIDER_B/CLOCK_DIVIDER_A));
        freq=1/(CLOCK_DIVIDER_N+(double)CLOCK_DIVIDER_B/CLOCK_DIVIDER_A);
        freq=freq*I2S_BASE_CLK;
        // Serial.printf("calculted for i2s frequency:%f Mhz N:%d B:%d A:%d\n",freq/1000000,CLOCK_DIVIDER_N,CLOCK_DIVIDER_B,CLOCK_DIVIDER_A);
        double pulseduration=1000000000/freq;
        // Serial.printf("Pulse duration: %f ns\n",pulseduration);
        // gPulsesPerBit = (T1ns + T2ns + T3ns)/FASTLED_I2S_NS_PER_PULSE;
        
        //Serial.print("Pulses per bit: "); Serial.println(gPulsesPerBit);
        
        //int ones_for_one  = ((T1ns + T2ns - 1)/FASTLED_I2S_NS_PER_PULSE) + 1;
        ones_for_one  = T1/pgc_ +T2/pgc_;
        //Serial.print("One bit:  target ");
        //Serial.print(T1ns+T2ns); Serial.print("ns --- ");
        //Serial.print(ones_for_one); Serial.print(" 1 bits");
        //Serial.print(" = "); Serial.print(ones_for_one * FASTLED_I2S_NS_PER_PULSE); Serial.println("ns");
        // Serial.printf("one bit : target %d  ns --- %d  pulses 1 bit = %f ns\n",T1ns+T2ns,ones_for_one ,ones_for_one*pulseduration);
        
        
        int i = 0;
        while ( i < ones_for_one ) {
            gOneBit[i] = 0xFFFFFF00;
            i++;
        }
        while ( i < gPulsesPerBit ) {
            gOneBit[i] = 0x00000000;
            i++;
        }
        
        //int ones_for_zero = ((T1ns - 1)/FASTLED_I2S_NS_PER_PULSE) + 1;
        ones_for_zero =T1/pgc_  ;
       // Serial.print("Zero bit:  target ");
       // Serial.print(T1ns); Serial.print("ns --- ");
        //Serial.print(ones_for_zero); Serial.print(" 1 bits");
        //Serial.print(" = "); Serial.print(ones_for_zero * FASTLED_I2S_NS_PER_PULSE); Serial.println("ns");
        // Serial.printf("Zero bit : target %d ns --- %d pulses  1 bit =   %f ns\n",T1ns,ones_for_zero ,ones_for_zero*pulseduration);
        i = 0;
        while ( i < ones_for_zero ) {
            gZeroBit[i] = 0xFFFFFF00;
            i++;
        }
        while ( i < gPulsesPerBit ) {
            gZeroBit[i] = 0x00000000;
            i++;
        }
        
        memset(gPixelRow, 0, NUM_COLOR_CHANNELS * 32);
        memset(gPixelBits, 0, NUM_COLOR_CHANNELS * 32);
    }
    
    static DMABuffer * allocateDMABuffer(int bytes)
    {
        DMABuffer * b = (DMABuffer *)heap_caps_malloc(sizeof(DMABuffer), MALLOC_CAP_DMA);
        
        b->buffer = (uint8_t *)heap_caps_malloc(bytes, MALLOC_CAP_DMA);
        memset(b->buffer, 0, bytes);
        
        b->descriptor.length = bytes;
        b->descriptor.size = bytes;
        b->descriptor.owner = 1;
        b->descriptor.sosf = 1;
        b->descriptor.buf = b->buffer;
        b->descriptor.offset = 0;
        b->descriptor.empty = 0;
        b->descriptor.eof = 1;
        b->descriptor.qe.stqe_next = 0;
        
        return b;
    }
    
    static void i2sInit()
    {
        // -- Only need to do this once
        if (gInitialized) return;
        
        // -- Construct the bit patterns for ones and zeros
        initBitPatterns();
        
        // -- Choose whether to use I2S device 0 or device 1
        //    Set up the various device-specific parameters
        int interruptSource;
        if (I2S_DEVICE == 0) {
            i2s = &I2S0;
            periph_module_enable(PERIPH_I2S0_MODULE);
            interruptSource = ETS_I2S0_INTR_SOURCE;
            i2s_base_pin_index = I2S0O_DATA_OUT0_IDX;
        } else {
            i2s = &I2S1;
            periph_module_enable(PERIPH_I2S1_MODULE);
            interruptSource = ETS_I2S1_INTR_SOURCE;
            i2s_base_pin_index = I2S1O_DATA_OUT0_IDX;
        }
        
        // -- Reset everything
        i2sReset();
        i2sReset_DMA();
        i2sReset_FIFO();
        
        // -- Main configuration
        i2s->conf.tx_msb_right = 1;
        i2s->conf.tx_mono = 0;
        i2s->conf.tx_short_sync = 0;
        i2s->conf.tx_msb_shift = 0;
        i2s->conf.tx_right_first = 1; // 0;//1;
        i2s->conf.tx_slave_mod = 0;
        
        // -- Set parallel mode
        i2s->conf2.val = 0;
        i2s->conf2.lcd_en = 1;
        i2s->conf2.lcd_tx_wrx2_en = 0; // 0 for 16 or 32 parallel output
        i2s->conf2.lcd_tx_sdx2_en = 0; // HN
        
        // -- Set up the clock rate and sampling
        i2s->sample_rate_conf.val = 0;
        i2s->sample_rate_conf.tx_bits_mod = 32; // Number of parallel bits/pins
        i2s->sample_rate_conf.tx_bck_div_num = 1;
        i2s->clkm_conf.val = 0;
        i2s->clkm_conf.clka_en = 0;
        
        // -- Data clock is computed as Base/(div_num + (div_b/div_a))
        //    Base is 80Mhz, so 80/(10 + 0/1) = 8Mhz
        //    One cycle is 125ns
        i2s->clkm_conf.clkm_div_a = CLOCK_DIVIDER_A;
        i2s->clkm_conf.clkm_div_b = CLOCK_DIVIDER_B;
        i2s->clkm_conf.clkm_div_num = CLOCK_DIVIDER_N;
        
        i2s->fifo_conf.val = 0;
        i2s->fifo_conf.tx_fifo_mod_force_en = 1;
        i2s->fifo_conf.tx_fifo_mod = 3;  // 32-bit single channel data
        i2s->fifo_conf.tx_data_num = 32; // fifo length
        i2s->fifo_conf.dscr_en = 1;      // fifo will use dma
        
        i2s->conf1.val = 0;
        i2s->conf1.tx_stop_en = 0;
        i2s->conf1.tx_pcm_bypass = 1;
        
        i2s->conf_chan.val = 0;
        i2s->conf_chan.tx_chan_mod = 1; // Mono mode, with tx_msb_right = 1, everything goes to right-channel
        
        i2s->timing.val = 0;
        
        // -- Allocate two DMA buffers
        dmaBuffers[0] = allocateDMABuffer(32 * NUM_COLOR_CHANNELS * gPulsesPerBit);
        dmaBuffers[1] = allocateDMABuffer(32 * NUM_COLOR_CHANNELS * gPulsesPerBit);
        
        // -- Arrange them as a circularly linked list
        dmaBuffers[0]->descriptor.qe.stqe_next = &(dmaBuffers[1]->descriptor);
        dmaBuffers[1]->descriptor.qe.stqe_next = &(dmaBuffers[0]->descriptor);
       
        // -- Allocate i2s interrupt
        SET_PERI_REG_BITS(I2S_INT_ENA_REG(I2S_DEVICE), I2S_OUT_EOF_INT_ENA_V, 1, I2S_OUT_EOF_INT_ENA_S);
        esp_err_t e = esp_intr_alloc(interruptSource, 0, // ESP_INTR_FLAG_INTRDISABLED | ESP_INTR_FLAG_LEVEL3,
                                     &interruptHandler, 0, &gI2S_intr_handle);
        
        // -- Create a semaphore to block execution until all the controllers are done
        if (gTX_sem == NULL) {
            gTX_sem = xSemaphoreCreateBinary();
            xSemaphoreGive(gTX_sem);
        }
        
        // Serial.println("Init I2S");
        gInitialized = true;
    }
    
    /** Clear DMA buffer
     *
     *  Yves' clever trick: initialize the bits that we know must be 0
     *  or 1 regardless of what bit they encode.
     */
    static void empty( uint32_t *buf)
    {
        for(int i=0;i<8*NUM_COLOR_CHANNELS;i++)
        {
            int offset=gPulsesPerBit*i;
            for(int j=0;j<ones_for_zero;j++)
                buf[offset+j]=0xffffffff;
            
            for(int j=ones_for_one;j<gPulsesPerBit;j++)
                buf[offset+j]=0;
        }
    }
    
    // -- Show pixels
    //    This is the main entry point for the controller.
    virtual void showPixels(PixelController<RGB_ORDER> & pixels)
    {
        if (gNumStarted == 0) {
            // -- First controller: make sure everything is set up
            xSemaphoreTake(gTX_sem, portMAX_DELAY);
        }
        
        // -- Initialize the local state, save a pointer to the pixel
        //    data. We need to make a copy because pixels is a local
        //    variable in the calling function, and this data structure
        //    needs to outlive this call to showPixels.
        (*mPixels) = pixels;
        
        // -- Keep track of the number of strips we've seen
        gNumStarted++;

        // Serial.print("Show pixels ");
        // Serial.println(gNumStarted);
        
        // -- The last call to showPixels is the one responsible for doing
        //    all of the actual work
        if (gNumStarted == gNumControllers) {
            empty((uint32_t*)dmaBuffers[0]->buffer);
            empty((uint32_t*)dmaBuffers[1]->buffer);
            gCurBuffer = 0;
            gDoneFilling = false;
            
            // -- Prefill both buffers
            fillBuffer();
            fillBuffer();
            
            i2sStart();
            
            // -- Wait here while the rest of the data is sent. The interrupt handler
            //    will keep refilling the DMA buffers until it is all sent; then it
            //    gives the semaphore back.
            xSemaphoreTake(gTX_sem, portMAX_DELAY);
            xSemaphoreGive(gTX_sem);
            
            i2sStop();
            
            // -- Reset the counters
            gNumStarted = 0;
        }
    }
    
    // -- Custom interrupt handler
    static IRAM_ATTR void interruptHandler(void *arg)
    {
        if (i2s->int_st.out_eof) {
            i2s->int_clr.val = i2s->int_raw.val;
            
            if ( ! gDoneFilling) {
                fillBuffer();
            } else {
                portBASE_TYPE HPTaskAwoken = 0;
                xSemaphoreGiveFromISR(gTX_sem, &HPTaskAwoken);
                if(HPTaskAwoken == pdTRUE) portYIELD_FROM_ISR();
            }
        }
    }
    
    /** Fill DMA buffer
     *
     *  This is where the real work happens: take a row of pixels (one
     *  from each strip), transpose and encode the bits, and store
     *  them in the DMA buffer for the I2S peripheral to read.
     */
    static void fillBuffer()
    {
        // -- Alternate between buffers
        volatile uint32_t * buf = (uint32_t *) dmaBuffers[gCurBuffer]->buffer;
        gCurBuffer = (gCurBuffer + 1) % NUM_DMA_BUFFERS;
        
        // -- Get the requested pixel from each controller. Store the
        //    data for each color channel in a separate array.
        uint32_t has_data_mask = 0;
        for (int i = 0; i < gNumControllers; i++) {
            // -- Store the pixels in reverse controller order starting at index 23
            //    This causes the bits to come out in the right position after we
            //    transpose them.
            int bit_index = 23-i;
            ClocklessController * pController = static_cast<ClocklessController*>(gControllers[i]);
            if (pController->mPixels->has(1)) {
                gPixelRow[0][bit_index] = pController->mPixels->loadAndScale0();
                gPixelRow[1][bit_index] = pController->mPixels->loadAndScale1();
                gPixelRow[2][bit_index] = pController->mPixels->loadAndScale2();
                pController->mPixels->advanceData();
                pController->mPixels->stepDithering();
                
                // -- Record that this controller still has data to send
                has_data_mask |= (1 << (i+8));
            }
        }
        
        // -- None of the strips has data? We are done.
        if (has_data_mask == 0) {
            gDoneFilling = true;
            return;
        }
        
        // -- Transpose and encode the pixel data for the DMA buffer
        int buf_index = 0;
        for (int channel = 0; channel < NUM_COLOR_CHANNELS; channel++) {
            
            // -- Tranpose each array: all the bit 7's, then all the bit 6's, ...
            transpose32(gPixelRow[channel], gPixelBits[channel][0] );
            
            //Serial.print("Channel: "); Serial.print(channel); Serial.print(" ");
            for (int bitnum = 0; bitnum < 8; bitnum++) {
                uint8_t * row = (uint8_t *) (gPixelBits[channel][bitnum]);
                uint32_t bit = (row[0] << 24) | (row[1] << 16) | (row[2] << 8) | row[3];
                
               /* SZG: More general, but too slow:
                    for (int pulse_num = 0; pulse_num < gPulsesPerBit; pulse_num++) {
                        buf[buf_index++] = has_data_mask & ( (bit & gOneBit[pulse_num]) | (~bit & gZeroBit[pulse_num]) );
                     }
               */

                // -- Only fill in the pulses that are different between the "0" and "1" encodings
                for(int pulse_num = ones_for_zero; pulse_num < ones_for_one; pulse_num++) {
                    buf[bitnum*gPulsesPerBit+channel*8*gPulsesPerBit+pulse_num] = has_data_mask & bit;
                }
            }
        }
    }
    
    static void transpose32(uint8_t * pixels, uint8_t * bits)
    {
        transpose8rS32(& pixels[0],  1, 4, & bits[0]);
        transpose8rS32(& pixels[8],  1, 4, & bits[1]);
        transpose8rS32(& pixels[16], 1, 4, & bits[2]);
        //transpose8rS32(& pixels[24], 1, 4, & bits[3]);  Can only use 24 bits
    }
    
    /** Transpose 8x8 bit matrix
     *  From Hacker's Delight
     */
    static void transpose8rS32(uint8_t * A, int m, int n, uint8_t * B)
    {
        uint32_t x, y, t;
        
        // Load the array and pack it into x and y.
        
        x = (A[0]<<24)   | (A[m]<<16)   | (A[2*m]<<8) | A[3*m];
        y = (A[4*m]<<24) | (A[5*m]<<16) | (A[6*m]<<8) | A[7*m];
        
        t = (x ^ (x >> 7)) & 0x00AA00AA;  x = x ^ t ^ (t << 7);
        t = (y ^ (y >> 7)) & 0x00AA00AA;  y = y ^ t ^ (t << 7);
        
        t = (x ^ (x >>14)) & 0x0000CCCC;  x = x ^ t ^ (t <<14);
        t = (y ^ (y >>14)) & 0x0000CCCC;  y = y ^ t ^ (t <<14);
        
        t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
        y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
        x = t;
        
        B[0]=x>>24;    B[n]=x>>16;    B[2*n]=x>>8;  B[3*n]=x;
        B[4*n]=y>>24;  B[5*n]=y>>16;  B[6*n]=y>>8;  B[7*n]=y;
    }
    
    /** Start I2S transmission
     */
    static void i2sStart()
    {
        // esp_intr_disable(gI2S_intr_handle);
        // Serial.println("I2S start");
        i2sReset();
        //Serial.println(dmaBuffers[0]->sampleCount());
        i2s->lc_conf.val=I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN | I2S_OUT_DATA_BURST_EN;
        i2s->out_link.addr = (uint32_t) & (dmaBuffers[0]->descriptor);
        i2s->out_link.start = 1;
        ////vTaskDelay(5);
        i2s->int_clr.val = i2s->int_raw.val;
        // //vTaskDelay(5);
        i2s->int_ena.out_dscr_err = 1;
        //enable interrupt
        ////vTaskDelay(5);
        esp_intr_enable(gI2S_intr_handle);
        // //vTaskDelay(5);
        i2s->int_ena.val = 0;
        i2s->int_ena.out_eof = 1;
        
        //start transmission
        i2s->conf.tx_start = 1;
    }
    
    static void i2sReset()
    {
        // Serial.println("I2S reset");
        const unsigned long lc_conf_reset_flags = I2S_IN_RST_M | I2S_OUT_RST_M | I2S_AHBM_RST_M | I2S_AHBM_FIFO_RST_M;
        i2s->lc_conf.val |= lc_conf_reset_flags;
        i2s->lc_conf.val &= ~lc_conf_reset_flags;
        
        const uint32_t conf_reset_flags = I2S_RX_RESET_M | I2S_RX_FIFO_RESET_M | I2S_TX_RESET_M | I2S_TX_FIFO_RESET_M;
        i2s->conf.val |= conf_reset_flags;
        i2s->conf.val &= ~conf_reset_flags;
    }
    
    static void i2sReset_DMA()
    {
        i2s->lc_conf.in_rst=1; i2s->lc_conf.in_rst=0;
        i2s->lc_conf.out_rst=1; i2s->lc_conf.out_rst=0;
    }
    
    static void i2sReset_FIFO()
    {
        i2s->conf.rx_fifo_reset=1; i2s->conf.rx_fifo_reset=0;
        i2s->conf.tx_fifo_reset=1; i2s->conf.tx_fifo_reset=0;
    }
    
    static void i2sStop()
    {
        // Serial.println("I2S stop");
        esp_intr_disable(gI2S_intr_handle);
        i2sReset();
        i2s->conf.rx_start = 0;
        i2s->conf.tx_start = 0;
    }
};

FASTLED_NAMESPACE_END
