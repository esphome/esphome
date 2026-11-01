#pragma once

// C ABI header; BME690 C++ wrapper code lives in namespace esphome::bme690.

/**
 * Copyright (C) Bosch Sensortec GmbH. All Rights Reserved. Confidential.
 *
 * Disclaimer
 *
 * Common:
 * Bosch Sensortec products are developed for the consumer goods industry. They may only be used
 * within the parameters of the respective valid product data sheet. Bosch Sensortec products are
 * provided with the express understanding that there is no warranty of fitness for a particular purpose.
 * They are not fit for use in life-sustaining, safety or security sensitive systems or any system or device
 * that may lead to bodily harm or property damage if the system or device malfunctions. In addition,
 * Bosch Sensortec products are not fit for use in products which interact with motor vehicle systems.
 * The resale and/or use of products are at the purchaser's own risk and his own responsibility. The
 * examination of fitness for the intended use is the sole responsibility of the Purchaser.
 *
 * The purchaser shall indemnify Bosch Sensortec from all third party claims, including any claims for
 * incidental, or consequential damages, arising from any product use not covered by the parameters of
 * the respective valid product data sheet or not approved by Bosch Sensortec and reimburse Bosch
 * Sensortec for all costs in connection with such claims.
 *
 * The purchaser must monitor the market for the purchased products, particularly with regard to
 * product safety and inform Bosch Sensortec without delay of all security relevant incidents.
 *
 * Engineering Samples are marked with an asterisk (*) or (e). Samples may vary from the valid
 * technical specifications of the product series. They are therefore not intended or fit for resale to third
 * parties or for use in end products. Their sole purpose is internal client testing. The testing of an
 * engineering sample may in no way replace the testing of a product series. Bosch Sensortec
 * assumes no liability for the use of engineering samples. By accepting the engineering samples, the
 * Purchaser agrees to indemnify Bosch Sensortec from all claims arising from the use of engineering
 * samples.
 *
 * Special:
 * This software module (hereinafter called "Software") and any information on application-sheets
 * (hereinafter called "Information") is provided free of charge for the sole purpose to support your
 * application work. The Software and Information is subject to the following terms and conditions:
 *
 * The Software is specifically designed for the exclusive use for Bosch Sensortec products by
 * personnel who have special experience and training. Do not use this Software if you do not have the
 * proper experience or training.
 *
 * This Software package is provided `` as is `` and without any expressed or implied warranties,
 * including without limitation, the implied warranties of merchantability and fitness for a particular
 * purpose.
 *
 * Bosch Sensortec and their representatives and agents deny any liability for the functional impairment
 * of this Software in terms of fitness, performance and safety. Bosch Sensortec and their
 * representatives and agents shall not be liable for any direct or indirect damages or injury, except as
 * otherwise stipulated in mandatory applicable law.
 *
 * The Information provided is believed to be accurate and reliable. Bosch Sensortec assumes no
 * responsibility for the consequences of use of such Information nor for any infringement of patents or
 * other rights of third parties which may result from its use. No license is granted by implication or
 * otherwise under any patent or patent rights of Bosch. Specifications mentioned in the Information are
 * subject to change without notice.
 *
 * It is not allowed to deliver the source code of the Software to any third party without permission of
 * Bosch Sensortec.
 *
 * @file     bsec_interface.h
 *
 * @brief
 * Contains the multi-instance API for BSEC
 *
 */

#ifndef BSEC_INTERFACE_H_
#define BSEC_INTERFACE_H_

#include "bsec_datatypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! @addtogroup bsec_lib_interface BSEC Interfaces
 *  @brief The interface functions are used for interfacing one or more sensors with corresponding BSEC library
 * instances.
 *
 * # Interface usage
 *
 * The following provides a short overview on the typical operation sequence for BSEC.
 *
 * - Initialization of the library
 *
 * | Steps                                                               | Function                 |
 * |---------------------------------------------------------------------|--------------------------|
 * | Initialization of library                                           | bsec_init()              |
 * | Update configuration settings (optional)                            | bsec_set_configuration() |
 * | Restore the state of the library (optional)                         | bsec_set_state()         |
 *
 *
 * - The following function is called to enable output signals and define their sampling rate / operation mode.
 *
 * | Steps                                       |  Function                  |
 * |---------------------------------------------|----------------------------|
 * | Enable library outputs with specified mode  | bsec_update_subscription() |
 *
 *
 * - This table describes the main processing loop.
 *
 * | Steps                                     | Function                         |
 * |-------------------------------------------|----------------------------------|
 * | Retrieve sensor settings to be used       | bsec_sensor_control()            |
 * | Configure sensor and trigger measurement  | See BME690 API and example codes |
 * | Read results from sensor                  | See BME690 API and example codes |
 * | Perform signal processing                 | bsec_do_steps()                  |
 *
 *
 * - Before shutting down the system, the current state of BSEC can be retrieved and can then be used during
 *   re-initialization to continue processing.
 *
 * | Steps                                       | Function          |
 * |---------------------------------------------|-------------------|
 * | Retrieve the current library state          |  bsec_get_state() |
 * | Retrieve the current library configuration  |  bsec_get_configuration() |
 *
 *
 * ### Configuration and state
 *
 * Values of variables belonging to a BSEC instance are divided into two groups:
 *  - Values **not updated by processing** of signals belong to the **configuration group**. If available, BSEC can be
 *    configured before use with a customer specific configuration string.
 *  - Values **updated during processing** are member of the **state group**. Saving and restoring of the state of BSEC
 *    is necessary to maintain previously estimated sensor models and baseline information which is important for best
 *    performance of the gas sensor outputs.
 *
 * @note BSEC library consists of adaptive algorithms which models the gas sensor which improves its performance over
 *       the time. These will be lost if library is initialized due to system reset. In order to avoid this situation
 *       library state shall be stored in non volatile memory so that it can be loaded after system reset.
 *
 *
 *   @{
 */

/********************************************************/
/* function prototype declarations */

/*!
 * @brief Function that provides the size of the internal instance in bytes.
 * To be used for allocating memory for struct BSEC_STRUCT_NAME
 * @return Size of the internal instance in bytes
 */
size_t bsec_get_instance_size(void);

/*!
 * @brief Return the version information of BSEC library instance
 * @param[in,out]   inst    Reference to the pointer containing the instance
 * @param [out]     bsec_version_p      pointer to struct which is to be populated with the version information
 *
 * @return Zero if successful, otherwise an error code
 *
 * See also: bsec_version_t
 *
 */
bsec_library_return_t bsec_get_version(void *inst, bsec_version_t *bsec_version_p);

/*!
 * @brief Initialize the library instance
 *
 * Initialization and reset of BSEC library instance is performed by calling bsec_init(). Calling this function sets up
 * the relation among all internal modules, initializes run-time dependent library states and resets the configuration
 * and state of all BSEC signal processing modules to defaults.
 *
 * Before any further use, the library must be initialized. This ensure that all memory and states are in defined
 * conditions prior to processing any data.
 *
 * @param[in,out]   inst    Reference to the pointer containing the instance
 *
 * @return Zero if successful, otherwise an error code
 *
 */
bsec_library_return_t bsec_init(void *inst);

/*!
 * @brief Subscribe to library virtual sensors outputs
 *
 * Use bsec_update_subscription() to instruct BSEC which of the processed output signals
 * of the library instance are requested at which sample rates.
 *
 * See ::bsec_virtual_sensor_t for available library outputs.
 *
 * Based on the requested virtual sensors outputs, BSEC will provide information about the required physical sensor
 * input signals (see ::bsec_physical_sensor_t) with corresponding sample rates. This information is purely
 * informational as bsec_sensor_control() will ensure the sensor is operated in the required manner. To disable a
 * virtual sensor, set the sample rate to ::BSEC_SAMPLE_RATE_DISABLED.
 *
 * The subscription update using bsec_update_subscription() is apart from the signal processing one of the the most
 * important functions. It allows to enable the desired library outputs. The function determines which physical input
 * sensor signals are required at which sample rate to produce the virtual output sensor signals requested by the user.
 * When this function returns with success, the requested outputs are called subscribed. A very important feature is the
 * retaining of already subscribed outputs. Further outputs can be requested or disabled both individually and
 * group-wise in addition to already subscribed outputs without changing them unless a change of already subscribed
 * outputs is requested.
 *
 * @note The state of the library concerning the subscribed outputs cannot be retained among reboots.
 *
 * The interface of bsec_update_subscription() requires the usage of arrays of sensor configuration structures.
 * Such a structure has the fields sensor identifier and sample rate. These fields have the properties:
 *  - Output signals of virtual sensors must be requested using unique identifiers (Member of ::bsec_virtual_sensor_t)
 *  - Different sets of identifiers are available for inputs of physical sensors and outputs of virtual sensors
 *  - Identifiers are unique values defined by the library, not from external
 *  - Sample rates must be provided as value of
 *     - An allowed sample rate for continuously sampled signals
 *     - 65535.0f (BSEC_SAMPLE_RATE_DISABLED) to turn off outputs and identify disabled inputs
 *
 * @note The same sensor identifiers are also used within the functions bsec_do_steps().
 *
 * The usage principles of bsec_update_subscription() are:
 *  - Differential updates (i.e., only asking for outputs that the user would like to change) is supported.
 *  - Invalid requests of outputs are ignored. Also if one of the requested outputs is unavailable, all the requests
 *    are ignored. At the same time, a warning is returned.
 *  - To disable BSEC, all outputs shall be turned off. Only enabled (subscribed) outputs have to be disabled while
 *    already disabled outputs do not have to be disabled explicitly.
 *
 *
 * @param[in,out]   inst                            Reference to the pointer containing the instance
 * @param[in]       requested_virtual_sensors       Pointer to array of requested virtual sensor (output) configurations
 * for the library
 * @param[in]       n_requested_virtual_sensors     Number of virtual sensor structs pointed by
 * requested_virtual_sensors
 * @param[out]      required_sensor_settings        Pointer to array of required physical sensor configurations for the
 * library
 * @param[in,out]   n_required_sensor_settings      [in] Size of allocated required_sensor_settings array, [out] number
 * of sensor configurations returned
 *
 * @return Zero when successful, otherwise an error code
 *
 * @sa bsec_sensor_configuration_t
 * @sa bsec_physical_sensor_t
 * @sa bsec_virtual_sensor_t
 *
 */
bsec_library_return_t bsec_update_subscription(void *inst, const bsec_sensor_configuration_t *requested_virtual_sensors,
                                               uint8_t n_requested_virtual_sensors,
                                               bsec_sensor_configuration_t *required_sensor_settings,
                                               uint8_t *n_required_sensor_settings);

/*!
 * @brief Main signal processing function of BSEC library instance
 *
 *
 * Processing of the input signals and returning of output samples for each instances of BSEC library is performed by
 * bsec_do_steps().
 * - The samples of all library inputs must be passed with unique identifiers representing the input signals from
 *   physical sensors where the order of these inputs can be chosen arbitrary. However, all input have to be provided
 *   within the same time period as they are read. A sequential provision to the library might result in undefined
 *   behavior.
 * - The samples of all library outputs are returned with unique identifiers corresponding to the output signals of
 *   virtual sensors where the order of the returned outputs may be arbitrary.
 * - The samples of all input as well as output signals of physical as well as virtual sensors use the same
 *   representation in memory with the following fields:
 * - Sensor identifier:
 *   - For inputs: required to identify the input signal from a physical sensor
 *   - For output: overwritten by bsec_do_steps() to identify the returned signal from a virtual sensor
 *   - Time stamp of the sample
 *
 * Calling bsec_do_steps() requires the samples of the input signals to be provided along with their time stamp when
 * they are recorded and only when they are acquired. Repetition of samples with the same time stamp are ignored and
 * result in a warning. Repetition of values of samples which are not acquired anew by a sensor result in deviations
 * of the computed output signals. Concerning the returned output samples, an important feature is, that a value is
 * returned for an output only when a new occurrence has been computed. A sample of an output signal is returned only
 * once.
 *
 *
 *
 * @param[in,out]   inst            Reference to the pointer containing the instance
 * @param[in]       inputs          Array of input data samples. Each array element represents a sample of a different
 * physical sensor.
 * @param[in]       n_inputs        Number of passed input data structs.
 * @param[out]      outputs         Array of output data samples. Each array element represents a sample of a different
 * virtual sensor.
 * @param[in,out]   n_outputs       [in] Number of allocated output structs, [out] number of outputs returned
 *
 * @return Zero when successful, otherwise an error code
 *
 */
bsec_library_return_t bsec_do_steps(void *inst, const bsec_input_t *inputs, uint8_t n_inputs, bsec_output_t *outputs,
                                    uint8_t *n_outputs);

/*!
 * @brief Reset a particular virtual sensor output of the library instance
 *
 * This function allows specific virtual sensor outputs of each library instance to be reset to be reset. The meaning of
 * "reset" depends on the specific output. In case of the IAQ output, reset means zeroing the output to the current
 * ambient conditions.
 *
 * @param[in,out]   inst                Reference to the pointer containing the instance
 * @param[in]       sensor_id           Virtual sensor to be reset
 *
 * @return Zero when successful, otherwise an error code
 *
 */
bsec_library_return_t bsec_reset_output(void *inst, uint8_t sensor_id);

/*!
 * @brief Update algorithm configuration parameters of the library instance
 *
 * BSEC uses a default configuration for the modules and common settings. The initial configuration can be customized
 * by bsec_set_configuration(). This is an optional step.
 *
 * @note A work buffer with sufficient size is required and has to be provided by the function caller to decompose
 * the serialization and apply it to the library and its modules.
 *
 * Please use #BSEC_MAX_PROPERTY_BLOB_SIZE for allotting the required size.
 *
 * @param[in,out]   inst                    Reference to the pointer containing the instance
 * @param[in]       serialized_settings     Settings serialized to a binary blob
 * @param[in]       n_serialized_settings   Size of the settings blob
 * @param[in,out]   work_buffer             Work buffer used to parse the blob
 * @param[in]       n_work_buffer_size      Length of the work buffer available for parsing the blob
 *
 * @return Zero when successful, otherwise an error code
 *
 */
bsec_library_return_t bsec_set_configuration(void *inst, const uint8_t *serialized_settings,
                                             uint32_t n_serialized_settings, uint8_t *work_buffer,
                                             uint32_t n_work_buffer_size);
/*!
 * @brief Restore the internal state of the library instance
 *
 * BSEC uses a default state for all signal processing modules and the BSEC module for each instance. To ensure optimal
 * performance, especially of the gas sensor functionality, it is recommended to retrieve the state using
 * bsec_get_state() before unloading the library, storing it in some form of non-volatile memory, and setting it using
 * bsec_set_state() before resuming further operation of the library.
 *
 * @note A work buffer with sufficient size is required and has to be provided by the function caller to decompose the
 * serialization and apply it to the library and its modules.
 *
 * Please use #BSEC_MAX_STATE_BLOB_SIZE for allotting the required size.
 *
 * @param[in,out]   inst                    Reference to the pointer containing the instance
 * @param[in]       serialized_state        States serialized to a binary blob
 * @param[in]       n_serialized_state      Size of the state blob
 * @param[in,out]   work_buffer             Work buffer used to parse the blob
 * @param[in]       n_work_buffer_size      Length of the work buffer available for parsing the blob
 *
 * @return Zero when successful, otherwise an error code
 *
 */
bsec_library_return_t bsec_set_state(void *inst, const uint8_t *serialized_state, uint32_t n_serialized_state,
                                     uint8_t *work_buffer, uint32_t n_work_buffer_size);

/*!
 * @brief Retrieve the current library instance configuration
 *
 * BSEC allows to retrieve the current configuration of the library instance using bsec_get_configuration().
 * This API returns a binary blob encoding.
 * the current configuration parameters of the library in a format compatible with bsec_set_configuration().
 *
 * Please use #BSEC_MAX_PROPERTY_BLOB_SIZE for allotting the required size.
 *
 * @param[in,out]   inst                        Reference to the pointer containing the instance
 * @param[in]       config_id                   Identifier for a specific set of configuration settings to be returned;
 *                                              shall be zero to retrieve all configuration settings.
 * @param[out]      serialized_settings         Buffer to hold the serialized config blob
 * @param[in]       n_serialized_settings_max   Maximum available size for the serialized settings
 * @param[in,out]   work_buffer                 Work buffer used to parse the binary blob
 * @param[in]       n_work_buffer               Length of the work buffer available for parsing the blob
 * @param[out]      n_serialized_settings       Actual size of the returned serialized configuration blob
 *
 * @return Zero when successful, otherwise an error code
 *
 */
bsec_library_return_t bsec_get_configuration(void *inst, uint8_t config_id, uint8_t *serialized_settings,
                                             uint32_t n_serialized_settings_max, uint8_t *work_buffer,
                                             uint32_t n_work_buffer, uint32_t *n_serialized_settings);

/*!
 *@brief Retrieve the current internal library instance state
 *
 * BSEC allows to retrieve the current states of all signal processing modules and the BSEC module of the library
 *instance using bsec_get_state(). This allows a restart of the processing after a reboot of the system by calling
 *bsec_set_state().
 *
 * Please use #BSEC_MAX_STATE_BLOB_SIZE for allotting the required size.
 *
 * @param[in,out]   inst                        Reference to the pointer containing the instance
 * @param[in]       state_set_id                Identifier for a specific set of states to be returned; shall be
 *                                              zero to retrieve all states.
 * @param[out]      serialized_state            Buffer to hold the serialized config blob
 * @param[in]       n_serialized_state_max      Maximum available size for the serialized states
 * @param[in,out]   work_buffer                 Work buffer used to parse the blob
 * @param[in]       n_work_buffer               Length of the work buffer available for parsing the blob
 * @param[out]      n_serialized_state          Actual size of the returned serialized blob
 *
 * @return Zero when successful, otherwise an error code
 *
 */
bsec_library_return_t bsec_get_state(void *inst, uint8_t state_set_id, uint8_t *serialized_state,
                                     uint32_t n_serialized_state_max, uint8_t *work_buffer, uint32_t n_work_buffer,
                                     uint32_t *n_serialized_state);

/*!
 * @brief Retrieve BMExxx sensor instructions for the library instance
 *
 * The bsec_sensor_control() interface is a key feature of BSEC, as it allows an easy way for the signal processing
 * library to control the operation of the BME sensor using the correspodning BSEC library instance. This is important
 * since gas sensor behaviour is mainly determined by how the integrated heater is configured. To ensure an easy
 * integration of BSEC into any system, bsec_sensor_control() will provide the caller with information about the current
 * sensor configuration that is necessary to fulfill the input requirements derived from the current outputs requested
 * via bsec_update_subscription().
 *
 * In practice the use of this function shall be as follows:
 * - Call bsec_sensor_control() which returns a bsec_bme_settings_t struct.
 * - Based on the information contained in this struct, the sensor is configured and a forced-mode measurement is
 *   triggered if requested by bsec_sensor_control().
 * - Once this forced-mode measurement is complete, the signals specified in this struct shall be passed to
 *   bsec_do_steps() to perform the signal processing.
 * - After processing, the process should sleep until the bsec_bme_settings_t::next_call timestamp is reached.
 *
 *
 * @param[in,out]   inst                      Reference to the pointer containing the instance
 * @param [in]      time_stamp                Current timestamp in [ns]
 * @param[out]      sensor_settings           Settings to be passed to API to operate sensor at this time instance
 *
 * @return Zero when successful, otherwise an error code
 */
bsec_library_return_t bsec_sensor_control(void *inst, int64_t time_stamp, bsec_bme_settings_t *sensor_settings);

/*@}*/  // BSEC Interface

#ifdef __cplusplus
}
#endif

#endif /* BSEC_INTERFACE_H_ */
