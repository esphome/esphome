/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "usb/usb_types_ch9.h"

typedef const usb_standard_desc_t *cdc_func_array_t[]; // Array of pointers to const usb_standard_desc_t

typedef struct
{
    const usb_ep_desc_t *notif_ep;
    const usb_ep_desc_t *in_ep;
    const usb_ep_desc_t *out_ep;
    const usb_intf_desc_t *notif_intf;
    const usb_intf_desc_t *data_intf;
    cdc_func_array_t *func;
    int func_cnt;
    uint16_t maxSegmentSize;
} cdc_parsed_info_t;

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Parse CDC interface descriptor
     *
     * #. Check if the required interface exists
     * #. Parse the interface descriptor
     * #. Check if the device is CDC compliant
     * #. For CDC compliant devices also parse second interface descriptor and functional descriptors
     *
     * @param[in] device_desc Pointer to Device descriptor
     * @param[in] config_desc Pointer do Configuration descriptor
     * @param[in] intf_idx    Index of the required interface
     * @param[out] info_ret   Array of parsed information, see cdc_parsed_info_t
     * @return
     *     - ESP_OK:            Success
     *     - ESP_ERR_NOT_FOUND: Interfaces and endpoints NOT found
     */
    esp_err_t cdc_parse_interface_descriptor(const usb_device_desc_t *device_desc, const usb_config_desc_t *config_desc, uint8_t intf_idx, cdc_parsed_info_t *info_ret);

    /**
     * @brief Print CDC specific descriptor in human readable form
     *
     * This is a callback function that is called from USB Host library,
     * when it wants to print full configuration descriptor to stdout.
     *
     * @param[in] _desc CDC specific descriptor
     */
    void cdc_print_desc(const usb_standard_desc_t *_desc);

#ifdef __cplusplus
}
#endif
