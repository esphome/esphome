#ifdef USE_ESP_IDF

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/network/ip_address.h"


#include <lwip/dns.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/spi_master.h"
namespace esphome
{

    namespace ethernet_spi
    {
        enum EthernetType_spi
        {
            ETHERNET_TYPE_W5500=0,
            ETHERNET_TYPE_KSZ8851SNL,
            ETHERNET_TYPE_DM9051,

        };

        struct ManualIP_spi{
            network::IPAddress static_ip;
            network::IPAddress gateway;
            network::IPAddress subnet;
            network::IPAddress dns1;
            network::IPAddress dns2;
        };

        enum class EthernetComponentState{
            STOPPED,
            CONNECTING,
            CONNECTED,
        };



        class Ethernet_SPI_component : public Component
        {
        private:
            
        public:
            Ethernet_SPI_component(/* args */);
            ~Ethernet_SPI_component();
            void setup() override;
            void loop() override;
            void dump_config() override;
            float get_setup_priority() const override;
            bool can_proceed() override;
            bool is_connected();
            void set_phy_addr(uint8_t phy_addr);
            void set_power_pin(int power_pin);
            void set_mosi_pin(uint8_t mosi_pin);
            void set_miso_pin(uint8_t miso_pin);
            void set_cs_pin(uint8_t cs_pin);
            void set_sclk_pin(uint8_t sclk_pin );
            void set_clockspeed(uint8_t clockspeed);
            network::IPAddress get_ip_address();
            std::string get_use_address() const;
            void set_use_address(const std::string &use_address);
        protected:
            static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
            static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

            void start_connect_();
            void dump_connect_params_();

            std::string use_address_;
            uint8_t phy_addr_{0};
            int power_pin_{-1};
            uint8_t sclk_pin_{23};
            uint8_t mosi_pin_{13};
            uint8_t miso_pin_{12};
            uint8_t cs_pin_{15};
            uint8_t clockspeed_{5};


            EthernetType_spi type_{ETHERNET_TYPE_W5500};
            optional<ManualIP_spi> manual_ip_spi{};


            bool started_{false};
            bool connected_{false};
            EthernetComponentState state_{EthernetComponentState::STOPPED};
            uint32_t connect_begin_;
            esp_netif_t *eth_netif_{nullptr};
            esp_eth_handle_t eth_handle_spi;
            esp_netif_t *eth_netif_spi;
        };
        extern Ethernet_SPI_component *global_eth_component;
        Ethernet_SPI_component::ethernet_SPI_component(/* args */)
        {

        }
        
        Ethernet_SPI_component::~ethernet_SPI_component()
        {
        }
        
    }
    
}
#endif