
#include "ethernet_SPI_component.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "esphome/core/application.h"

#ifdef USE_ESP_IDF
#include <lwip/dns.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
namespace esphome
{
    namespace ethernet_spi
    {
        typedef struct {
            uint8_t spi_cs_gpio;
            uint8_t int_gpio;
            int8_t phy_reset_gpio;
            uint8_t phy_addr;
        }spi_eth_module_config_t;
        static const char *const TAG="ethernet_spi";
        ethernet_SPI_component *global_eth_component;
        #define ESPHL_ERROR_CHECK(err, message) \
            if ((err) != ESP_OK) { \
                ESP_LOGE(TAG, message ": (%d) %s", err, esp_err_to_name(err)); \
                this->mark_failed(); \
                return; \
            }
        Ethernet_SPI_component::ethernet_SPI_component()
        {
            global_eth_component=this;
        }
        
        void Ethernet_SPI_component::setup()
        {
            ESP_LOGCONFIG(TAG,"Setting up Ethernet...");
            delay(300);
            esp_err_t err;
            err = esp_netif_init();
            ESPHL_ERROR_CHECK(err, "ETH netif init error");
            err = esp_event_loop_create_default();
            ESPHL_ERROR_CHECK(err, "ETH event loop error");
            //Create instance(s) of esp-netif for SPI Ethernet(s)
            esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
            esp_netif_config_t cfg_spi = {
                .base = &esp_netif_config,
                .driver=nullptr,
                .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
            };
            char if_key_str[10];
            char if_desc_str[10];
            char num_str[3];
            for (int i = 0; i < 1; i++) {
                itoa(i, num_str, 10);
                trcat(strcpy(if_key_str, "ETH_SPI_"), num_str);
                strcat(strcpy(if_desc_str, "eth"), num_str);
                esp_netif_config.if_key = if_key_str;
                esp_netif_config.if_desc = if_desc_str;
                esp_netif_config.route_prio = 30 - i;
                this->eth_netif_spi = esp_netif_new(&cfg_spi);
            }
            eth_mac_config_t mac_config_spi = ETH_MAC_DEFAULT_CONFIG();
            eth_phy_config_t phy_config_spi = ETH_PHY_DEFAULT_CONFIG();

            gpio_install_isr_service(0);
            spi_bus_config_t buscfg = {
                .miso_io_num = this->mosi_pin_,
                .mosi_io_num = this->miso_pin_,
                .sclk_io_num = this->sclk_pin_,
                .quadwp_io_num = -1,
                .quadhd_io_num = -1,
                #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
                .data4_io_num = -1,
                .data5_io_num = -1,
                .data6_io_num = -1,
                .data7_io_num = -1,
                #endif
                .max_transfer_sz = 0,
                .flags = 0,
                .intr_flags = 0,
            };
            ESP_ERROR_CHECK(spi_bus_initialize(1, &buscfg, SPI_DMA_CH_AUTO));

            // Init specific SPI Ethernet module configuration from Kconfig (CS GPIO, Interrupt GPIO, etc.)
            spi_eth_module_config_t spi_eth_module_config[1];
            INIT_SPI_ETH_MODULE_CONFIG(spi_eth_module_config, 0);
            esp_eth_mac_t *mac_spi[1];
            esp_eth_phy_t *phy_spi[1];
            spi_device_interface_config_t spi_devcfg = {
                .command_bits = 16, // Actually it's the address phase in W5500 SPI frame
                .address_bits = 8,  // Actually it's the control phase in W5500 SPI frame
                .mode = 0,
                .duty_cycle_pos=0,
                .cs_ena_pretrans=0,
                .cs_ena_posttrans=0,
                .clock_speed_hz = this->clockspeed_ * 1000 * 1000,
                .input_delay_ns=0,
                .spics_io_num=this->cs_pin_,
                .flags=0,
                .queue_size=20,
                .pre_cb=nullptr,
                .post_cb=nullptr,
            };
            spi_devcfg.spics_io_num=this->cs_pin_;
            phy_config_spi.phy_addr=this->phy_addr_;
            phy_config_spi.reset_gpio_num=-1;
            switch (this->type_)
            {
            case ETHERNET_TYPE_W5500:
                eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(CONFIG_EXAMPLE_ETH_SPI_HOST, &spi_devcfg);
                w5500_config.int_gpio_num = spi_eth_module_config[0].int_gpio;
                mac_spi[0] = esp_eth_mac_new_w5500(&w5500_config, &mac_config_spi);
                phy_spi[0] = esp_eth_phy_new_w5500(&phy_config_spi);
                break;
            default:
                break;
            }
            esp_eth_config_t eth_config_spi = ETH_DEFAULT_CONFIG(mac_spi[0], phy_spi[0]);
            
            ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config_spi, this->eth_handle_spi));
            ESP_ERROR_CHECK(esp_eth_ioctl(this->eth_handle_spi, ETH_CMD_S_MAC_ADDR, (uint8_t[]) {
                0x02, 0x00, 0x00, 0x12, 0x34, 0x56 + 0
            }));
            ESP_ERROR_CHECK(esp_netif_attach(this->eth_netif_spi, esp_eth_new_netif_glue(this->eth_handle_spi)));
            
            err = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &EthernetComponent::eth_event_handler, nullptr);
            ESPHL_ERROR_CHECK(err, "ETH event handler register error");
            err = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &EthernetComponent::got_ip_event_handler, nullptr);
            ESPHL_ERROR_CHECK(err, "GOT IP event handler register error");
            ESP_ERROR_CHECK(esp_eth_start(this->eth_handle_spi));
        }


        void Ethernet_SPI_component::loop()
        {
            const uint32_t now = millis();
            switch(this->state_)
            {
                case EthernetComponentState::STOPPED:
                    if(this->started_)
                    {
                        ESP_LOGI(TAG, "Starting ethernet connection");
                        this->state_ = EthernetComponentState::CONNECTING;
                        this->start_connect_();
                    }
                break;
                case EthernetComponentState::CONNECTING:
                    if(!this->started_)
                    {
                        ESP_LOGI(TAG, "Stopped ethernet connection");
                        this->state_ = EthernetComponentState::STOPPED;
                        this->start_connect_();
                    }
                break;
                case EthernetComponentState::CONNECTED:
                    if(this->started_)
                    {
                        ESP_LOGI(TAG, "Starting ethernet connection");
                        this->state_ = EthernetComponentState::CONNECTING;
                        this->start_connect_();
                    }
                    else if(this->connected_)
                    {
                        ESP_LOGI(TAG, "Connected via Ethernet!");
                        this->state_ = EthernetComponentState::CONNECTED;

                        this->dump_connect_params_();
                        this->status_clear_warning();
                    }
                    else if(now - this->connect_begin_ > 15000)
                    {
                        ESP_LOGW(TAG, "Connecting via ethernet failed! Re-connecting...");
                        this->start_connect_();
                    }
                break;
                case EthernetComponentState::CONNECTED:
                    if (!this->started_) {
                    ESP_LOGI(TAG, "Stopped ethernet connection");
                    this->state_ = EthernetComponentState::STOPPED;
                } 
                else if (!this->connected_) {
                    ESP_LOGW(TAG, "Connection via Ethernet lost! Re-connecting...");
                    this->state_ = EthernetComponentState::CONNECTING;
                    this->start_connect_();
                }
                break;
            }
        }


        void Ethernet_SPI_component::dump_config()
        {
            ESP_LOGCONFIG(TAG,"Ethernet_SPI:");
            this->dump_connect_params_();
            ESP_LOGCONFIG(TAG," MOSI pin:%u",this->mosi_pin_);
            ESP_LOGCONFIG(TAG," MISO pin:%u",this->miso_pin_);
            ESP_LOGCONFIG(TAG," cs pin:%u",this->cs_pin_);
            ESP_LOGCONFIG(TAG," sclk pin:%u",this->sclk_pin_);
        }
        void Ethernet_SPI_component::dump_connect_params_()
        {
            esp_netif_ip_info_t ip;
            esp_netif_get_ip_info(this->eth_netif_spi,&ip);
            ESP_LOGCONFIG(TAG, "  IP Address: %s", network::IPAddress(ip.ip.addr).str().c_str());
            ESP_LOGCONFIG(TAG, "  Hostname: '%s'", App.get_name().c_str());
            ESP_LOGCONFIG(TAG, "  Subnet: %s", network::IPAddress(ip.netmask.addr).str().c_str());
            ESP_LOGCONFIG(TAG, "  Gateway: %s", network::IPAddress(ip.gw.addr).str().c_str());
            uint8_t mac[6];
            err = esp_eth_ioctl(this->eth_handle_, ETH_CMD_G_MAC_ADDR, &mac);
            ESPHL_ERROR_CHECK(err, "ETH_CMD_G_MAC error");
            ESP_LOGCONFIG(TAG, "  MAC Address: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        }
        float EthernetComponent::get_setup_priority() const { return setup_priority::WIFI; }
        bool EthernetComponent::can_proceed() { return this->is_connected(); }
        void Ethernet_SPI_component::start_connect_()
        {
            this->connect_begin_ = millis();
            this->status_set_warning();
            esp_err_t err;
            err = esp_netif_set_hostname(this->eth_netif_spi, App.get_name().c_str());
            if (err != ERR_OK) {
                ESP_LOGW(TAG, "esp_netif_set_hostname failed: %s", esp_err_to_name(err));
            }
            esp_netif_ip_info_t info;
            if (this->manual_ip_spi.has_value()) {
                info.ip.addr = static_cast<uint32_t>(this->manual_ip_spi->static_ip);
                info.gw.addr = static_cast<uint32_t>(this->manual_ip_spi->gateway);
                info.netmask.addr = static_cast<uint32_t>(this->manual_ip_spi->subnet);
            }
            else {
                info.ip.addr = 0;
                info.gw.addr = 0;
                info.netmask.addr = 0;
            }
            esp_netif_dhcp_status_t status = ESP_NETIF_DHCP_INIT;
            err = esp_netif_dhcpc_get_status(this->eth_netif_spi, &status);
            ESPHL_ERROR_CHECK(err, "DHCPC Get Status Failed!");

            ESP_LOGV(TAG, "DHCP Client Status: %d", status);

            err = esp_netif_dhcpc_stop(this->eth_netif_spi);
            if (err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
                ESPHL_ERROR_CHECK(err, "DHCPC stop error");
            }

            err = esp_netif_set_ip_info(this->eth_netif_spi, &info);
            ESPHL_ERROR_CHECK(err, "DHCPC set IP info error");
            if (this->manual_ip_.has_value()) {
                if (uint32_t(this->manual_ip_spi->dns1) != 0) {
                    ip_addr_t d;
                    d.addr = static_cast<uint32_t>(this->manual_ip_spi->dns1);
                    dns_setserver(0, &d);
                }
                if (uint32_t(this->manual_ip_->dns1) != 0) {
                    ip_addr_t d;
                    d.addr = static_cast<uint32_t>(this->manual_ip_->dns2);
                    dns_setserver(1, &d);
                }
                else
                {
                    err = esp_netif_dhcpc_start(this->eth_netif_spi);
                    if (err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
                        ESPHL_ERROR_CHECK(err, "DHCPC start error");
                    }
                }
            }
            this->connect_begin_ = millis();
            this->status_set_warning();
        }
        network::IPAddress Ethernet_SPI_component::get_ip_address() {
            esp_netif_ip_info_t ip;
            esp_netif_get_ip_info(this->eth_netif_spi, &ip);
            return {ip.ip.addr};
        }
        void Ethernet_SPI_component::got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,void *event_data) {
            global_eth_component->connected_ = true;
            ESP_LOGV(TAG, "[Ethernet event] ETH Got IP (num=%d)", event_id);
        }
        void Ethernet_SPI_component::start_connect_() {
            this->connect_begin_ = millis();
            this->status_set_warning();

            esp_err_t err;
            err = esp_netif_set_hostname(this->eth_netif_spi, App.get_name().c_str());
            if (err != ERR_OK) {
                ESP_LOGW(TAG, "esp_netif_set_hostname failed: %s", esp_err_to_name(err));
            }
            esp_netif_ip_info_t info;
            if (this->manual_ip_spi.has_value()) {
                info.ip.addr = static_cast<uint32_t>(this->manual_ip_spi->static_ip);
                info.gw.addr = static_cast<uint32_t>(this->manual_ip_spi->gateway);
                info.netmask.addr = static_cast<uint32_t>(this->manual_ip_spi->subnet);
            } else {
                info.ip.addr = 0;
                info.gw.addr = 0;
                info.netmask.addr = 0;
            }
            esp_netif_dhcp_status_t status = ESP_NETIF_DHCP_INIT;
            err = esp_netif_dhcpc_get_status(this->eth_netif_spi, &status);
            ESPHL_ERROR_CHECK(err, "DHCPC Get Status Failed!");

            ESP_LOGV(TAG, "DHCP Client Status: %d", status);

            err = esp_netif_dhcpc_stop(this->eth_netif_spi);
            if (err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
             ESPHL_ERROR_CHECK(err, "DHCPC stop error");
            }

            err = esp_netif_set_ip_info(this->eth_netif_spi, &info);
            ESPHL_ERROR_CHECK(err, "DHCPC set IP info error");
            if (this->manual_ip_spi.has_value()) {
                if (uint32_t(this->manual_ip_spi->dns1) != 0) {
                    ip_addr_t d;
            #if LWIP_IPV6
                    d.type = IPADDR_TYPE_V4;
                    d.u_addr.ip4.addr = static_cast<uint32_t>(this->manual_ip_spi->dns1);
            #else
                    d.addr = static_cast<uint32_t>(this->manual_ip_spi->dns1);
            #endif
                    dns_setserver(0, &d);
                }
                if (uint32_t(this->manual_ip_spi->dns1) != 0) {
                    ip_addr_t d;
            #if LWIP_IPV6
                    d.type = IPADDR_TYPE_V4;
                    d.u_addr.ip4.addr = static_cast<uint32_t>(this->manual_ip_spi->dns2);
            #else
                    d.addr = static_cast<uint32_t>(this->manual_ip_spi->dns2);
            #endif
                    dns_setserver(1, &d);
                }
            } 
            else {
                err = esp_netif_dhcpc_start(this->eth_netif_spi);
                if (err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
                    ESPHL_ERROR_CHECK(err, "DHCPC start error");
                }
            }
            this->connect_begin_ = millis();
            this->status_set_warning();
        }
        void Ethernet_SPI_component::set_manual_ip(const ManualIP &manual_ip) { this->manual_ip_api = manual_ip; }
        void Ethernet_SPI_component::eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event, void *event_data)
        {
            const char *event_name;

            switch (event) {
                case ETHERNET_EVENT_START:
                    event_name = "ETH started";
                    global_eth_component->started_ = true;
                break;
                case ETHERNET_EVENT_STOP:
                    event_name = "ETH stopped";
                    global_eth_component->started_ = false;
                    global_eth_component->connected_ = false;
                break;
                case ETHERNET_EVENT_CONNECTED:
                    event_name = "ETH connected";
                break;
                case ETHERNET_EVENT_DISCONNECTED:
                    event_name = "ETH disconnected";
                    global_eth_component->connected_ = false;
                break;
                default:
                    return;
            }
            ESP_LOGV(TAG, "[Ethernet event] %s (num=%d)", event_name, event);

        }

        
        bool Ethernet_SPI_component::is_connected(){ return this->state_== EthernetComponentState::CONNECTED;}
        void Ethernet_SPI_component::set_sclk_pin(uint8_t sclk_pin){this->sclk_pin_=sclk_pin;}
        void Ethernet_SPI_component::set_phy_addr(uint8_t phy_addr){this->phy_addr_=phy_addr;}
        void Ethernet_SPI_component::set_power_pin(int power_pin){this->power_pin_=power_pin;}
        void Ethernet_SPI_component::set_mosi_pin(uint8_t mosi_pin){this->mosi_pin_=mosi_pin;}
        void Ethernet_SPI_component::set_miso_pin(uint8_t miso_pin){this->miso_pin_=miso_pin;}
        void Ethernet_SPI_component::set_cs_pin(uint8_t cs_pin){this->cs_pin_=cs_pin};
        void Ethernet_SPI_component::set_clockspeed(uint8_t clockspeed){this->clockspeed_=clockspeed;}
    }
}
#endif