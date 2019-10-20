#ifndef __FASTSPI_ARM_NRF52_H
#define __FASTSPI_ARM_NRF52_H


#ifndef FASTLED_FORCE_SOFTWARE_SPI

    #include <nrf_spim.h>

    #define FASTLED_ALL_PINS_HARDWARE_SPI


    // NRF52810 has SPIM0: Frequencies from 125kbps to 8Mbps
    // NRF52832 adds SPIM1, SPIM2 (same frequencies)
    // NRF52840 adds SPIM3 (same frequencies), adds SPIM3 that can be @ up to 32Mbps frequency(!)
    #if !defined(FASTLED_NRF52_SPIM)
        #define FASTLED_NRF52_SPIM   NRF_SPIM0
    #endif

    /* This class is slightly simpler than fastpin, as it can rely on fastpin
     * to handle the mapping to the underlying PN.XX board-level pins...
     */

    /// SPI_CLOCK_DIVIDER is number of CPU clock cycles per SPI transmission bit?
    template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
    class NRF52SPIOutput {

    private:
        // static variables -- always using same SPIM instance
        static bool s_InUse;
        static bool s_NeedToWait; // a data transfer was started, and completion event was not cleared.

        /*
        // TODO -- Workaround nRF52840 errata #198, which relates to
        //         contention between SPIM3 and CPU over AHB.
        //         The workaround is to ensure the SPIM TX buffer
        //         is on a different / dedicated RAM block.
        //         This also avoids AHB contention generally, so
        //         should be applied to all supported boards.
        //         
        //         But... how to allocate m_Buffer[] to be at a
        //         specific memory range?  Also, might need to
        //         avoid use of single-transaction writeBytes()
        //         as cannot control where that memory lies....
        */
        static uint8_t  s_BufferIndex;
        static uint8_t  s_Buffer[2][2]; // 2x two-byte buffers, allows one buffer currently being sent, and a second one being prepped to send.

        // This allows saving the configuration of the SPIM instance
        // upon select(), and restoring the configuration upon release().
        struct spim_config {
            uint32_t inten;
            uint32_t shorts;
            uint32_t sck_pin;
            uint32_t mosi_pin;
            uint32_t miso_pin;
            uint32_t frequency;
            // data pointers, RX/TX counts not saved as would only hide bugs
            uint32_t config; // mode & bit order
            uint32_t orc;

#if false // additional configuration to save/restore for SPIM3
            uint32_t csn_pin;
            uint32_t csn_polarity; // CSNPOL
            uint32_t csn_duration; // IFTIMING.CSNDUR
            uint32_t rx_delay;     // IFTIMING.RXDELAY
            uint32_t dcx_pin;      // PSELDCX
            uint32_t dcx_config;   // DCXCNT
#endif

        } m_SpiSavedConfig;
        void saveSpimConfig() {
            m_SpiSavedConfig.inten          = FASTLED_NRF52_SPIM->INTENSET;
            m_SpiSavedConfig.shorts         = FASTLED_NRF52_SPIM->SHORTS;
            m_SpiSavedConfig.sck_pin        = FASTLED_NRF52_SPIM->PSEL.SCK;
            m_SpiSavedConfig.mosi_pin       = FASTLED_NRF52_SPIM->PSEL.MOSI;
            m_SpiSavedConfig.miso_pin       = FASTLED_NRF52_SPIM->PSEL.MISO;
            m_SpiSavedConfig.frequency      = FASTLED_NRF52_SPIM->FREQUENCY;
            m_SpiSavedConfig.config         = FASTLED_NRF52_SPIM->CONFIG;
            m_SpiSavedConfig.orc            = FASTLED_NRF52_SPIM->ORC;

#if false // additional configuration to save/restore for SPIM3
            m_SpiSavedConfig.csn_pin        = FASTLED_NRF52_SPIM->PSEL.CSN;
            m_SpiSavedConfig.csn_polarity   = FASTLED_NRF52_SPIM->CSNPOL;
            m_SpiSavedConfig.csn_duration   = FASTLED_NRF52_SPIM->IFTIMING.CSNDUR;
            m_SpiSavedConfig.dcx_pin        = FASTLED_NRF52_SPIM->PSELDCX;
            m_SpiSavedConfig.dcx_config     = FASTLED_NRF52_SPIM->DCXCNT;
#endif
        }
        void restoreSpimConfig() {
            // 0. ASSERT() the SPIM instance is not enabled

            FASTLED_NRF52_SPIM->INTENCLR        = 0xFFFFFFFF;
            FASTLED_NRF52_SPIM->INTENSET        = m_SpiSavedConfig.inten;
            FASTLED_NRF52_SPIM->SHORTS          = m_SpiSavedConfig.shorts;
            FASTLED_NRF52_SPIM->PSEL.SCK        = m_SpiSavedConfig.sck_pin;
            FASTLED_NRF52_SPIM->PSEL.MOSI       = m_SpiSavedConfig.mosi_pin;
            FASTLED_NRF52_SPIM->PSEL.MISO       = m_SpiSavedConfig.miso_pin;
            FASTLED_NRF52_SPIM->FREQUENCY       = m_SpiSavedConfig.frequency;
            FASTLED_NRF52_SPIM->CONFIG          = m_SpiSavedConfig.config;
            FASTLED_NRF52_SPIM->ORC             = m_SpiSavedConfig.orc;

#if false // additional configuration to save/restore for SPIM3
            FASTLED_NRF52_SPIM->PSEL.CSN        = m_SpiSavedConfig.csn_pin;
            FASTLED_NRF52_SPIM->CSNPOL          = m_SpiSavedConfig.csn_polarity;
            FASTLED_NRF52_SPIM->IFTIMING.CSNDUR = m_SpiSavedConfig.csn_duration;
            FASTLED_NRF52_SPIM->PSELDCX         = m_SpiSavedConfig.dcx_pin;
            FASTLED_NRF52_SPIM->DCXCNT          = m_SpiSavedConfig.dcx_config;
#endif
        }

    public:
        NRF52SPIOutput() {}

        // Low frequency GPIO is for signals with a frequency up to 10 kHz.  Lowest speed SPIM is 125kbps.
        static_assert(!FastPin<_DATA_PIN>::LowSpeedOnlyRecommended(),  "Invalid (low-speed only) pin specified");
        static_assert(!FastPin<_CLOCK_PIN>::LowSpeedOnlyRecommended(), "Invalid (low-speed only) pin specified");

        /// initialize the SPI subssytem
        void init() {
            // 0. ASSERT() the SPIM instance is not enabled / in use
            //ASSERT(m_SPIM->ENABLE != (SPIM_ENABLE_ENABLE_Enabled << SPIM_ENABLE_ENABLE_Pos));

            // 1. set pins to output/H0H1 drive/etc.
            FastPin<_DATA_PIN>::setOutput();
            FastPin<_CLOCK_PIN>::setOutput();

            // 2. Configure SPIMx
            nrf_spim_configure(
                FASTLED_NRF52_SPIM,
                NRF_SPIM_MODE_0,
                NRF_SPIM_BIT_ORDER_MSB_FIRST
                );
            nrf_spim_frequency_set(
                FASTLED_NRF52_SPIM,
                NRF_SPIM_FREQ_4M // BUGBUG -- use _SPI_CLOCK_DIVIDER to determine frequency
                );
            nrf_spim_pins_set(
                FASTLED_NRF52_SPIM,
                FastPin<_CLOCK_PIN>::nrf_pin(),
                FastPin<_DATA_PIN>::nrf_pin(),
                NRF_SPIM_PIN_NOT_CONNECTED
                );

            // 4. Ensure events are cleared
            nrf_spim_event_clear(FASTLED_NRF52_SPIM, NRF_SPIM_EVENT_END);
            nrf_spim_event_clear(FASTLED_NRF52_SPIM, NRF_SPIM_EVENT_STARTED);

            // 5. Enable the SPIM instance
            nrf_spim_enable(FASTLED_NRF52_SPIM);
        }

        /// latch the CS select
        void select() {
            //ASSERT(!s_InUse);
            saveSpimConfig();
            s_InUse = true;
            init();
        }

        /// release the CS select
        void release() {
            //ASSERT(s_InUse);
            waitFully();
            s_InUse = false;
            restoreSpimConfig();
        }

        /// wait until all queued up data has been written
        static void waitFully() {
            if (!s_NeedToWait) return;
            // else, need to wait for END event
            while(!FASTLED_NRF52_SPIM->EVENTS_END) {};
            s_NeedToWait = 0;
            // only use two events in this code...
            nrf_spim_event_clear(FASTLED_NRF52_SPIM, NRF_SPIM_EVENT_END);
            nrf_spim_event_clear(FASTLED_NRF52_SPIM, NRF_SPIM_EVENT_STARTED);
            return;
        }
        // wait only until we can add a new transaction into the registers
        // (caller must still waitFully() before actually starting this next transaction)
        static void wait() {
            if (!s_NeedToWait) return;
            while (!FASTLED_NRF52_SPIM->EVENTS_STARTED) {};
            // leave the event set here... caller must waitFully() and start next transaction
            return;
        }

        /// write a byte out via SPI (returns immediately on writing register)
        static void writeByte(uint8_t b) {
            wait();
            // cannot use pointer to stack, so copy to m_buffer[]
            uint8_t i = (s_BufferIndex ? 1u : 0u);
            s_BufferIndex = !s_BufferIndex; // 1 <==> 0 swap

            s_Buffer[i][0u] = b; // cannot use the stack location, so copy to a more permanent buffer...
            nrf_spim_tx_buffer_set(
                FASTLED_NRF52_SPIM,
                &(s_Buffer[i][0u]),
                1
                );

            waitFully();
            nrf_spim_task_trigger(
                FASTLED_NRF52_SPIM,
                NRF_SPIM_TASK_START
                );
            return;
        }

        /// write a word out via SPI (returns immediately on writing register)
        static void writeWord(uint16_t w) {
            wait();
            // cannot use pointer to stack, so copy to m_buffer[]
            uint8_t i = (s_BufferIndex ? 1u : 0u);
            s_BufferIndex = !s_BufferIndex; // 1 <==> 0 swap

            s_Buffer[i][0u] = (w >> 8u); // cannot use the stack location, so copy to a more permanent buffer...
            s_Buffer[i][1u] = (w & 0xFFu); // cannot use the stack location, so copy to a more permanent buffer...
            nrf_spim_tx_buffer_set(
                FASTLED_NRF52_SPIM,
                &(s_Buffer[i][0u]),
                2
                );

            waitFully();
            nrf_spim_task_trigger(
                FASTLED_NRF52_SPIM,
                NRF_SPIM_TASK_START
                );
            return;
        }

        /// A raw set of writing byte values, assumes setup/init/waiting done elsewhere (static for use by adjustment classes)
        static void writeBytesValueRaw(uint8_t value, int len) {
            while (len--) { writeByte(value); }
        }

        /// A full cycle of writing a value for len bytes, including select, release, and waiting
        void writeBytesValue(uint8_t value, int len) {
            select();
            writeBytesValueRaw(value, len);
            waitFully();
            release();
        }

        /// A full cycle of writing a raw block of data out, including select, release, and waiting
        void writeBytes(uint8_t *data, int len) {
            // This is a special-case, with no adjustment of the bytes... write them directly...
            select();
            wait();
            nrf_spim_tx_buffer_set(
                FASTLED_NRF52_SPIM,
                data,
                len
                );
            waitFully();
            nrf_spim_task_trigger(
                FASTLED_NRF52_SPIM,
                NRF_SPIM_TASK_START
                );
            waitFully();
            release();
        }

        /// A full cycle of writing a raw block of data out, including select, release, and waiting
        template<class D> void writeBytes(uint8_t *data, int len) {
            uint8_t * end = data + len;
            select();
            wait();
            while(data != end) {
                writeByte(D::adjust(*data++));
            }
            D::postBlock(len);
            waitFully();
            release();
        }
        /// specialization for DATA_NOP ...
        //template<DATA_NOP> void writeBytes(uint8_t * data, int len) {
        //    writeBytes(data, len);
        //}

        /// write a single bit out, which bit from the passed in byte is determined by template parameter
        template <uint8_t BIT> inline static void writeBit(uint8_t b) {
            // SPIM instance must be finished transmitting and then disabled
            waitFully();
            nrf_spim_disable(FASTLED_NRF52_SPIM);
            // set the data pin to appropriate state
            if (b & (1 << BIT)) {
                FastPin<_DATA_PIN>::hi();
            } else {
                FastPin<_DATA_PIN>::lo();
            }
            // delay 1/2 cycle per SPI bit
            delaycycles<_SPI_CLOCK_DIVIDER/2>();
            FastPin<_CLOCK_PIN>::toggle();
            delaycycles<_SPI_CLOCK_DIVIDER/2>();
            FastPin<_CLOCK_PIN>::toggle();
            // re-enable the SPIM instance
            nrf_spim_enable(FASTLED_NRF52_SPIM);
        }

        /// write out pixel data from the given PixelController object, including select, release, and waiting
        template <uint8_t FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels) {
            select();
            int len = pixels.mLen;
            // TODO: If user indicates a pre-allocated double-buffer,
            //       then process all the pixels at once into that buffer,
            //       then use the non-templated WriteBytes(data, len) function
            //       to write the entire buffer as a single SPI transaction.
            while (pixels.has(1)) {
                if (FLAGS & FLAG_START_BIT) {
                    writeBit<0>(1);
                }
                writeByte(D::adjust(pixels.loadAndScale0()));
                writeByte(D::adjust(pixels.loadAndScale1()));
                writeByte(D::adjust(pixels.loadAndScale2()));
                pixels.advanceData();
                pixels.stepDithering();
            }
            D::postBlock(len);
            waitFully();
            release();
        }
    };

    // Static member definition and initialization using templates.
    // see https://stackoverflow.com/questions/3229883/static-member-initialization-in-a-class-template#answer-3229919
    template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
    bool NRF52SPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER>::s_InUse = false;
    template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
    bool NRF52SPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER>::s_NeedToWait = false;
    template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
    uint8_t NRF52SPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER>::s_BufferIndex = 0;
    template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
    uint8_t NRF52SPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER>::s_Buffer[2][2] = {{0,0},{0,0}};

#endif // #ifndef FASTLED_FORCE_SOFTWARE_SPI



#endif // #ifndef __FASTPIN_ARM_NRF52_H
