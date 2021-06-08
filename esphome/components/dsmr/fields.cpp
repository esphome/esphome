/**
 * Arduino DSMR parser.
 *
 * This software is licensed under the MIT License.
 *
 * Copyright (c) 2015 Matthijs Kooijman <matthijs@stdin.nl>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Field parsing functions
 */

#include "fields.h"

using namespace dsmr;
using namespace dsmr::fields;

// Since C++11 it is possible to define the initial values for static
// const members in the class declaration, but if their address is
// taken, they still need a normal definition somewhere (to allocate
// storage).
constexpr char units::none[];
constexpr char units::kWh[];
constexpr char units::Wh[];
constexpr char units::kW[];
constexpr char units::W[];
constexpr char units::V[];
constexpr char units::mV[];
constexpr char units::A[];
constexpr char units::mA[];
constexpr char units::m3[];
constexpr char units::dm3[];
constexpr char units::GJ[];
constexpr char units::MJ[];
constexpr char units::kvar[];
constexpr char units::kvarh[];

constexpr ObisId identification::id;
constexpr char identification::name_progmem[];
constexpr const __FlashStringHelper *identification::name;

constexpr ObisId p1_version::id;
constexpr char p1_version::name_progmem[];
constexpr const __FlashStringHelper *p1_version::name;

/* extra field for Belgium */
constexpr ObisId p1_version_be::id;
constexpr char p1_version_be::name_progmem[];
constexpr const __FlashStringHelper *p1_version_be::name;

constexpr ObisId timestamp::id;
constexpr char timestamp::name_progmem[];
constexpr const __FlashStringHelper *timestamp::name;

constexpr ObisId equipment_id::id;
constexpr char equipment_id::name_progmem[];
constexpr const __FlashStringHelper *equipment_id::name;

/* extra for Lux */
constexpr ObisId energy_delivered_lux::id;
constexpr char energy_delivered_lux::name_progmem[];
constexpr const __FlashStringHelper *energy_delivered_lux::name;

constexpr ObisId energy_delivered_tariff1::id;
constexpr char energy_delivered_tariff1::name_progmem[];
constexpr const __FlashStringHelper *energy_delivered_tariff1::name;

constexpr ObisId energy_delivered_tariff2::id;
constexpr char energy_delivered_tariff2::name_progmem[];
constexpr const __FlashStringHelper *energy_delivered_tariff2::name;

/* extra for Lux */
constexpr ObisId energy_returned_lux::id;
constexpr char energy_returned_lux::name_progmem[];
constexpr const __FlashStringHelper *energy_returned_lux::name;

constexpr ObisId energy_returned_tariff1::id;
constexpr char energy_returned_tariff1::name_progmem[];
constexpr const __FlashStringHelper *energy_returned_tariff1::name;

constexpr ObisId energy_returned_tariff2::id;
constexpr char energy_returned_tariff2::name_progmem[];
constexpr const __FlashStringHelper *energy_returned_tariff2::name;

/* extra for Lux */
constexpr ObisId total_imported_energy::id;
constexpr char total_imported_energy::name_progmem[];
constexpr const __FlashStringHelper *total_imported_energy::name;

/* extra for Lux */
constexpr ObisId total_exported_energy::id;
constexpr char total_exported_energy::name_progmem[];
constexpr const __FlashStringHelper *total_exported_energy::name;

/* extra for Lux */
constexpr ObisId reactive_power_delivered::id;
constexpr char reactive_power_delivered::name_progmem[];
constexpr const __FlashStringHelper *reactive_power_delivered::name;

/* extra for Lux */
constexpr ObisId reactive_power_returned::id;
constexpr char reactive_power_returned::name_progmem[];
constexpr const __FlashStringHelper *reactive_power_returned::name;

constexpr ObisId electricity_tariff::id;
constexpr char electricity_tariff::name_progmem[];
constexpr const __FlashStringHelper *electricity_tariff::name;

constexpr ObisId power_delivered::id;
constexpr char power_delivered::name_progmem[];
constexpr const __FlashStringHelper *power_delivered::name;

constexpr ObisId power_returned::id;
constexpr char power_returned::name_progmem[];
constexpr const __FlashStringHelper *power_returned::name;

constexpr ObisId electricity_threshold::id;
constexpr char electricity_threshold::name_progmem[];
constexpr const __FlashStringHelper *electricity_threshold::name;

constexpr ObisId electricity_switch_position::id;
constexpr char electricity_switch_position::name_progmem[];
constexpr const __FlashStringHelper *electricity_switch_position::name;

constexpr ObisId electricity_failures::id;
constexpr char electricity_failures::name_progmem[];
constexpr const __FlashStringHelper *electricity_failures::name;

constexpr ObisId electricity_long_failures::id;
constexpr char electricity_long_failures::name_progmem[];
constexpr const __FlashStringHelper *electricity_long_failures::name;

constexpr ObisId electricity_failure_log::id;
constexpr char electricity_failure_log::name_progmem[];
constexpr const __FlashStringHelper *electricity_failure_log::name;

constexpr ObisId electricity_sags_l1::id;
constexpr char electricity_sags_l1::name_progmem[];
constexpr const __FlashStringHelper *electricity_sags_l1::name;

constexpr ObisId electricity_sags_l2::id;
constexpr char electricity_sags_l2::name_progmem[];
constexpr const __FlashStringHelper *electricity_sags_l2::name;

constexpr ObisId electricity_sags_l3::id;
constexpr char electricity_sags_l3::name_progmem[];
constexpr const __FlashStringHelper *electricity_sags_l3::name;

constexpr ObisId electricity_swells_l1::id;
constexpr char electricity_swells_l1::name_progmem[];
constexpr const __FlashStringHelper *electricity_swells_l1::name;

constexpr ObisId electricity_swells_l2::id;
constexpr char electricity_swells_l2::name_progmem[];
constexpr const __FlashStringHelper *electricity_swells_l2::name;

constexpr ObisId electricity_swells_l3::id;
constexpr char electricity_swells_l3::name_progmem[];
constexpr const __FlashStringHelper *electricity_swells_l3::name;

constexpr ObisId message_short::id;
constexpr char message_short::name_progmem[];
constexpr const __FlashStringHelper *message_short::name;

constexpr ObisId message_long::id;
constexpr char message_long::name_progmem[];
constexpr const __FlashStringHelper *message_long::name;

constexpr ObisId voltage_l1::id;
constexpr char voltage_l1::name_progmem[];
constexpr const __FlashStringHelper *voltage_l1::name;

constexpr ObisId voltage_l2::id;
constexpr char voltage_l2::name_progmem[];
constexpr const __FlashStringHelper *voltage_l2::name;

constexpr ObisId voltage_l3::id;
constexpr char voltage_l3::name_progmem[];
constexpr const __FlashStringHelper *voltage_l3::name;

constexpr ObisId current_l1::id;
constexpr char current_l1::name_progmem[];
constexpr const __FlashStringHelper *current_l1::name;

constexpr ObisId current_l2::id;
constexpr char current_l2::name_progmem[];
constexpr const __FlashStringHelper *current_l2::name;

constexpr ObisId current_l3::id;
constexpr char current_l3::name_progmem[];
constexpr const __FlashStringHelper *current_l3::name;

constexpr ObisId power_delivered_l1::id;
constexpr char power_delivered_l1::name_progmem[];
constexpr const __FlashStringHelper *power_delivered_l1::name;

constexpr ObisId power_delivered_l2::id;
constexpr char power_delivered_l2::name_progmem[];
constexpr const __FlashStringHelper *power_delivered_l2::name;

constexpr ObisId power_delivered_l3::id;
constexpr char power_delivered_l3::name_progmem[];
constexpr const __FlashStringHelper *power_delivered_l3::name;

constexpr ObisId power_returned_l1::id;
constexpr char power_returned_l1::name_progmem[];
constexpr const __FlashStringHelper *power_returned_l1::name;

constexpr ObisId power_returned_l2::id;
constexpr char power_returned_l2::name_progmem[];
constexpr const __FlashStringHelper *power_returned_l2::name;

constexpr ObisId power_returned_l3::id;
constexpr char power_returned_l3::name_progmem[];
constexpr const __FlashStringHelper *power_returned_l3::name;

/* LUX */
constexpr ObisId reactive_power_delivered_l1::id;
constexpr char reactive_power_delivered_l1::name_progmem[];
constexpr const __FlashStringHelper *reactive_power_delivered_l1::name;

/* LUX */
constexpr ObisId reactive_power_delivered_l2::id;
constexpr char reactive_power_delivered_l2::name_progmem[];
constexpr const __FlashStringHelper *reactive_power_delivered_l2::name;

/* LUX */
constexpr ObisId reactive_power_delivered_l3::id;
constexpr char reactive_power_delivered_l3::name_progmem[];
constexpr const __FlashStringHelper *reactive_power_delivered_l3::name;

/* LUX */
constexpr ObisId reactive_power_returned_l1::id;
constexpr char reactive_power_returned_l1::name_progmem[];
constexpr const __FlashStringHelper *reactive_power_returned_l1::name;

/* LUX */
constexpr ObisId reactive_power_returned_l2::id;
constexpr char reactive_power_returned_l2::name_progmem[];
constexpr const __FlashStringHelper *reactive_power_returned_l2::name;

/* LUX */
constexpr ObisId reactive_power_returned_l3::id;
constexpr char reactive_power_returned_l3::name_progmem[];
constexpr const __FlashStringHelper *reactive_power_returned_l3::name;

constexpr ObisId gas_device_type::id;
constexpr char gas_device_type::name_progmem[];
constexpr const __FlashStringHelper *gas_device_type::name;

constexpr ObisId gas_equipment_id::id;
constexpr char gas_equipment_id::name_progmem[];
constexpr const __FlashStringHelper *gas_equipment_id::name;

constexpr ObisId gas_valve_position::id;
constexpr char gas_valve_position::name_progmem[];
constexpr const __FlashStringHelper *gas_valve_position::name;

/* _NL */
constexpr ObisId gas_delivered::id;
constexpr char gas_delivered::name_progmem[];
constexpr const __FlashStringHelper *gas_delivered::name;

/* _BE */
constexpr ObisId gas_delivered_be::id;
constexpr char gas_delivered_be::name_progmem[];
constexpr const __FlashStringHelper *gas_delivered_be::name;

constexpr ObisId thermal_device_type::id;
constexpr char thermal_device_type::name_progmem[];
constexpr const __FlashStringHelper *thermal_device_type::name;

constexpr ObisId thermal_equipment_id::id;
constexpr char thermal_equipment_id::name_progmem[];
constexpr const __FlashStringHelper *thermal_equipment_id::name;

constexpr ObisId thermal_valve_position::id;
constexpr char thermal_valve_position::name_progmem[];
constexpr const __FlashStringHelper *thermal_valve_position::name;

constexpr ObisId thermal_delivered::id;
constexpr char thermal_delivered::name_progmem[];
constexpr const __FlashStringHelper *thermal_delivered::name;

constexpr ObisId water_device_type::id;
constexpr char water_device_type::name_progmem[];
constexpr const __FlashStringHelper *water_device_type::name;

constexpr ObisId water_equipment_id::id;
constexpr char water_equipment_id::name_progmem[];
constexpr const __FlashStringHelper *water_equipment_id::name;

constexpr ObisId water_valve_position::id;
constexpr char water_valve_position::name_progmem[];
constexpr const __FlashStringHelper *water_valve_position::name;

constexpr ObisId water_delivered::id;
constexpr char water_delivered::name_progmem[];
constexpr const __FlashStringHelper *water_delivered::name;

constexpr ObisId slave_device_type::id;
constexpr char slave_device_type::name_progmem[];
constexpr const __FlashStringHelper *slave_device_type::name;

constexpr ObisId slave_equipment_id::id;
constexpr char slave_equipment_id::name_progmem[];
constexpr const __FlashStringHelper *slave_equipment_id::name;

constexpr ObisId slave_valve_position::id;
constexpr char slave_valve_position::name_progmem[];
constexpr const __FlashStringHelper *slave_valve_position::name;

constexpr ObisId slave_delivered::id;
constexpr char slave_delivered::name_progmem[];
constexpr const __FlashStringHelper *slave_delivered::name;
