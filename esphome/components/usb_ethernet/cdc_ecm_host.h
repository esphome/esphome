/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "usb/usb_host.h"
#include "usb_types_cdc.h"
#include "esp_err.h"
#include "esp_event.h"

// Pass these to cdc_ecm_host_open() to signal that you don't care about VID/PID of the opened device
#define CDC_HOST_ANY_VID (0)
#define CDC_HOST_ANY_PID (0)

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct cdc_dev_s *cdc_ecm_dev_hdl_t;

    /**
     * @brief CDC-ECM Device Event types to upper layer
     * PN COMPLETED
     *
     */
    typedef enum
    {
        CDC_ECM_HOST_EVENT_NETWORK_CONNECTION, //!< Network connection state changed
        CDC_ECM_HOST_EVENT_SPEED_CHANGE,       //!< Link speed changed
        CDC_ECM_HOST_EVENT_ERROR,              //!< Error occurred
        CDC_ECM_HOST_EVENT_DISCONNECTED        //!< Device disconnected
    } cdc_ecm_host_dev_event_t;

    /**
     * @brief CDC-ECM Device Event data structure
     * PN COMPLETED
     *
     */
    typedef struct
    {
        cdc_ecm_host_dev_event_t type; //!< Type of event (Network Connection, Speed Change, Error, etc.)
        union
        {
            int error;                 //!< Error code from USB Host
            bool network_connected;    //!< Indicates if the network is connected (true = connected, false = disconnected)
            uint32_t link_speed;       //!< Link speed in bits per second (e.g., 100000000 for 100Mbps)
            cdc_ecm_dev_hdl_t cdc_hdl; //!< Handle to the ECM device, used for disconnection events
        } data;
    } cdc_ecm_host_dev_event_data_t;

    /**
     * @brief CDC-ECM Speed Change Event Data
     * PN COMPLETED
     *
     */
    typedef struct __attribute__((packed))
    {
        /** Request type. Bits 0:4 determine recipient, see
         * \ref usb_request_recipient. Bits 5:6 determine type, see
         * \ref usb_request_type. Bit 7 determines data transfer direction, see
         * \ref usb_endpoint_direction.
         */
        uint8_t bmRequestType;

        /** Request. If the type bits of bmRequestType are equal to
         * \ref usb_request_type::LIBUSB_REQUEST_TYPE_STANDARD
         * "USB_REQUEST_TYPE_STANDARD" then this field refers to
         * \ref usb_standard_request. For other cases, use of this field is
         * application-specific. */
        uint8_t bRequest;

        /** Value. Varies according to request */
        uint16_t wValue;

        /** Index. Varies according to request, typically used to pass an index
         * or offset */
        uint16_t wIndex;

        /** Number of bytes to transfer */
        uint16_t wLength;
    } cdc_ecm_setup_packet_t;

    /**
     * @brief CDC-ECM Control Transfer Data
     * PN COMPLETED
     *
     */
    typedef struct __attribute__((packed))
    {
        uint32_t downlink_speed; //!< RX speed in bits per second
        uint32_t uplink_speed;   //!< TX speed in bits per second
    } cdc_ecm_speed_change_data_t;
    /**
     * @brief New USB device callback
     * PN COMPLETED
     *
     * Provides already opened usb_dev, that will be closed after this callback returns.
     * This is useful for peeking device's descriptors, e.g. peeking VID/PID and loading proper driver.
     *
     * @attention This callback is called from USB Host context, so the CDC device can't be opened here.
     */
    typedef void (*cdc_ecm_new_dev_callback_t)(usb_device_handle_t usb_dev);

    /**
     * @brief Data receive callback type
     * PN COMPLETED
     *
     * @param[in] data     Pointer to received data
     * @param[in] data_len Length of received data in bytes
     * @param[in] user_arg User's argument passed to open function
     * @return true        Received data was processed     -> Flush RX buffer
     * @return false       Received data was NOT processed -> Append new data to the buffer
     */
    typedef bool (*cdc_ecm_data_callback_t)(const uint8_t *data, size_t data_len, void *user_arg);

    /**
     * @brief Device event callback type
     * PN COMPLETED
     *
     * @param[in] event    Event structure
     * @param[in] user_arg User's argument passed to open function
     */
    typedef void (*cdc_ecm_host_dev_callback_t)(const cdc_ecm_host_dev_event_data_t *event, void *user_ctx);

    /**
     * @brief NetIf event callback type
     * PN COMPLETED
     *
     * @param[in] event-base    Event structure
     * @param[in] event_id      Event ID
     * @param[in] event_data    Event data
     */
    typedef void (*cdc_ecm_event_callback_t)(void *arg, esp_event_base_t event_base,
                                             int32_t event_id, void *event_data);

    /**
     * @brief Configuration structure of USB Host CDC-ECM driver
     * PN COMPLETED
     *
     */
    typedef struct
    {
        size_t driver_task_stack_size;         /**< Stack size of the driver's task */
        unsigned driver_task_priority;         /**< Priority of the driver's task */
        int xCoreID;                           /**< Core affinity of the driver's task */
        cdc_ecm_new_dev_callback_t new_dev_cb; /**< New USB device connected callback. Can be NULL. */
    } cdc_ecm_host_driver_config_t;

    /**
     * @brief Configuration structure of CDC-ECM device
     * PN COMPLETED
     *
     */
    typedef struct
    {
        uint32_t connection_timeout_ms;       /**< Timeout for USB device connection in [ms] */
        size_t out_buffer_size;               /**< Maximum size of USB bulk out transfer, set to 0 for read-only devices */
        size_t in_buffer_size;                /**< Maximum size of USB bulk in transfer */
        cdc_ecm_host_dev_callback_t event_cb; /**< Device's event callback function. Can be NULL */
        cdc_ecm_data_callback_t data_cb;      /**< Device's data RX callback function. Can be NULL for write-only devices */
        void *user_arg;                       /**< User's argument that will be passed to the callbacks */
    } cdc_ecm_host_device_config_t;

    /**
     * @brief Parameters structure of cdc_ecm_init()
     * PN COMPLETED
     *
     */

    typedef struct
    {
        uint16_t vid;                      //!< Device's Vendor ID
        uint16_t pids[2];                  //!< Device's Product ID(s)
        cdc_ecm_event_callback_t event_cb; //!< Event callback function for IP and ETH events
        void *callback_arg;                //!< User's argument passed to the event callback
        char *hostname;                    //!< Hostname for the device
        char *nameserver;                  //!< Nameserver for the device
        char *if_key;                      //!< Interface key for the device
        char *if_desc;                     //!< Interface description for the device
        bool disable_dhcp;                 //!< If true, do not start DHCP client (for manual IP configuration)
    } cdc_ecm_params_t;

    /**
     * @brief Initialise CDC-ECM process
     * PN COMPLETED
     *
     *
     * @param[in] cdc_ecm_params    Parameters structure for CDC-ECM process
     * @return
     */
    void cdc_ecm_init(cdc_ecm_params_t *cdc_ecm_params);

    /**
     * @brief Install CDC-ECM driver
     * PN COMPLETED
     *
     * - USB Host Library must already be installed before calling this function (via usb_host_install())
     * - This function should be called before calling any other CDC driver functions
     *
     * @param[in] driver_config Driver configuration structure. If set to NULL, a default configuration will be used.
     * @return
     *   - ESP_OK: Success
     *   - ESP_ERR_INVALID_STATE: The CDC driver is already installed or USB host library is not installed
     *   - ESP_ERR_NO_MEM: Not enough memory for installing the driver
     */
    esp_err_t cdc_ecm_host_install(const cdc_ecm_host_driver_config_t *driver_config);

    /**
     * @brief Uninstall CDC-ACM driver
     * PN COMPLETED
     *
     * - Users must ensure that all CDC devices must be closed via cdc_ecm_host_close() before calling this function
     *
     * @return
     *   - ESP_OK: Success
     *   - ESP_ERR_INVALID_STATE: The CDC driver is not installed or not all CDC devices are closed
     *   - ESP_ERR_NOT_FINISHED: The CDC driver failed to uninstall completely
     */
    esp_err_t cdc_ecm_host_uninstall(void);

    /**
     * @brief Register new USB device callback
     * PN COMPLETED
     *
     * The callback will be called for every new USB device, not just CDC-ACM class.
     *
     * @param[in] new_dev_cb New device callback function
     * @return
     *   - ESP_OK: Success
     */
    esp_err_t cdc_ecm_host_register_new_dev_callback(cdc_ecm_new_dev_callback_t new_dev_cb);

    /**
     * @brief Open CDC-ACM device
     * PN COMPLETED
     *
     * The driver first looks for CDC compliant descriptor, if it is not found the driver checks if the interface has 2 Bulk endpoints that can be used for data
     *
     * Use CDC_HOST_ANY_* macros to signal that you don't care about the device's VID and PID. In this case, first USB device will be opened.
     * It is recommended to use this feature if only one device can ever be in the system (there is no USB HUB connected).
     *
     * @param[in] vid           Device's Vendor ID, set to CDC_HOST_ANY_VID for any
     * @param[in] pid           Device's Product ID, set to CDC_HOST_ANY_PID for any
     * @param[in] interface_idx Index of device's interface used for CDC-ACM communication
     * @param[in] dev_config    Configuration structure of the device
     * @param[out] cdc_hdl_ret  CDC device handle
     * @return
     *   - ESP_OK: Success
     *   - ESP_ERR_INVALID_STATE: The CDC driver is not installed
     *   - ESP_ERR_INVALID_ARG: dev_config or cdc_hdl_ret is NULL
     *   - ESP_ERR_NO_MEM: Not enough memory for opening the device
     *   - ESP_ERR_NOT_FOUND: USB device with specified VID/PID is not connected or does not have specified interface
     */
    esp_err_t cdc_ecm_host_open(uint16_t vid, uint16_t pid, uint8_t interface_idx, const cdc_ecm_host_device_config_t *dev_config, cdc_ecm_dev_hdl_t *cdc_hdl_ret);

    /**
     * @brief Retrieve MAC address from cdc_hdl
     * PN COMPLETED
     *
     * Return to on transfer completed OK.
     * Cancel the transfer and issue user's callback in case of an error.
     *
     * @param cdc_hdl CDC device handle
     * @param[in] mac_addr MAC address buffer
     * @return true Transfer completed
     * @return false Transfer NOT completed
     */
    esp_err_t cdc_ecm_get_mac_addr(cdc_ecm_dev_hdl_t cdc_hdl, uint8_t *mac_addr);

    /**
     * @brief Retrieve connection status  from cdc_hdl
     * PN COMPLETED
     *
     *
     * @param cdc_hdl CDC device handle
     * @return true Device is connected
     * @return false Devics is NOT completed
     */
    bool cdc_ecm_get_connection_status(cdc_ecm_dev_hdl_t cdc_hdl);

    /**
     * @brief Close CDC device and release its resources
     * PN COMPLETED
     *
     * @note All in-flight transfers will be prematurely canceled.
     * @param[in] cdc_hdl CDC handle obtained from cdc_ecm_host_open()
     * @return
     *   - ESP_OK: Success - device closed
     *   - ESP_ERR_INVALID_STATE: cdc_hdl is NULL or the CDC driver is not installed
     */
    esp_err_t cdc_ecm_host_close(cdc_ecm_dev_hdl_t cdc_hdl);

    /**
     * @brief Transmit data - blocking mode
     * PN COMPLETED
     *
     * @param cdc_hdl CDC handle obtained from cdc_ecm_host_open()
     * @param[in] data       Data to be sent
     * @param[in] data_len   Data length
     * @param[in] timeout_ms Timeout in [ms]
     * @return esp_err_t
     */
    esp_err_t cdc_ecm_host_data_tx_blocking(cdc_ecm_dev_hdl_t cdc_hdl, const uint8_t *data, size_t data_len, uint32_t timeout_ms);

    /**
     * @brief SetInterface function
     *
     * @see Table 9-4 of USB 2.0 specification
     *
     * @param     cdc_hdl     CDC handle obtained from cdc_acm_host_open()
     * @return esp_err_t
     */
    esp_err_t cdc_ecm_set_interface(cdc_ecm_dev_hdl_t cdc_hdl);

    /**
     * @brief SetPacketFilter function
     *
     * @see Section 6.2.4 of the CDC-ECM Specification (Rev 1.2)
     *
     * @param     cdc_hdl     CDC handle obtained from cdc_acm_host_open()
     * @param[in] filter_mas  Packet Filter Mask
     * @return esp_err_t
     */
    esp_err_t cdc_ecm_set_packet_filter(cdc_ecm_dev_hdl_t cdc_hdl, uint16_t filter_mask);

    /**
     * @brief SetMulticastFilter function - allows host use device mac address in netif
     *
     *
     * @param     cdc_hdl     CDC handle obtained from cdc_acm_host_open()
     * @param[in] mac  Device mac address
     * @return esp_err_t
     */
    esp_err_t cdc_ecm_set_multicast_filter(cdc_ecm_dev_hdl_t cdc_hdl, uint8_t *mac);

    /**
     * @brief Get String Descriptor function
     *
     * @see Section 6.2.4 of the CDC-ECM Specification (Rev 1.2)
     *
     * @param     cdc_hdl           CDC handle obtained from cdc_acm_host_open()
     * @param[in] uint16_t index    Index of the string descriptor
     * @param[out] uint8_t *output  Output buffer
     *
     * @return esp_err_t
     */
    esp_err_t cdc_ecm_get_string_desc(cdc_ecm_dev_hdl_t cdc_hdl, uint16_t index, uint8_t *output);

    /**
     * @brief Print device's descriptors
     * PN COMPLETED
     *
     * Device and full Configuration descriptors are printed in human readable format to stdout.
     *
     * @param cdc_hdl CDC handle obtained from cdc_ecm_host_open()
     */
    void cdc_ecm_host_desc_print(cdc_ecm_dev_hdl_t cdc_hdl);

    /**
     * @brief Get CDC functional descriptor
     * PN COMPLETED
     *
     * @param cdc_hdl       CDC handle obtained from cdc_ecm_host_open()
     * @param[in] desc_type Type of functional descriptor
     * @param[out] desc_out Pointer to the required descriptor
     * @return
     *   - ESP_OK: Success
     *   - ESP_ERR_INVALID_ARG: Invalid device or descriptor type
     *   - ESP_ERR_NOT_FOUND: The required descriptor is not present in the device
     */
    esp_err_t cdc_ecm_host_cdc_desc_get(cdc_ecm_dev_hdl_t cdc_hdl, cdc_desc_subtype_t desc_type, const usb_standard_desc_t **desc_out);

    /**
     * @brief Send command to CTRL endpoint
     * PN COMPLETED
     *
     * Sends Control transfer as described in USB specification chapter 9.
     * This function can be used by device drivers that use custom/vendor specific commands.
     * These commands can either extend or replace commands defined in USB CDC-PSTN specification rev. 1.2.
     *
     * @param        cdc_hdl       CDC handle obtained from cdc_ecm_host_open()
     * @param[in]    bmRequestType Field of USB control request
     * @param[in]    bRequest      Field of USB control request
     * @param[in]    wValue        Field of USB control request
     * @param[in]    wIndex        Field of USB control request
     * @param[in]    wLength       Field of USB control request
     * @param[inout] data          Field of USB control request
     * @return esp_err_t
     */
    esp_err_t cdc_ecm_host_send_custom_request(cdc_ecm_dev_hdl_t cdc_hdl, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength, uint8_t *data);

#ifdef __cplusplus
}
class CdcAcmDevice
{
public:
    // Operators
    CdcAcmDevice() : cdc_hdl(NULL) {};
    virtual ~CdcAcmDevice()
    {
        // Close CDC-ACM device, if it wasn't explicitly closed
        if (this->cdc_hdl != NULL)
        {
            this->close();
        }
    }

    inline esp_err_t tx_blocking(uint8_t *data, size_t len, uint32_t timeout_ms = 100)
    {
        return cdc_ecm_host_data_tx_blocking(this->cdc_hdl, data, len, timeout_ms);
    }

    inline esp_err_t open(uint16_t vid, uint16_t pid, uint8_t interface_idx, const cdc_ecm_host_device_config_t *dev_config)
    {
        return cdc_ecm_host_open(vid, pid, interface_idx, dev_config, &this->cdc_hdl);
    }

    inline esp_err_t open_vendor_specific(uint16_t vid, uint16_t pid, uint8_t interface_idx, const cdc_ecm_host_device_config_t *dev_config)
    {
        return cdc_ecm_host_open(vid, pid, interface_idx, dev_config, &this->cdc_hdl);
    }

    inline esp_err_t close()
    {
        const esp_err_t err = cdc_ecm_host_close(this->cdc_hdl);
        if (err == ESP_OK)
        {
            this->cdc_hdl = NULL;
        }
        return err;
    }

    virtual inline esp_err_t set_interface()
    {
        return cdc_ecm_set_interface(this->cdc_hdl);
    }

    virtual inline esp_err_t set_packet_filter(uint32_t filter_mask)
    {
        return cdc_ecm_set_packet_filter(this->cdc_hdl, filter_mask);
    }

    inline esp_err_t send_custom_request(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength, uint8_t *data)
    {
        return cdc_ecm_host_send_custom_request(this->cdc_hdl, bmRequestType, bRequest, wValue, wIndex, wLength, data);
    }

private:
    CdcAcmDevice &operator=(const CdcAcmDevice &Copy);
    bool operator==(const CdcAcmDevice &param) const;
    bool operator!=(const CdcAcmDevice &param) const;
    cdc_ecm_dev_hdl_t cdc_hdl;
};
#endif
