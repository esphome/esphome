/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "soc/soc_caps.h"
#include "lwip/ip_addr.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_mac.h"

#include "usb/usb_host.h"
#include "cdc_ecm_host.h"
#include "cdc_host_descriptor_parsing.h"
#include "cdc_host_types.h"

static const char *TAG = "cdc_ecm";

// Control transfer constants
#define CDC_ECM_CTRL_TRANSFER_SIZE (64) // All standard CTRL requests and responses fit in this size
#define CDC_ECM_CTRL_TIMEOUT_MS (5000)  // Every CDC device should be able to respond to CTRL transfer in 5 seconds
#define CDC_ECM_USB_HOST_PRIORITY (20)

// CDC-ECM spinlock
static portMUX_TYPE cdc_ecm_lock = portMUX_INITIALIZER_UNLOCKED;
#define CDC_ECM_ENTER_CRITICAL() portENTER_CRITICAL(&cdc_ecm_lock)
#define CDC_ECM_EXIT_CRITICAL() portEXIT_CRITICAL(&cdc_ecm_lock)

// CDC-ECM events
#define CDC_ECM_TEARDOWN BIT0
#define CDC_ECM_TEARDOWN_COMPLETE BIT1

// CDC-ECM check macros
#define CDC_ECM_CHECK(cond, ret_val) ({ \
    if (!(cond))                        \
    {                                   \
        return (ret_val);               \
    }                                   \
})

#define CDC_ECM_CHECK_FROM_CRIT(cond, ret_val) ({ \
    if (!(cond))                                  \
    {                                             \
        CDC_ECM_EXIT_CRITICAL();                  \
        return ret_val;                           \
    }                                             \
})

cdc_ecm_dev_hdl_t cdc_dev = NULL;
esp_netif_t *usb_netif = NULL;
static SemaphoreHandle_t device_disconnected_sem;

static bool network_connected = false;
uint32_t link_speed = 0;

// CDC-ECM driver object
typedef struct
{
    usb_host_client_handle_t cdc_ecm_client_hdl; /*!< USB Host handle reused for all CDC-ECM devices in the system */
    SemaphoreHandle_t open_close_mutex;
    EventGroupHandle_t event_group;
    cdc_ecm_new_dev_callback_t new_dev_cb;
    SLIST_HEAD(list_dev, cdc_dev_s)
    cdc_devices_list; /*!< List of open pseudo devices */
} cdc_ecm_obj_t;

static cdc_ecm_obj_t *p_cdc_ecm_obj = NULL;

/**
 * @brief Default CDC-ECM driver configuration
 *
 * This configuration is used when user passes NULL to config pointer during device open.
 */
static const cdc_ecm_host_driver_config_t cdc_ecm_driver_config_default = {
    .driver_task_stack_size = 8192,
    .driver_task_priority = 10,
    .xCoreID = 0,
    .new_dev_cb = NULL,
};

/**
 * @brief Notification received callback
 *
 * Notification (interrupt) IN transfer is submitted at the end of this function to ensure periodic poll of IN endpoint.
 *
 * @param[in] transfer Transfer that triggered the callback
 */
static void notif_xfer_cb(usb_transfer_t *transfer);

/**
 * @brief Data received callback
 *
 * Data (bulk) IN transfer is submitted at the end of this function to ensure continuous poll of IN endpoint.
 *
 * @param[in] transfer Transfer that triggered the callback
 */
static void in_xfer_cb(usb_transfer_t *transfer);

/**
 * @brief Data send callback
 *
 * Reused for bulk OUT and CTRL transfers
 *
 * @param[in] transfer Transfer that triggered the callback
 */
static void out_xfer_cb(usb_transfer_t *transfer);

/**
 * @brief USB Host Client event callback
 *
 * Handling of USB device connection/disconnection to/from root HUB.
 *
 * @param[in] event_msg Event message type
 * @param[in] arg Caller's argument (not used in this driver)
 */
static void usb_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg);

/**
 * @brief Reset IN transfer
 *
 * In in_xfer_cb() we can modify IN transfer parameters, this function resets the transfer to its defaults
 *
 * @param[in] cdc_dev Pointer to CDC device
 */
static void cdc_ecm_reset_in_transfer(cdc_dev_t *cdc_dev)
{
    assert(cdc_dev->data.in_xfer);
    usb_transfer_t *transfer = cdc_dev->data.in_xfer;
    uint8_t **ptr = (uint8_t **)(&(transfer->data_buffer));
    *ptr = cdc_dev->data.in_data_buffer_base;
    transfer->num_bytes = transfer->data_buffer_size;
    // This is a hotfix for IDF changes, where 'transfer->data_buffer_size' does not contain actual buffer length,
    // but *allocated* buffer length, which can be larger if CONFIG_HEAP_POISONING_COMPREHENSIVE is enabled
    transfer->num_bytes -= transfer->data_buffer_size % cdc_dev->data.in_mps;
}

/**
 * @brief CDC-ECM driver handling task
 *
 * USB host client registration and deregistration is handled here.
 *
 * @param[in] arg User's argument. Handle of a task that started this task.
 */
static void cdc_ecm_client_task(void *arg)
{
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    cdc_ecm_obj_t *cdc_ecm_obj = p_cdc_ecm_obj; // Make local copy of the driver's handle
    assert(cdc_ecm_obj->cdc_ecm_client_hdl);

    // Start handling client's events
    while (1)
    {
        usb_host_client_handle_events(cdc_ecm_obj->cdc_ecm_client_hdl, portMAX_DELAY);
        EventBits_t events = xEventGroupGetBits(cdc_ecm_obj->event_group);
        if (events & CDC_ECM_TEARDOWN)
        {
            break;
        }
    }

    ESP_LOGD(TAG, "Deregistering client");
    ESP_ERROR_CHECK(usb_host_client_deregister(cdc_ecm_obj->cdc_ecm_client_hdl));
    xEventGroupSetBits(cdc_ecm_obj->event_group, CDC_ECM_TEARDOWN_COMPLETE);
    vTaskDelete(NULL);
}

/**
 * @brief Cancel transfer and reset endpoint
 *
 * This function will cancel ongoing transfer a reset its endpoint to ready state.
 *
 * @param[in] dev_hdl USB device handle
 * @param[in] transfer Transfer to be cancelled
 * @return esp_err_t
 */
static esp_err_t cdc_acm_reset_transfer_endpoint(usb_device_handle_t dev_hdl, usb_transfer_t *transfer)
{
    assert(dev_hdl);
    assert(transfer);

    ESP_RETURN_ON_ERROR(usb_host_endpoint_halt(dev_hdl, transfer->bEndpointAddress), TAG, );
    ESP_RETURN_ON_ERROR(usb_host_endpoint_flush(dev_hdl, transfer->bEndpointAddress), TAG, );
    usb_host_endpoint_clear(dev_hdl, transfer->bEndpointAddress);
    return ESP_OK;
}

/**
 * @brief Start CDC device
 *
 * After this call, USB host peripheral will continuously poll IN endpoints.
 *
 * @param cdc_dev
 * @param[in] event_cb  Device event callback
 * @param[in] in_cb     Data received callback
 * @param[in] user_arg  Optional user's argument, that will be passed to the callbacks
 * @return esp_err_t
 */
static esp_err_t cdc_ecm_start(cdc_dev_t *cdc_dev, cdc_ecm_host_dev_callback_t event_cb, cdc_ecm_data_callback_t in_cb, void *user_arg)
{
    esp_err_t ret = ESP_OK;
    assert(cdc_dev);

    CDC_ECM_ENTER_CRITICAL();
    cdc_dev->notif.cb = event_cb;
    cdc_dev->data.in_cb = in_cb;
    cdc_dev->cb_arg = user_arg;
    CDC_ECM_EXIT_CRITICAL();

    // Claim data interface and start polling its IN endpoint
    ESP_GOTO_ON_ERROR(
        usb_host_interface_claim(
            p_cdc_ecm_obj->cdc_ecm_client_hdl,
            cdc_dev->dev_hdl,
            cdc_dev->data.intf_desc->bInterfaceNumber,
            cdc_dev->data.intf_desc->bAlternateSetting),
        err, TAG, "Could not claim interface");
    if (cdc_dev->data.in_xfer)
    {
        ESP_LOGD(TAG, "Submitting poll for BULK IN transfer (start)");
        ESP_ERROR_CHECK(usb_host_transfer_submit(cdc_dev->data.in_xfer));
    }

    // Claim the BULK OUT endpoint to send data
    if (cdc_dev->data.out_xfer)
    {
        ESP_LOGD(TAG, "Submitting poll for BULK OUT transfer");
        ESP_ERROR_CHECK(usb_host_transfer_submit(cdc_dev->data.out_xfer));
    }
    else
    {
        ESP_LOGE(TAG, "No OUT transfer available to submit!");
    }

    // If notification are supported, claim its interface and start polling its IN endpoint
    if (cdc_dev->notif.xfer)
    {
        if (cdc_dev->notif.intf_desc != cdc_dev->data.intf_desc)
        {
            ESP_GOTO_ON_ERROR(
                usb_host_interface_claim(
                    p_cdc_ecm_obj->cdc_ecm_client_hdl,
                    cdc_dev->dev_hdl,
                    cdc_dev->notif.intf_desc->bInterfaceNumber,
                    cdc_dev->notif.intf_desc->bAlternateSetting),
                err, TAG, "Could not claim interface");
        }
        ESP_LOGD(TAG, "Submitting poll for INTR IN transfer (Start)");
        ESP_ERROR_CHECK(usb_host_transfer_submit(cdc_dev->notif.xfer));
    }

    // Everything OK, add the device into list and return
    CDC_ECM_ENTER_CRITICAL();
    SLIST_INSERT_HEAD(&p_cdc_ecm_obj->cdc_devices_list, cdc_dev, list_entry);
    CDC_ECM_EXIT_CRITICAL();
    return ret;

err:
    usb_host_interface_release(p_cdc_ecm_obj->cdc_ecm_client_hdl, cdc_dev->dev_hdl, cdc_dev->data.intf_desc->bInterfaceNumber);
    if (cdc_dev->notif.xfer && (cdc_dev->notif.intf_desc != cdc_dev->data.intf_desc))
    {
        usb_host_interface_release(p_cdc_ecm_obj->cdc_ecm_client_hdl, cdc_dev->dev_hdl, cdc_dev->notif.intf_desc->bInterfaceNumber);
    }
    return ret;
}

static void cdc_ecm_transfers_free(cdc_dev_t *cdc_dev);
/**
 * @brief Helper function that releases resources claimed by CDC device
 *
 * Close underlying USB device, free device driver memory
 *
 * @note All interfaces claimed by this device must be release before calling this function
 * @param cdc_dev CDC device handle to be removed
 */
static void cdc_ecm_device_remove(cdc_dev_t *cdc_dev)
{
    assert(cdc_dev);
    cdc_ecm_transfers_free(cdc_dev);
    free(cdc_dev->cdc_func_desc);
    // We don't check the error code of usb_host_device_close, as the close might fail, if someone else is still using the device (not all interfaces are released)
    usb_host_device_close(p_cdc_ecm_obj->cdc_ecm_client_hdl, cdc_dev->dev_hdl); // Gracefully continue on error
    free(cdc_dev);
}

/**
 * @brief Open USB device with requested VID/PID
 *
 * This function has two regular return paths:
 * 1. USB device with matching VID/PID is already opened by this driver: allocate new CDC device on top of the already opened USB device.
 * 2. USB device with matching VID/PID is NOT opened by this driver yet: poll USB connected devices until it is found.
 *
 * @note This function will block for timeout_ms, if the device is not enumerated at the moment of calling this function.
 * @param[in] vid Vendor ID
 * @param[in] pid Product ID
 * @param[in] timeout_ms Connection timeout [ms]
 * @param[out] dev CDC-ACM device
 * @return esp_err_t
 */
static esp_err_t cdc_acm_find_and_open_usb_device(uint16_t vid, uint16_t pid, int timeout_ms, cdc_dev_t **dev)
{
    assert(p_cdc_ecm_obj);
    assert(dev);

    *dev = calloc(1, sizeof(cdc_dev_t));
    if (*dev == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    // First, check list of already opened CDC devices
    cdc_dev_t *cdc_dev;
    SLIST_FOREACH(cdc_dev, &p_cdc_ecm_obj->cdc_devices_list, list_entry)
    {
        const usb_device_desc_t *device_desc;
        ESP_ERROR_CHECK(usb_host_get_device_descriptor(cdc_dev->dev_hdl, &device_desc));
        if ((vid == device_desc->idVendor || vid == CDC_HOST_ANY_VID) &&
            (pid == device_desc->idProduct || pid == CDC_HOST_ANY_PID))
        {
            // Return path 1:
            (*dev)->dev_hdl = cdc_dev->dev_hdl;
            return ESP_OK;
        }
    }

    // Second, poll connected devices until new device is connected or timeout
    TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    TimeOut_t connection_timeout;
    vTaskSetTimeOutState(&connection_timeout);

    do
    {
        // ESP_LOGD(TAG, "Checking list of connected USB devices");
        uint8_t dev_addr_list[10];
        int num_of_devices;
        ESP_ERROR_CHECK(usb_host_device_addr_list_fill(sizeof(dev_addr_list), dev_addr_list, &num_of_devices));

        // Go through device address list and find the one we are looking for
        for (int i = 0; i < num_of_devices; i++)
        {
            usb_device_handle_t current_device;
            // Open USB device
            if (usb_host_device_open(p_cdc_ecm_obj->cdc_ecm_client_hdl, dev_addr_list[i], &current_device) != ESP_OK)
            {
                continue; // In case we failed to open this device, continue with next one in the list
            }
            assert(current_device);
            const usb_device_desc_t *device_desc;
            ESP_ERROR_CHECK(usb_host_get_device_descriptor(current_device, &device_desc));
            if ((vid == device_desc->idVendor || vid == CDC_HOST_ANY_VID) &&
                (pid == device_desc->idProduct || pid == CDC_HOST_ANY_PID))
            {
                // Return path 2:
                (*dev)->dev_hdl = current_device;
                return ESP_OK;
            }
            usb_host_device_close(p_cdc_ecm_obj->cdc_ecm_client_hdl, current_device);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    } while (xTaskCheckForTimeOut(&connection_timeout, &timeout_ticks) == pdFALSE);

    // Timeout was reached, clean-up
    free(*dev);
    *dev = NULL;
    return ESP_ERR_NOT_FOUND;
}

esp_err_t cdc_ecm_host_install(const cdc_ecm_host_driver_config_t *driver_config)
{
    CDC_ECM_CHECK(!p_cdc_ecm_obj, ESP_ERR_INVALID_STATE);

    // Check driver configuration, use default if NULL is passed
    if (driver_config == NULL)
    {
        driver_config = &cdc_ecm_driver_config_default;
    }

    // Allocate all we need for this driver
    esp_err_t ret;
    cdc_ecm_obj_t *cdc_ecm_obj = heap_caps_calloc(1, sizeof(cdc_ecm_obj_t), MALLOC_CAP_DEFAULT);
    EventGroupHandle_t event_group = xEventGroupCreate();
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    TaskHandle_t driver_task_h = NULL;
    xTaskCreatePinnedToCore(
        cdc_ecm_client_task, "USB-CDC", driver_config->driver_task_stack_size, NULL,
        driver_config->driver_task_priority, &driver_task_h, driver_config->xCoreID);

    if (cdc_ecm_obj == NULL || driver_task_h == NULL || event_group == NULL || mutex == NULL)
    {
        ret = ESP_ERR_NO_MEM;
        goto err;
    }

    // Register USB Host client
    usb_host_client_handle_t usb_client = NULL;
    const usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 3,
        .async.client_event_callback = usb_event_cb,
        .async.callback_arg = NULL};
    ESP_GOTO_ON_ERROR(usb_host_client_register(&client_config, &usb_client), err, TAG, "Failed to register USB host client");

    // Initialize CDC-ACM driver structure
    SLIST_INIT(&(cdc_ecm_obj->cdc_devices_list));
    cdc_ecm_obj->event_group = event_group;
    cdc_ecm_obj->open_close_mutex = mutex;
    cdc_ecm_obj->cdc_ecm_client_hdl = usb_client;
    cdc_ecm_obj->new_dev_cb = driver_config->new_dev_cb;

    // Between 1st call of this function and following section, another task might try to install this driver:
    // Make sure that there is only one instance of this driver in the system
    CDC_ECM_ENTER_CRITICAL();
    if (p_cdc_ecm_obj)
    {
        // Already created
        ret = ESP_ERR_INVALID_STATE;
        CDC_ECM_EXIT_CRITICAL();
        goto client_err;
    }
    else
    {
        p_cdc_ecm_obj = cdc_ecm_obj;
    }
    CDC_ECM_EXIT_CRITICAL();

    // Everything OK: Start CDC-Driver task and return
    xTaskNotifyGive(driver_task_h);
    return ESP_OK;

client_err:
    usb_host_client_deregister(usb_client);
err: // Clean-up
    free(cdc_ecm_obj);
    if (event_group)
    {
        vEventGroupDelete(event_group);
    }
    if (driver_task_h)
    {
        vTaskDelete(driver_task_h);
    }
    if (mutex)
    {
        vSemaphoreDelete(mutex);
    }
    return ret;
}

esp_err_t cdc_ecm_host_uninstall()
{
    esp_err_t ret;

    CDC_ECM_ENTER_CRITICAL();
    CDC_ECM_CHECK_FROM_CRIT(p_cdc_ecm_obj, ESP_ERR_INVALID_STATE);
    cdc_ecm_obj_t *cdc_ecm_obj = p_cdc_ecm_obj; // Save Driver's handle to temporary handle
    CDC_ECM_EXIT_CRITICAL();

    xSemaphoreTake(p_cdc_ecm_obj->open_close_mutex, portMAX_DELAY); // Wait for all open/close calls to finish

    CDC_ECM_ENTER_CRITICAL();
    if (SLIST_EMPTY(&p_cdc_ecm_obj->cdc_devices_list))
    {                         // Check that device list is empty (all devices closed)
        p_cdc_ecm_obj = NULL; // NULL static driver pointer: No open/close calls form this point
    }
    else
    {
        ret = ESP_ERR_INVALID_STATE;
        CDC_ECM_EXIT_CRITICAL();
        goto unblock;
    }
    CDC_ECM_EXIT_CRITICAL();

    // Signal to CDC task to stop, unblock it and wait for its deletion
    xEventGroupSetBits(cdc_ecm_obj->event_group, CDC_ECM_TEARDOWN);
    usb_host_client_unblock(cdc_ecm_obj->cdc_ecm_client_hdl);
    ESP_GOTO_ON_FALSE(
        xEventGroupWaitBits(cdc_ecm_obj->event_group, CDC_ECM_TEARDOWN_COMPLETE, pdFALSE, pdFALSE, pdMS_TO_TICKS(100)),
        ESP_ERR_NOT_FINISHED, unblock, TAG, );

    // Free remaining resources and return
    vEventGroupDelete(cdc_ecm_obj->event_group);
    xSemaphoreGive(cdc_ecm_obj->open_close_mutex);
    vSemaphoreDelete(cdc_ecm_obj->open_close_mutex);
    free(cdc_ecm_obj);
    return ESP_OK;

unblock:
    xSemaphoreGive(cdc_ecm_obj->open_close_mutex);
    return ret;
}

esp_err_t cdc_ecm_host_register_new_dev_callback(cdc_ecm_new_dev_callback_t new_dev_cb)
{
    CDC_ECM_ENTER_CRITICAL();
    p_cdc_ecm_obj->new_dev_cb = new_dev_cb;
    CDC_ECM_EXIT_CRITICAL();
    return ESP_OK;
}

/**
 * @brief Free USB transfers used by this device
 *
 * @note There can be no transfers in flight, at the moment of calling this function.
 * @param[in] cdc_dev Pointer to CDC device
 */
static void cdc_ecm_transfers_free(cdc_dev_t *cdc_dev) // PMN check this is used
{
    assert(cdc_dev);
    if (cdc_dev->notif.xfer != NULL)
    {
        usb_host_transfer_free(cdc_dev->notif.xfer);
    }
    if (cdc_dev->data.in_xfer != NULL)
    {
        cdc_ecm_reset_in_transfer(cdc_dev);
        usb_host_transfer_free(cdc_dev->data.in_xfer);
    }
    if (cdc_dev->data.out_xfer != NULL)
    {
        if (cdc_dev->data.out_xfer->context != NULL)
        {
            vSemaphoreDelete((SemaphoreHandle_t)cdc_dev->data.out_xfer->context);
        }
        if (cdc_dev->data.out_mux != NULL)
        {
            vSemaphoreDelete(cdc_dev->data.out_mux);
        }
        usb_host_transfer_free(cdc_dev->data.out_xfer);
    }
    if (cdc_dev->ctrl_transfer != NULL)
    {
        if (cdc_dev->ctrl_transfer->context != NULL)
        {
            vSemaphoreDelete((SemaphoreHandle_t)cdc_dev->ctrl_transfer->context);
        }
        if (cdc_dev->ctrl_mux != NULL)
        {
            vSemaphoreDelete(cdc_dev->ctrl_mux);
        }
        usb_host_transfer_free(cdc_dev->ctrl_transfer);
    }
}

/**
 * @brief Allocate CDC transfers
 *
 * @param[in] cdc_dev       Pointer to CDC device
 * @param[in] notif_ep_desc Pointer to notification EP descriptor
 * @param[in] in_ep_desc-   Pointer to data IN EP descriptor
 * @param[in] in_buf_len    Length of data IN buffer
 * @param[in] out_ep_desc   Pointer to data OUT EP descriptor
 * @param[in] out_buf_len   Length of data OUT buffer
 * @return
 *     - ESP_OK:            Success
 *     - ESP_ERR_NO_MEM:    Not enough memory for transfers and semaphores allocation
 *     - ESP_ERR_NOT_FOUND: IN or OUT endpoints were not found in the selected interface
 */
static esp_err_t cdc_ecm_transfers_allocate(cdc_dev_t *cdc_dev, const usb_ep_desc_t *notif_ep_desc, const usb_ep_desc_t *in_ep_desc, size_t in_buf_len, const usb_ep_desc_t *out_ep_desc, size_t out_buf_len)
{
    assert(in_ep_desc);
    assert(out_ep_desc);
    esp_err_t ret;

    // 1. Setup notification transfer if it is supported
    if (notif_ep_desc)
    {
        ESP_LOGD(TAG, "Setting up Notifications transfer on endpoint: 0x%02X, MPS: %d", notif_ep_desc->bEndpointAddress, USB_EP_DESC_GET_MPS(notif_ep_desc));
        ESP_GOTO_ON_ERROR(
            usb_host_transfer_alloc(USB_EP_DESC_GET_MPS(notif_ep_desc), 0, &cdc_dev->notif.xfer),
            err, TAG, );
        cdc_dev->notif.xfer->device_handle = cdc_dev->dev_hdl;
        cdc_dev->notif.xfer->bEndpointAddress = notif_ep_desc->bEndpointAddress;
        cdc_dev->notif.xfer->callback = notif_xfer_cb;
        cdc_dev->notif.xfer->context = cdc_dev;
        cdc_dev->notif.xfer->num_bytes = USB_EP_DESC_GET_MPS(notif_ep_desc);
    }

    // 2. Setup control transfer
    ESP_GOTO_ON_ERROR(
        usb_host_transfer_alloc(CDC_ECM_CTRL_TRANSFER_SIZE, 0, &cdc_dev->ctrl_transfer),
        err, TAG, );
    cdc_dev->ctrl_transfer->timeout_ms = 1000;
    cdc_dev->ctrl_transfer->bEndpointAddress = 0;
    cdc_dev->ctrl_transfer->device_handle = cdc_dev->dev_hdl;
    cdc_dev->ctrl_transfer->callback = out_xfer_cb;
    cdc_dev->ctrl_transfer->context = xSemaphoreCreateBinary();
    ESP_GOTO_ON_FALSE(cdc_dev->ctrl_transfer->context, ESP_ERR_NO_MEM, err, TAG, );
    cdc_dev->ctrl_mux = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(cdc_dev->ctrl_mux, ESP_ERR_NO_MEM, err, TAG, );

    // 3. Setup IN data transfer (if it is required (in_buf_len > 0))
    if (in_buf_len != 0)
    {
        ESP_GOTO_ON_ERROR(
            usb_host_transfer_alloc(in_buf_len, 0, &cdc_dev->data.in_xfer),
            err, TAG, );
        assert(cdc_dev->data.in_xfer);
        cdc_dev->data.in_xfer->callback = in_xfer_cb;
        cdc_dev->data.in_xfer->num_bytes = in_buf_len;
        cdc_dev->data.in_xfer->bEndpointAddress = in_ep_desc->bEndpointAddress;
        cdc_dev->data.in_xfer->device_handle = cdc_dev->dev_hdl;
        cdc_dev->data.in_xfer->context = cdc_dev;
        cdc_dev->data.in_mps = USB_EP_DESC_GET_MPS(in_ep_desc);
        cdc_dev->data.in_data_buffer_base = cdc_dev->data.in_xfer->data_buffer;
    }

    // 4. Setup OUT bulk transfer (if it is required (out_buf_len > 0))
    if (out_buf_len != 0)
    {
        ESP_GOTO_ON_ERROR(
            usb_host_transfer_alloc(out_buf_len, 0, &cdc_dev->data.out_xfer),
            err, TAG, );
        assert(cdc_dev->data.out_xfer);
        cdc_dev->data.out_xfer->device_handle = cdc_dev->dev_hdl;
        cdc_dev->data.out_xfer->context = xSemaphoreCreateBinary();
        ESP_GOTO_ON_FALSE(cdc_dev->data.out_xfer->context, ESP_ERR_NO_MEM, err, TAG, );
        cdc_dev->data.out_mux = xSemaphoreCreateMutex();
        ESP_GOTO_ON_FALSE(cdc_dev->data.out_mux, ESP_ERR_NO_MEM, err, TAG, );
        cdc_dev->data.out_xfer->bEndpointAddress = out_ep_desc->bEndpointAddress;
        cdc_dev->data.out_xfer->callback = out_xfer_cb;
    }
    return ESP_OK;

err:
    cdc_ecm_transfers_free(cdc_dev);
    return ret;
}

esp_err_t cdc_ecm_host_open(uint16_t vid, uint16_t pid, uint8_t interface_idx, const cdc_ecm_host_device_config_t *dev_config, cdc_ecm_dev_hdl_t *cdc_hdl_ret)
{
    esp_err_t ret;
    uint8_t mac_str_idx = 0xff;

    CDC_ECM_CHECK(p_cdc_ecm_obj, ESP_ERR_INVALID_STATE);
    CDC_ECM_CHECK(dev_config, ESP_ERR_INVALID_ARG);
    CDC_ECM_CHECK(cdc_hdl_ret, ESP_ERR_INVALID_ARG);

    xSemaphoreTake(p_cdc_ecm_obj->open_close_mutex, portMAX_DELAY);
    // Find underlying USB device
    cdc_dev_t *cdc_dev;
    ret = cdc_acm_find_and_open_usb_device(vid, pid, dev_config->connection_timeout_ms, &cdc_dev);
    if (ESP_OK != ret)
    {
        goto exit;
    }
    ESP_LOGD(TAG, "Found USB device with VID: 0x%04X, PID: 0x%04X", vid, pid);

    // Get Device and Configuration descriptors
    const usb_config_desc_t *config_desc;
    const usb_device_desc_t *device_desc;
    ESP_ERROR_CHECK(usb_host_get_device_descriptor(cdc_dev->dev_hdl, &device_desc));
    ESP_ERROR_CHECK(usb_host_get_active_config_descriptor(cdc_dev->dev_hdl, &config_desc));

    // Parse the required interface descriptor
    cdc_parsed_info_t cdc_info;
    ESP_GOTO_ON_ERROR(
        cdc_parse_interface_descriptor(device_desc, config_desc, interface_idx, &cdc_info),
        err, TAG, "Could not open required interface as CDC");

    // Save all members of cdc_dev
    cdc_dev->data.intf_desc = cdc_info.data_intf;
    cdc_dev->data_protocol = (cdc_data_protocol_t)cdc_dev->data.intf_desc->bInterfaceProtocol;
    cdc_dev->notif.intf_desc = cdc_info.notif_intf;
    if (cdc_info.notif_intf)
    {
        cdc_dev->comm_protocol = (cdc_comm_protocol_t)cdc_dev->notif.intf_desc->bInterfaceProtocol;
    }
    cdc_dev->cdc_func_desc = cdc_info.func;
    cdc_dev->cdc_func_desc_cnt = cdc_info.func_cnt;

    // The following line is here for backward compatibility with v1.0.*
    // where fixed size of IN buffer (equal to IN Maximum Packet Size) was used
    const size_t in_buf_size = (dev_config->data_cb && (dev_config->in_buffer_size == 0)) ? USB_EP_DESC_GET_MPS(cdc_info.in_ep) : dev_config->in_buffer_size;

    ESP_LOGD(TAG, "CDC-ECM device opened: VID: 0x%04X, PID: 0x%04X, Notification Endpoint: 0x%02X, IN Endpoint: 0x%02X, OUT Endpoint: 0x%02X, bAlternateSetting: %d",
             device_desc->idVendor, device_desc->idProduct, cdc_info.notif_ep->bEndpointAddress, cdc_info.in_ep->bEndpointAddress, cdc_info.out_ep->bEndpointAddress, cdc_dev->data.intf_desc->bAlternateSetting);

    // Allocate USB transfers, claim CDC interfaces and return CDC-ECM handle
    ESP_GOTO_ON_ERROR(
        cdc_ecm_transfers_allocate(cdc_dev, cdc_info.notif_ep, cdc_info.in_ep, in_buf_size, cdc_info.out_ep, dev_config->out_buffer_size),
        err, TAG, ); // TODO: update buffers based on device configuration values.
    ESP_GOTO_ON_ERROR(cdc_ecm_start(cdc_dev, dev_config->event_cb, dev_config->data_cb, dev_config->user_arg), err, TAG, );
    *cdc_hdl_ret = (cdc_ecm_dev_hdl_t)cdc_dev;

    esp_err_t err = cdc_ecm_set_interface(cdc_dev);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set CDC-ECM interface: %s", esp_err_to_name(err));
        goto err;
    }

    const usb_standard_desc_t *desc;
    ret = cdc_ecm_host_cdc_desc_get(cdc_dev, USB_CDC_DESC_SUBTYPE_ETH, &desc);
    if (ret == ESP_OK && desc)
    {

        const cdc_ecm_eth_desc_t *eth_desc = (const cdc_ecm_eth_desc_t *)desc;
        cdc_dev->max_segment_size = eth_desc->wMaxSegmentSize;
        mac_str_idx = eth_desc->iMACAddress;
        if (mac_str_idx == 0xff)
        {
            ESP_LOGE(TAG, "Could not find cdc ecm mac string\r\n");
            goto err;
        }
    }
    else
    {
        ESP_LOGE(TAG, "CDC Ethernet descriptor not found");
    }

    // Optional: Set Ethernet Packet Filter
    // err = cdc_ecm_set_packet_filter(cdc_dev, 0x001C);
    // if (err != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "Failed to set Ethernet Packet Filter: %s", esp_err_to_name(err));
    //     goto err;
    // }

    // Alternative - set MAC address to match that of CDC-ECM device
    // char mac_buffer[32];
    // memset(mac_buffer, 0, 32);
    // ret = cdc_ecm_get_string_desc(cdc_dev, mac_str_idx, mac_buffer);
    // if (ret < 0)
    // {
    //     ESP_LOGE(TAG, "Failed to get MAC address string descriptor: %s", esp_err_to_name(ret));
    //     return ret;
    // }

    // for (int i = 0, j = 0; i < 12; i += 2, j++)
    // {
    //     char byte_str[3] = {mac_buffer[i], mac_buffer[i + 1], '\0'};
    //     uint32_t byte = strtoul(byte_str, NULL, 16);
    //     cdc_dev->mac[j] = (uint8_t)byte;
    // }

    // ESP_LOGI(TAG, "CDC ECM MAC address %02x:%02x:%02x:%02x:%02x:%02x\r\n",
    //          cdc_dev->mac[0], cdc_dev->mac[1], cdc_dev->mac[2], cdc_dev->mac[3], cdc_dev->mac[4], cdc_dev->mac[5]);

    // TODO: only set multicast filtering for MAC address if device supports it
    // err = cdc_ecm_set_multicast_filter(cdc_dev, cdc_dev->mac);
    // if (err != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "Failed to set Multicast Filter: %s", esp_err_to_name(err));
    // }

    xSemaphoreGive(p_cdc_ecm_obj->open_close_mutex);
    return ESP_OK;

err:
    cdc_ecm_device_remove(cdc_dev);
exit:
    xSemaphoreGive(p_cdc_ecm_obj->open_close_mutex);
    *cdc_hdl_ret = NULL;
    return ret;
}

esp_err_t cdc_ecm_host_close(cdc_ecm_dev_hdl_t cdc_hdl)
{
    CDC_ECM_CHECK(p_cdc_ecm_obj, ESP_ERR_INVALID_STATE);
    CDC_ECM_CHECK(cdc_hdl, ESP_ERR_INVALID_ARG);

    xSemaphoreTake(p_cdc_ecm_obj->open_close_mutex, portMAX_DELAY);

    // Make sure that the device is in the devices list (that it is not already closed)
    cdc_dev_t *cdc_dev;
    bool device_found = false;
    CDC_ECM_ENTER_CRITICAL();
    SLIST_FOREACH(cdc_dev, &p_cdc_ecm_obj->cdc_devices_list, list_entry)
    {
        if (cdc_dev == (cdc_dev_t *)cdc_hdl)
        {
            device_found = true;
            break;
        }
    }

    // Device was not found in the cdc_devices_list; it was already closed, return OK
    if (!device_found)
    {
        CDC_ECM_EXIT_CRITICAL();
        xSemaphoreGive(p_cdc_ecm_obj->open_close_mutex);
        return ESP_OK;
    }

    // No user callbacks from this point
    cdc_dev->notif.cb = NULL;
    cdc_dev->data.in_cb = NULL;
    CDC_ECM_EXIT_CRITICAL();

    // Cancel polling of BULK IN and INTERRUPT IN
    if (cdc_dev->data.in_xfer)
    {
        ESP_ERROR_CHECK(cdc_acm_reset_transfer_endpoint(cdc_dev->dev_hdl, cdc_dev->data.in_xfer));
    }
    if (cdc_dev->notif.xfer != NULL)
    {
        ESP_ERROR_CHECK(cdc_acm_reset_transfer_endpoint(cdc_dev->dev_hdl, cdc_dev->notif.xfer));
    }

    // Release all interfaces
    ESP_ERROR_CHECK(usb_host_interface_release(p_cdc_ecm_obj->cdc_ecm_client_hdl, cdc_dev->dev_hdl, cdc_dev->data.intf_desc->bInterfaceNumber));
    if ((cdc_dev->notif.intf_desc != NULL) && (cdc_dev->notif.intf_desc != cdc_dev->data.intf_desc))
    {
        ESP_ERROR_CHECK(usb_host_interface_release(p_cdc_ecm_obj->cdc_ecm_client_hdl, cdc_dev->dev_hdl, cdc_dev->notif.intf_desc->bInterfaceNumber));
    }

    CDC_ECM_ENTER_CRITICAL();
    SLIST_REMOVE(&p_cdc_ecm_obj->cdc_devices_list, cdc_dev, cdc_dev_s, list_entry);
    CDC_ECM_EXIT_CRITICAL();

    cdc_ecm_device_remove(cdc_dev);
    xSemaphoreGive(p_cdc_ecm_obj->open_close_mutex);
    return ESP_OK;
}

void cdc_ecm_host_desc_print(cdc_ecm_dev_hdl_t cdc_hdl)
{
    assert(cdc_hdl);
    cdc_dev_t *cdc_dev = (cdc_dev_t *)cdc_hdl;

    const usb_device_desc_t *device_desc;
    const usb_config_desc_t *config_desc;
    ESP_ERROR_CHECK_WITHOUT_ABORT(usb_host_get_device_descriptor(cdc_dev->dev_hdl, &device_desc));
    ESP_ERROR_CHECK_WITHOUT_ABORT(usb_host_get_active_config_descriptor(cdc_dev->dev_hdl, &config_desc));
    usb_print_device_descriptor(device_desc);
    usb_print_config_descriptor(config_desc, cdc_print_desc);
}

/**
 * @brief Retrieve MAC address from cdc_hdl
 *
 *
 * @param cdc_hdl CDC device handle
 * @param[in] mac_addr MAC address buffer
 * @return true Transfer completed
 * @return false Transfer NOT completed
 */
esp_err_t cdc_ecm_get_mac_addr(cdc_ecm_dev_hdl_t cdc_hdl, uint8_t *mac_addr)
{
    CDC_ECM_CHECK(cdc_hdl, ESP_ERR_INVALID_ARG);
    cdc_dev_t *cdc_dev = (cdc_dev_t *)cdc_hdl;
    // ESP_LOGD(TAG, "Set MAC Address in get_mac_addr: %02X:%02X:%02X:%02X:%02X:%02X",
    //  cdc_dev->mac[0], cdc_dev->mac[1], cdc_dev->mac[2], cdc_dev->mac[3], cdc_dev->mac[4], cdc_dev->mac[5]);
    memcpy(mac_addr, cdc_dev->mac, 6); // assuming ESP_MAC_ADDR_LEN is defined

    return ESP_OK;
}

/**
 * @brief Retrieve connection status  from cdc_hdl
 *
 *
 * @param cdc_hdl CDC device handle
 * @return true Device is connected
 * @return false Devics is NOT completed
 */
bool cdc_ecm_get_connection_status(cdc_ecm_dev_hdl_t cdc_hdl)
{
    CDC_ECM_CHECK(cdc_hdl, ESP_ERR_INVALID_ARG);
    cdc_dev_t *cdc_dev = (cdc_dev_t *)cdc_hdl;
    if (cdc_dev->connect_status)
    {
        return true;
    }
    return false;
}

/**
 * @brief Check finished transfer status
 *
 * Return to on transfer completed OK.
 * Cancel the transfer and issue user's callback in case of an error.
 *
 * @param[in] transfer Transfer to be checked
 * @return true Transfer completed
 * @return false Transfer NOT completed
 */
static bool cdc_ecm_is_transfer_completed(usb_transfer_t *transfer)
{
    cdc_dev_t *cdc_dev = (cdc_dev_t *)transfer->context;
    bool completed = false;

    switch (transfer->status)
    {
    case USB_TRANSFER_STATUS_COMPLETED:
        completed = true;
        break;
    case USB_TRANSFER_STATUS_NO_DEVICE: // User is notified about device disconnection from usb_event_cb
    case USB_TRANSFER_STATUS_CANCELED:
        break;
    case USB_TRANSFER_STATUS_ERROR:
    case USB_TRANSFER_STATUS_TIMED_OUT:
    case USB_TRANSFER_STATUS_STALL:
    case USB_TRANSFER_STATUS_OVERFLOW:
    case USB_TRANSFER_STATUS_SKIPPED:
    default:
        // Transfer was not completed or cancelled by user. Inform user about this
        if (cdc_dev->notif.cb)
        {
            const cdc_ecm_host_dev_event_data_t error_event = {
                .type = CDC_ECM_HOST_EVENT_ERROR,
                .data.error = (int)transfer->status};
            cdc_dev->notif.cb(&error_event, cdc_dev->cb_arg);
        }
    }
    return completed;
}

static void in_xfer_cb(usb_transfer_t *transfer)
{
    ESP_LOGV(TAG, "in xfer cb");
    cdc_dev_t *cdc_dev = (cdc_dev_t *)transfer->context;

    if (!cdc_ecm_is_transfer_completed(transfer))
    {
        return;
    }

    if (cdc_dev->data.in_cb)
    {
        const bool data_processed = cdc_dev->data.in_cb(transfer->data_buffer, transfer->actual_num_bytes, cdc_dev->cb_arg);

        // Information for developers:
        // In order to save RAM and CPU time, the application can indicate that the received data was not processed and that the application expects more data.
        // In this case, the next received data must be appended to the existing buffer.
        // Since the data_buffer in usb_transfer_t is a constant pointer, we must cast away to const qualifier.
        if (!data_processed)
        {
#if !SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE
            // In case the received data was not processed, the next RX data must be appended to current buffer
            uint8_t **ptr = (uint8_t **)(&(transfer->data_buffer));
            *ptr += transfer->actual_num_bytes;

            // Calculate remaining space in the buffer. Attention: pointer arithmetic!
            size_t space_left = transfer->data_buffer_size - (transfer->data_buffer - cdc_dev->data.in_data_buffer_base);
            uint16_t mps = cdc_dev->data.in_mps;
            transfer->num_bytes = (space_left / mps) * mps; // Round down to MPS for next transfer

            if (transfer->num_bytes == 0)
            {
                // The IN buffer cannot accept more data, inform the user and reset the buffer
                ESP_LOGW(TAG, "IN buffer overflow");
                // cdc_dev->serial_state.bOverRun = true;
                // TODO: work out if we need to do anything with this
                //  if (cdc_dev->notif.cb)
                //  {
                //      const cdc_ecm_host_dev_event_data_t serial_state_event = {
                //          .type = CDC_ACM_HOST_SERIAL_STATE,
                //          .data.serial_state = cdc_dev->serial_state};
                //      cdc_dev->notif.cb(&serial_state_event, cdc_dev->cb_arg);
                //  }

                cdc_ecm_reset_in_transfer(cdc_dev);
                // cdc_dev->serial_state.bOverRun = false;
            }
#else
            // For targets that must sync internal memory through L1CACHE, we cannot change the data_buffer
            // because it would lead to unaligned cache sync, which is not allowed
            ESP_LOGW(TAG, "RX buffer append is not yet supported on ESP32-P4!");
#endif
        }
        else
        {
            cdc_ecm_reset_in_transfer(cdc_dev);
        }
    }

    ESP_LOGV(TAG, "Submitting poll for BULK IN transfer");
    usb_host_transfer_submit(cdc_dev->data.in_xfer);
}

static void notif_xfer_cb(usb_transfer_t *transfer)
{
    ESP_LOGV(TAG, "notif xfer cb");
    cdc_dev_t *cdc_dev = (cdc_dev_t *)transfer->context;

    if (cdc_ecm_is_transfer_completed(transfer))
    {
        cdc_notification_t *notif = (cdc_notification_t *)transfer->data_buffer;
        switch (notif->bNotificationCode)
        {
        case USB_CDC_NOTIF_NETWORK_CONNECTION:
        {
            if (cdc_dev->notif.cb)
            {
                const cdc_ecm_host_dev_event_data_t net_conn_event = {
                    .type = CDC_ECM_HOST_EVENT_NETWORK_CONNECTION,
                    .data.network_connected = (bool)notif->wValue};
                cdc_dev->notif.cb(&net_conn_event, cdc_dev->cb_arg);
                cdc_dev->connect_status = notif->wValue;
            }
            break;
        }
        case USB_CDC_NOTIF_CONNECTION_SPEED_CHANGE:
        {
            if (cdc_dev->notif.cb)
            {

                if (notif->wLength == sizeof(cdc_ecm_speed_change_data_t))
                {
                    cdc_ecm_speed_change_data_t *speed_data = (cdc_ecm_speed_change_data_t *)(notif->Data);

                    // uint32_t rx_speed = speed_data->downlink_speed; // RX Speed (Little Endian)
                    uint32_t tx_speed = speed_data->uplink_speed; // TX Speed (Little Endian)

                    // printf("CDC-ECM Speed Change: RX = %lu bps, TX = %lu bps\n", rx_speed, tx_speed);
                    const cdc_ecm_host_dev_event_data_t speed_change_event = {
                        .type = CDC_ECM_HOST_EVENT_SPEED_CHANGE,
                        .data.link_speed = tx_speed};
                    cdc_dev->notif.cb(&speed_change_event, cdc_dev->cb_arg);
                }
                else
                {
                    ESP_LOGE(TAG, "Unexpected wLength for Speed Change Notification: %d\n", notif->wLength);
                }

                // ESP_LOGI(TAG, "Link Speed: %d", notif->wValue);
                // const cdc_ecm_host_dev_event_data_t speed_change_event = {
                //     .type = CDC_ECM_HOST_EVENT_SPEED_CHANGE,
                //     .data.link_speed = notif->wValue};
                // cdc_dev->notif.cb(&speed_change_event, cdc_dev->cb_arg);
            }
            break;
        }
        default:

            ESP_LOGW(TAG, "Unsupported notification type 0x%02X", notif->bNotificationCode);
            ESP_LOG_BUFFER_HEX(TAG, transfer->data_buffer, transfer->actual_num_bytes);
            break;
        }

        // Start polling for new data again
        ESP_LOGV(TAG, "Submitting poll for INTR IN transfer");
        usb_host_transfer_submit(cdc_dev->notif.xfer);
    }
}

static void out_xfer_cb(usb_transfer_t *transfer)
{
    // ESP_LOGD(TAG, "out/ctrl xfer cb");
    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED)
    {
        ESP_LOGV(TAG, "Bulk OUT transfer completed successfully, transferred %d bytes", transfer->actual_num_bytes);
    }
    else
    {
        ESP_LOGE(TAG, "Bulk OUT transfer failed with status: %d", transfer->status);
    }
    if (transfer->context)
    {
        xSemaphoreGive((SemaphoreHandle_t)transfer->context);
    }
}

static void usb_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg)
{
    switch (event_msg->event)
    {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
        // Guard p_cdc_ecm_obj->new_dev_cb from concurrent access
        ESP_LOGD(TAG, "New device connected");
        CDC_ECM_ENTER_CRITICAL();
        cdc_ecm_new_dev_callback_t _new_dev_cb = p_cdc_ecm_obj->new_dev_cb;
        CDC_ECM_EXIT_CRITICAL();

        if (_new_dev_cb)
        {
            usb_device_handle_t new_dev;
            if (usb_host_device_open(p_cdc_ecm_obj->cdc_ecm_client_hdl, event_msg->new_dev.address, &new_dev) != ESP_OK)
            {
                break;
            }
            assert(new_dev);
            _new_dev_cb(new_dev);
            usb_host_device_close(p_cdc_ecm_obj->cdc_ecm_client_hdl, new_dev);
        }

        break;
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
    {
        ESP_LOGD(TAG, "Device suddenly disconnected");
        // Find CDC pseudo-devices associated with this USB device and close them
        cdc_dev_t *cdc_dev;
        cdc_dev_t *tcdc_dev;
        // We are using 'SAFE' version of 'SLIST_FOREACH' which enables user to close the disconnected device in the callback
        SLIST_FOREACH_SAFE(cdc_dev, &p_cdc_ecm_obj->cdc_devices_list, list_entry, tcdc_dev)
        {
            if (cdc_dev->dev_hdl == event_msg->dev_gone.dev_hdl && cdc_dev->notif.cb)
            {
                // The suddenly disconnected device was opened by this driver: inform user about this
                const cdc_ecm_host_dev_event_data_t disconn_event = {
                    .type = CDC_ECM_HOST_EVENT_DISCONNECTED,
                    .data.cdc_hdl = (cdc_ecm_dev_hdl_t)cdc_dev,
                };
                cdc_dev->notif.cb(&disconn_event, cdc_dev->cb_arg);
            }
        }
        break;
    }
    default:
        assert(false);
        break;
    }
}

esp_err_t cdc_ecm_host_data_tx_blocking(cdc_ecm_dev_hdl_t cdc_hdl, const uint8_t *data, size_t data_len, uint32_t timeout_ms)
{
    esp_err_t ret;
    CDC_ECM_CHECK(cdc_hdl, ESP_ERR_INVALID_ARG);
    cdc_dev_t *cdc_dev = (cdc_dev_t *)cdc_hdl;
    CDC_ECM_CHECK(data && (data_len > 0), ESP_ERR_INVALID_ARG);
    CDC_ECM_CHECK(cdc_dev->data.out_xfer, ESP_ERR_NOT_SUPPORTED); // Device was opened as read-only.
    CDC_ECM_CHECK(data_len <= cdc_dev->data.out_xfer->data_buffer_size, ESP_ERR_INVALID_SIZE);

    if (!cdc_dev->connect_status)
    {
        return ESP_ERR_INVALID_STATE;
    }

    // Take OUT mutex and fill the OUT transfer
    BaseType_t taken = xSemaphoreTake(cdc_dev->data.out_mux, pdMS_TO_TICKS(timeout_ms));
    if (taken != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take OUT mutex");
        return ESP_ERR_TIMEOUT;
    }

    SemaphoreHandle_t transfer_finished_semaphore = (SemaphoreHandle_t)cdc_dev->data.out_xfer->context;
    xSemaphoreTake(transfer_finished_semaphore, 0); // Make sure the semaphore is taken before we submit new transfer

    memcpy(cdc_dev->data.out_xfer->data_buffer, data, data_len);
    cdc_dev->data.out_xfer->num_bytes = data_len;
    cdc_dev->data.out_xfer->timeout_ms = timeout_ms;

    if (cdc_dev->data.out_xfer->bEndpointAddress != 0x02)
    {
        ESP_LOGE(TAG, "Endpoint address is not 0x02");
        return ESP_ERR_INVALID_ARG;
    };

    ret = usb_host_transfer_submit(cdc_dev->data.out_xfer);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "usb_host_transfer_submit failed: %s", esp_err_to_name(ret));
        // let lwIP drop this frame
        goto unblock;
    }

    // Wait for OUT transfer completion
    taken = xSemaphoreTake(transfer_finished_semaphore, pdMS_TO_TICKS(timeout_ms));
    if (!taken)
    {
        cdc_acm_reset_transfer_endpoint(cdc_dev->dev_hdl, cdc_dev->data.out_xfer); // Resetting the endpoint will cause all in-progress transfers to complete
        ESP_LOGW(TAG, "TX transfer timeout");
        ret = ESP_ERR_TIMEOUT;
        goto unblock;
    }

    ESP_GOTO_ON_FALSE(cdc_dev->data.out_xfer->status == USB_TRANSFER_STATUS_COMPLETED, ESP_ERR_INVALID_RESPONSE, unblock, TAG, "Bulk OUT transfer error");
    ESP_GOTO_ON_FALSE(cdc_dev->data.out_xfer->actual_num_bytes == data_len, ESP_ERR_INVALID_RESPONSE, unblock, TAG, "Incorrect number of bytes transferred");
    ret = ESP_OK;

unblock:
    xSemaphoreGive(cdc_dev->data.out_mux);
    return ret;
}

esp_err_t cdc_ecm_set_interface(cdc_ecm_dev_hdl_t cdc_hdl)
{
    cdc_dev_t *cdc_dev = (cdc_dev_t *)cdc_hdl;

    uint8_t bAlternateSetting = cdc_dev->data.intf_desc->bAlternateSetting;

    ESP_LOGD(TAG, "Setting Ethernet Interface using bAlternateSetting: %d, bInterfaceNumber %d", bAlternateSetting, cdc_dev->data.intf_desc->bInterfaceNumber);

    esp_err_t err =
        cdc_ecm_host_send_custom_request(
            cdc_hdl,
            USB_BM_REQUEST_TYPE_DIR_OUT | USB_BM_REQUEST_TYPE_TYPE_STANDARD | USB_BM_REQUEST_TYPE_RECIP_INTERFACE,
            USB_B_REQUEST_SET_INTERFACE,
            bAlternateSetting,
            cdc_dev->data.intf_desc->bInterfaceNumber,
            0,
            NULL);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Unable to set Ethernet Interface: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t cdc_ecm_set_packet_filter(cdc_ecm_dev_hdl_t cdc_hdl, uint16_t filter_mask)
{
    CDC_ECM_CHECK(filter_mask, ESP_ERR_INVALID_ARG);
    cdc_dev_t *cdc_dev = (cdc_dev_t *)cdc_hdl;

    // Ensure alignment by using a local variable
    uint16_t aligned_filter_mask = filter_mask;

    ESP_LOGD(TAG, "Setting Ethernet Packet Filter: 0x%04X", filter_mask);

    esp_err_t err =
        cdc_ecm_host_send_custom_request(
            cdc_hdl,
            USB_BM_REQUEST_TYPE_DIR_OUT | USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE,
            USB_CDC_REQ_SET_ETHERNET_PACKET_FILTER,
            aligned_filter_mask,
            cdc_dev->data.intf_desc->bInterfaceNumber,
            0,
            NULL);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Unable to set Packet Filter: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t cdc_ecm_set_multicast_filter(cdc_ecm_dev_hdl_t cdc_hdl, uint8_t *mac)
{
    CDC_ECM_CHECK(mac, ESP_ERR_INVALID_ARG);
    cdc_dev_t *cdc_dev = (cdc_dev_t *)cdc_hdl;

    // ESP_LOGD(TAG, "Setting Ethernet Multicast Filter: 0x%04X", filter_mask);
    ESP_LOGI(TAG, "Setting Ethernet Multicast Filter: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    esp_err_t err =
        cdc_ecm_host_send_custom_request(
            cdc_hdl,
            USB_BM_REQUEST_TYPE_DIR_OUT | USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE,
            USB_CDC_REQ_SET_ETHERNET_MULTICAST_FILTERS,
            0,
            cdc_dev->data.intf_desc->bInterfaceNumber,
            sizeof(mac) * 6,
            (uint8_t *)&mac);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Unable to set Multicast Filter: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t cdc_ecm_get_string_desc(cdc_ecm_dev_hdl_t cdc_hdl, uint16_t index, uint8_t *output)
{
    CDC_ECM_CHECK(index, ESP_ERR_INVALID_ARG);
    CDC_ECM_CHECK(output, ESP_ERR_INVALID_ARG);

    uint8_t data[32]; // temp buffer to retrieve string descriptor

    ESP_LOGD(TAG, "Getting String Descriptor for ID: %d", index);

    esp_err_t err =
        cdc_ecm_host_send_custom_request(
            cdc_hdl,
            USB_BM_REQUEST_TYPE_DIR_IN | USB_BM_REQUEST_TYPE_TYPE_STANDARD | USB_BM_REQUEST_TYPE_RECIP_DEVICE,
            USB_CDC_REQ_GET_DESCRIPTOR,
            (uint16_t)((USB_DESCRIPTOR_TYPE_STRING << 8) | index),
            0x0409, // language type for US English
            32,
            data);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Unable to get String Descriptor: %s", esp_err_to_name(err));
        return err;
    }

    // The first byte in the descriptor is its total length.
    uint8_t len = data[0];

    // USB string descriptors are UTF-16LE encoded. The first two bytes are descriptor length and type.
    // We'll start at index 2 and copy every other byte to get the low byte (assuming ASCII).
    uint16_t i = 2;
    uint16_t j = 0;
    while (i < len)
    {
        output[j] = data[i];
        i += 2;
        j++;
    }
    output[j] = '\0';

    return ESP_OK;
}

esp_err_t cdc_ecm_host_send_custom_request(cdc_ecm_dev_hdl_t cdc_hdl, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength, uint8_t *data)
{
    CDC_ECM_CHECK(cdc_hdl, ESP_ERR_INVALID_ARG);

    cdc_dev_t *cdc_dev = (cdc_dev_t *)cdc_hdl;
    if (wLength > 0)
    {
        ESP_LOGI(TAG, "checking custom request data: %d", data[0]);
        CDC_ECM_CHECK(data, ESP_ERR_INVALID_ARG);
        ESP_LOGI(TAG, "Setting Custom Data: %02X:%02X:%02X:%02X:%02X:%02X",
                 data[0], data[1], data[2], data[3], data[4], data[5]);
    }
    CDC_ECM_CHECK(cdc_dev->ctrl_transfer->data_buffer_size >= wLength, ESP_ERR_INVALID_SIZE); // TODO: need to reinstte this

    esp_err_t ret;

    // Take Mutex and fill the CTRL request
    BaseType_t taken = xSemaphoreTake(cdc_dev->ctrl_mux, pdMS_TO_TICKS(CDC_ECM_CTRL_TIMEOUT_MS));
    if (!taken)
    {
        return ESP_ERR_TIMEOUT;
    }
    usb_setup_packet_t *req = (usb_setup_packet_t *)(cdc_dev->ctrl_transfer->data_buffer);
    uint8_t *start_of_data = (uint8_t *)req + sizeof(usb_setup_packet_t);
    req->bmRequestType = bmRequestType;
    req->bRequest = bRequest;
    req->wValue = wValue;
    req->wIndex = wIndex;
    req->wLength = wLength;

    // For IN transfers we must transfer data ownership to CDC driver
    const bool in_transfer = bmRequestType & USB_BM_REQUEST_TYPE_DIR_IN;
    if (!in_transfer && wLength)
    {
        memcpy(start_of_data, data, wLength);
    }

    cdc_dev->ctrl_transfer->num_bytes = wLength + sizeof(usb_setup_packet_t);
    ESP_GOTO_ON_ERROR(
        usb_host_transfer_submit_control(p_cdc_ecm_obj->cdc_ecm_client_hdl, cdc_dev->ctrl_transfer),
        unblock, TAG, "CTRL transfer failed");

    taken = xSemaphoreTake((SemaphoreHandle_t)cdc_dev->ctrl_transfer->context, pdMS_TO_TICKS(CDC_ECM_CTRL_TIMEOUT_MS));
    if (!taken)
    {
        // Transfer was not finished, error in USB LIB. Reset the endpoint
        cdc_acm_reset_transfer_endpoint(cdc_dev->dev_hdl, cdc_dev->ctrl_transfer);
        ret = ESP_ERR_TIMEOUT;
        goto unblock;
    }

    ESP_GOTO_ON_FALSE(cdc_dev->ctrl_transfer->status == USB_TRANSFER_STATUS_COMPLETED, ESP_ERR_INVALID_RESPONSE, unblock, TAG, "Control transfer error");
    // For OUT transfers, enforce exact size; for IN transfers, allow a smaller transfer.
    if (in_transfer)
    {
        // Ensure we have at least the header plus one byte of data.
        ESP_GOTO_ON_FALSE(cdc_dev->ctrl_transfer->actual_num_bytes >= (sizeof(usb_setup_packet_t) + 1),
                          ESP_ERR_INVALID_RESPONSE, unblock, TAG, "Insufficient data transferred");
        uint16_t actual_data_length = cdc_dev->ctrl_transfer->actual_num_bytes - sizeof(usb_setup_packet_t);
        uint16_t copy_length = (actual_data_length < wLength) ? actual_data_length : wLength;
        memcpy(data, start_of_data, copy_length);
    }
    else
    {
        ESP_GOTO_ON_FALSE(cdc_dev->ctrl_transfer->actual_num_bytes == cdc_dev->ctrl_transfer->num_bytes,
                          ESP_ERR_INVALID_RESPONSE, unblock, TAG, "Incorrect number of bytes transferred");
    }
    ret = ESP_OK;
    // For OUT transfers, we must transfer data ownership to user
    if (in_transfer)
    {
        memcpy(data, start_of_data, wLength);
    }
    ret = ESP_OK;

unblock:
    xSemaphoreGive(cdc_dev->ctrl_mux);
    return ret;
}

esp_err_t cdc_ecm_host_cdc_desc_get(cdc_ecm_dev_hdl_t cdc_hdl, cdc_desc_subtype_t desc_type, const usb_standard_desc_t **desc_out)
{
    CDC_ECM_CHECK(cdc_hdl, ESP_ERR_INVALID_ARG);
    CDC_ECM_CHECK(desc_type < USB_CDC_DESC_SUBTYPE_MAX, ESP_ERR_INVALID_ARG);
    cdc_dev_t *cdc_dev = (cdc_dev_t *)cdc_hdl;
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    *desc_out = NULL;

    for (int i = 0; i < cdc_dev->cdc_func_desc_cnt; i++)
    {
        const cdc_header_desc_t *_desc = (const cdc_header_desc_t *)((*(cdc_dev->cdc_func_desc))[i]);
        if (_desc->bDescriptorSubtype == desc_type)
        {
            ret = ESP_OK;
            *desc_out = (const usb_standard_desc_t *)_desc;
            break;
        }
    }
    return ret;
}

/**
 * @brief Data received callback
 * PN MOVE TO CDC ECM HOST
 *
 * @param[in] data     Pointer to received data
 * @param[in] data_len Length of received data in bytes
 * @param[in] arg      Argument we passed to the device open function
 * @return
 *   true:  We have processed the received data
 *   false: We expect more data
 */
static bool handle_rx(const uint8_t *data, size_t data_len, void *arg)
{
    if (usb_netif == NULL)
    {
        return false;
    }

    // Allocate a buffer for the received data
    uint8_t *rx_buffer = (uint8_t *)heap_caps_malloc(data_len, MALLOC_CAP_8BIT);
    if (!rx_buffer)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for RX buffer");
        return false;
    }
    memcpy(rx_buffer, data, data_len); // Copy data into a persistent buffer

    // Pass the copied buffer instead of the original `data`
    esp_err_t err = esp_netif_receive(usb_netif, rx_buffer, data_len, NULL);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to receive data: %s", esp_err_to_name(err));
        free(rx_buffer); // Free allocated buffer on failure
        return false;
    }
    return true;
}

/**
 * @brief Device event callback
 * PN MOVE TO CDC ECM HOST
 *
 * Apart from handling device disconnection it doesn't do anything useful
 *
 * @param[in] event    Device event type and data
 * @param[in] user_ctx Argument we passed to the device open function
 */
static void cdc_ecm_host_device_event_handler(const cdc_ecm_host_dev_event_data_t *event, void *user_ctx)
{
    switch (event->type)
    {
    case CDC_ECM_HOST_EVENT_ERROR:
        ESP_LOGE(TAG, "CDC-ACM error has occurred, err_no = %i", event->data.error);
        break;
    case CDC_ECM_HOST_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Device suddenly disconnected");
        esp_netif_action_disconnected(usb_netif, NULL, 0, NULL);
        esp_event_post(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &usb_netif, sizeof(esp_netif_t *), portMAX_DELAY);
        xSemaphoreGive(device_disconnected_sem);
        break;
    case CDC_ECM_HOST_EVENT_SPEED_CHANGE:
        if (event->data.link_speed != link_speed)
        {
            link_speed = event->data.link_speed;
        }
        break;
    case CDC_ECM_HOST_EVENT_NETWORK_CONNECTION:
        if (event->data.network_connected != network_connected)
        {
            network_connected = event->data.network_connected;
            if (!usb_netif)
            {
                return;
            }
            if (network_connected)
            {
                esp_netif_action_connected(usb_netif, NULL, 0, NULL);
                esp_event_post(ETH_EVENT, ETHERNET_EVENT_CONNECTED, &usb_netif, sizeof(esp_netif_t *), portMAX_DELAY);
            }
            else
            {
                esp_netif_action_disconnected(usb_netif, NULL, 0, NULL);
                esp_event_post(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &usb_netif, sizeof(esp_netif_t *), portMAX_DELAY);
            }
        }
        break;
    default:
        ESP_LOGW(TAG, "Unsupported CDC event: %i", event->type);
        break;
    }
}

/**
 * @brief USB Host library handling task
 * PN MOVE TO CDC ECM HOST
 *
 * @param arg Unused
 */
static void usb_lib_task(void *arg)
{
    while (1)
    {
        // Start handling system events
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
        {
            esp_err_t err = usb_host_device_free_all();
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to free all devices: %s", esp_err_to_name(err));
            }
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE)
        {
            ESP_LOGD(TAG, "USB: All devices freed");
            // Continue handling USB events to allow device reconnection
        }
    }
}

/**
 * @brief Callback from cdc_ecm_netif_init usb driver config for sending data over usb.
 *
 */
static esp_err_t netif_transmit(void *h, void *buffer, size_t len)
{
    cdc_ecm_dev_hdl_t cdc_dev = (cdc_ecm_dev_hdl_t)h;
    size_t out_buf_len = cdc_dev->max_segment_size; // TODO: need to link this to buffer limits from config.

    if (cdc_dev == NULL)
    {
        ESP_LOGE(TAG, "CDC device handle is NULL!");
        return ESP_FAIL;
    }

    const uint8_t *data_ptr = (const uint8_t *)buffer;
    size_t remaining_len = len;

    while (remaining_len > 0)
    {
        size_t chunk_len = remaining_len > out_buf_len ? out_buf_len : remaining_len;

        esp_err_t err = cdc_ecm_host_data_tx_blocking(cdc_dev, data_ptr, chunk_len, 100);
        if (err != ESP_OK) {
            // Log once per attempt
            ESP_LOGW(TAG, "netif_transmit: dropping frame chunk, tx_blocking failed: %s",
                     esp_err_to_name(err));
            // Drop this entire frame; returning OK here prevents lwIP from panicking.
            return ESP_OK;
        }

        data_ptr += chunk_len;
        remaining_len -= chunk_len;
    }

    return ESP_OK;
}

static void l2_free(void *h, void *buffer)
{
    free(buffer);
    return;
}

/**
 * @brief Netif Initialisation
 *
 */
esp_err_t cdc_ecm_netif_init(cdc_ecm_dev_hdl_t cdc_hdl, cdc_ecm_params_t *params)
{
    // esp_netif_init();
    // esp_event_loop_create_default();

    ESP_LOGI(TAG, "Initialising cdc_ecm_netif_init");

    esp_netif_ip_info_t ip_info = {0};

    // Set flags based on whether DHCP should be enabled
    esp_netif_flags_t netif_flags = ESP_NETIF_FLAG_EVENT_IP_MODIFIED | ESP_NETIF_FLAG_AUTOUP;
    if (!params->disable_dhcp) {
        netif_flags |= ESP_NETIF_DHCP_CLIENT;
    }

    esp_netif_inherent_config_t base_cfg = {
        .flags = netif_flags,
        .ip_info = &ip_info,
        .get_ip_event = IP_EVENT_ETH_GOT_IP,
        .lost_ip_event = IP_EVENT_ETH_LOST_IP,
        .if_key = "ETH_DEF",
        .if_desc = "usb cdc ecm host device",
        .route_prio = 10};

    if (params->if_key)
    {
        base_cfg.if_key = params->if_key;
    }
    if (params->if_desc)
    {
        base_cfg.if_desc = params->if_desc;
    }

    esp_netif_driver_ifconfig_t driver_cfg = {
        .handle = cdc_hdl,
        .transmit = netif_transmit,
        .driver_free_rx_buffer = l2_free};

    esp_netif_config_t cfg = {
        .base = &base_cfg,
        .driver = &driver_cfg,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH,
    };

    usb_netif = esp_netif_new(&cfg);
    if (usb_netif == NULL)
    {
        return ESP_FAIL;
    }

    esp_err_t err = esp_read_mac(cdc_dev->mac, ESP_MAC_ETH);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get MAC address: %s", esp_err_to_name(err));
        return err;
    }
    esp_netif_set_mac(usb_netif, cdc_dev->mac);

    // Only start DHCP if needed
    if (!params->disable_dhcp) {
        err = esp_netif_dhcpc_start(usb_netif);
        if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED)
        {
            ESP_LOGE(TAG, "Failed to start DHCP client: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGI(TAG, "DHCP disabled per configuration (manual IP will be used)");
    }

    if (params->hostname)
    {
        ESP_LOGI(TAG, "Setting hostname: %s", params->hostname);
        err = esp_netif_set_hostname(usb_netif, params->hostname);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set hostname, error: %s", esp_err_to_name(err));
        }
        // free(params->hostname); //not freed - needed in event of reconnection.
    }

    if (params->nameserver)
    {
        if (strcmp(params->nameserver, "") == 0)
        {
            // Set DNS Server
            esp_netif_dns_info_t dns;
            dns.ip.u_addr.ip4.addr = ipaddr_addr(params->nameserver);
            dns.ip.type = IPADDR_TYPE_V4;
            ESP_ERROR_CHECK(esp_netif_set_dns_info(usb_netif, ESP_NETIF_DNS_MAIN, &dns));
            ESP_LOGI(TAG, "Setting nameserver: %s", params->nameserver);
        }
        free(params->nameserver);
    }

    esp_netif_action_start(usb_netif, 0, 0, 0);

    if (cdc_ecm_get_connection_status(cdc_dev))
        esp_netif_action_connected(usb_netif, NULL, 0, NULL);

    return ESP_OK;
}

static bool set_config_cb(const usb_device_desc_t *dev_desc, uint8_t *bConfigurationValue)
{

    // If the USB device has more than one configuration, set the second configuration
    if (dev_desc->bNumConfigurations > 1)
    {
        ESP_LOGD(TAG, "USB has %d configurations, setting configuration 2", dev_desc->bNumConfigurations);
        *bConfigurationValue = 2;
    }
    else
    {
        ESP_LOGD(TAG, "USB has only one configuration, using default");
        *bConfigurationValue = 1;
    }
    return true;
}
/// This task installs the USB Host driver and continuously polls for device connection.
static void cdc_ecm_task(void *arg)
{
    cdc_ecm_params_t *params = (cdc_ecm_params_t *)arg;

    device_disconnected_sem = xSemaphoreCreateBinary();
    if (device_disconnected_sem == NULL) {
        ESP_LOGE(TAG, "Failed to allocate device_disconnected_sem - Out of Memory!");
        vTaskDelete(NULL); // Stop this task safely
        return;
    }

    // Install USB Host driver (should only be done once)
    ESP_LOGI(TAG, "Installing USB Host");
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
        .enum_filter_cb = set_config_cb,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    // Create a task that will handle USB library events
    BaseType_t task_created = xTaskCreate(usb_lib_task, "usb_lib", 4096, xTaskGetCurrentTaskHandle(), CDC_ECM_USB_HOST_PRIORITY, NULL);
    assert(task_created == pdTRUE);

    while (true)
    {
        ESP_ERROR_CHECK(cdc_ecm_host_install(NULL));

        const cdc_ecm_host_device_config_t dev_config = {
            .connection_timeout_ms = 1000,
            .out_buffer_size = 1536,
            .in_buffer_size = 1536,
            .user_arg = NULL,
            .event_cb = cdc_ecm_host_device_event_handler,
            .data_cb = handle_rx,
        };

        cdc_dev = NULL;
        ESP_LOGI(TAG, "Waiting for USB device connection...");

        esp_err_t err = ESP_FAIL;

        size_t num_pids = sizeof(params->pids) / sizeof(params->pids[0]);
        while (err != ESP_OK)
        {
            ESP_LOGD(TAG, "Trying to open USB device...");
            // Try both PID options
            for (size_t i = 0; i < num_pids; i++)
            {
                err = cdc_ecm_host_open(params->vid, params->pids[i], 0, &dev_config, &cdc_dev);
                if (err == ESP_OK)
                {
                    break;
                }
            }
            if (err != ESP_OK)
            {
                // Delay before retrying to avoid busy looping
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }

        ESP_LOGD(TAG, "USB device connected, waiting for Ethernet connection");

        if (params->event_cb)
        {
            esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, params->event_cb, params->callback_arg);
            esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, params->event_cb, params->callback_arg);
        }

        // Post an event to start the Ethernet connection.
        esp_event_post(ETH_EVENT, ETHERNET_EVENT_START, &usb_netif, sizeof(esp_netif_t *), portMAX_DELAY);

        // Initialize the network interface (event handler registrations are now in app_main)
        cdc_ecm_netif_init(cdc_dev, params);

        // send network connected state in case the device is already connected before registers were able to start.

        if (network_connected)
        {
            esp_event_post(ETH_EVENT, ETHERNET_EVENT_CONNECTED, &usb_netif, sizeof(esp_netif_t *), portMAX_DELAY);
        }
        // else
        // {
        //     esp_event_post(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &usb_netif, sizeof(esp_netif_t *), portMAX_DELAY);
        // }

        // Wait for the device to be disconnected before restarting the loop
        xSemaphoreTake(device_disconnected_sem, portMAX_DELAY);

        // ESP_LOGI(TAG, "Device disconnected");
        esp_event_post(ETH_EVENT, ETHERNET_EVENT_STOP, &usb_netif, sizeof(esp_netif_t *), portMAX_DELAY);

        esp_netif_action_stop(usb_netif, 0, 0, 0);
        esp_netif_destroy(usb_netif);
        usb_netif = NULL;
        cdc_ecm_host_close(cdc_dev);
        cdc_ecm_host_uninstall();
        cdc_dev = NULL;
        if (params->event_cb)
        {
            esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, params->event_cb);
            esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, params->event_cb);
            // esp_event_handler_unregister(IP_EVENT, IP_EVENT_TX_RX, params->event_cb);
        }
        vTaskDelay(100);
    }
}

/// This function initializes the CDC-ECM subsystem by creating the task.
/// It uses the statically allocated parameters.
void cdc_ecm_init(cdc_ecm_params_t *cdc_ecm_params)
{
    assert(cdc_ecm_params != NULL);
    // Create the task that handles the host installation and connection loop.
    BaseType_t task_created = xTaskCreate(cdc_ecm_task, "cdc_ecm_task", 1024 * 8, cdc_ecm_params, CDC_ECM_USB_HOST_PRIORITY, NULL);
    assert(task_created == pdTRUE);
}
