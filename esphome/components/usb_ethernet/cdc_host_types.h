/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <sys/queue.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "usb/usb_host.h"  // For USB device handle and transfers
#include "cdc_ecm_host.h"  // For callback types
#include "usb_types_cdc.h" // For protocol and serial state

typedef struct cdc_dev_s cdc_dev_t;
struct cdc_dev_s
{
    usb_device_handle_t dev_hdl; // USB device handle
    void *cb_arg;                // Common argument for user's callbacks (data IN and Notification)
    struct
    {
        usb_transfer_t *out_xfer;         // OUT data transfer
        usb_transfer_t *in_xfer;          // IN data transfer
        cdc_ecm_data_callback_t in_cb;    // User's callback for async (non-blocking) data IN
        uint16_t in_mps;                  // IN endpoint Maximum Packet Size
        uint8_t *in_data_buffer_base;     // Pointer to IN data buffer in usb_transfer_t
        const usb_intf_desc_t *intf_desc; // Pointer to data interface descriptor
        SemaphoreHandle_t out_mux;        // OUT mutex
    } data;

    struct
    {
        usb_transfer_t *xfer;             // IN notification transfer
        const usb_intf_desc_t *intf_desc; // Pointer to notification interface descriptor, can be NULL if there is no notification channel in the device
        cdc_ecm_host_dev_callback_t cb;   // User's callback for device events
    } notif;                              // Structure with Notif pipe data

    usb_transfer_t *ctrl_transfer;     // CTRL (endpoint 0) transfer
    SemaphoreHandle_t ctrl_mux;        // CTRL mutex
    bool connect_status;               // Connection status
    cdc_acm_uart_state_t serial_state; // Serial State
    cdc_comm_protocol_t comm_protocol;
    cdc_data_protocol_t data_protocol;
    uint16_t max_segment_size;                     // Maximum Segment Size
    uint8_t mac[6];                                // MAC address
    int cdc_func_desc_cnt;                         // Number of CDC Functional descriptors in following array
    const usb_standard_desc_t *(*cdc_func_desc)[]; // Pointer to array of pointers to const usb_standard_desc_t
    SLIST_ENTRY(cdc_dev_s)
    list_entry;
};
