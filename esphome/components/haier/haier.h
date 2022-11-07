#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace haier {

static const char *const TAG = "haier";


static const uint8_t TEMPERATURE         13
static const uint8_t HUMIDITY            15

static const uint8_t COMMAND             17


static const uint8_t MODE                23
static const uint8_t MODE_SMART          0
static const uint8_t MODE_COOL           1
static const uint8_t MODE_HEAT           2
static const uint8_t MODE_ONLY_FAN       3
static const uint8_t MODE_DRY            4

static const uint8_t FAN_SPEED           25
static const uint8_t FAN_MIN             2
static const uint8_t FAN_MIDDLE          1
static const uint8_t FAN_MAX             0
static const uint8_t FAN_AUTO            3

static const uint8_t SWING               27
static const uint8_t SWING_MODE          31
static const uint8_t SWING_OFF           00
static const uint8_t SWING_VERTICAL      01
static const uint8_t SWING_HORIZONTAL    02
static const uint8_t SWING_BOTH          01


static const uint8_t SWING_HORIZONTAL_MASK   (1 << 3)
static const uint8_t SWING_VERTICAL_MASK     (1 << 4)


static const uint8_t LOCK                28
static const uint8_t LOCK_ON             80
static const uint8_t LOCK_OFF            0


static const uint8_t POWER               29
static const uint8_t POWER_MASK          1


static const uint8_t FRESH               31
static const uint8_t FRESH_ON            1
static const uint8_t FRESH_OFF           0


static const uint8_t SET_TEMPERATURE     35
static const uint8_t DECIMAL_MASK        (1 << 5)


static const uint8_t CRC                 36


static const uint8_t CONFORT_PRESET_MASK     (1 << 3)


static const uint8_t MIN_SET_TEMPERATURE 16
static const uint8_t MAX_SET_TEMPERATURE 30
static const uint8_t STEP_TEMPERATURE 0.5f


static const uint8_t MIN_VALID_TEMPERATURE 10
static const uint8_t MAX_VALID_TEMPERATURE 50


class Haier : public climate::Climate, public uart::UARTDevice, public PollingComponent {

private:

    uint8_t lastCRC;
    uint8_t data[37];
    uint8_t poll[13] = {255, 255, 10, 0, 0, 0, 0, 0, 1, 1, 77, 1, 90};
    uint8_t on[13] = {255, 255, 10, 0, 0, 0, 0, 0, 1, 1, 77, 2, 91};
    uint8_t off[13] = {255, 255, 10, 0, 0, 0, 0, 0, 1, 1, 77, 3, 92};

    bool swing;

public:


    Haier() : PollingComponent(5 * 1000) {
        lastCRC = 0;
        this->swing = true;
    }


    void setup() override {

    }

    void loop() override {
        if (this->available()) {

            if (this->read() != 255) return;
            if (this->read() != 255) return;

            data[0] = 255;
            data[1] = 255;

            this->read_array(data + 2, sizeof(data) - 2);

            readData();
        }
    }

    void update() override {
        this->write_array(poll, sizeof(poll));
        auto raw = getHex(poll, sizeof(poll));
        ESP_LOGD("Haier", "POLL: %s ", raw.c_str());
    }

    void dump_config() {
      ESP_LOGCONFIG(TAG, "Haier:");
      ESP_LOGCONFIG(TAG, "  Update interval: %u", this->get_update_interval());
      this->check_uart_settings(9600);
    }

protected:
    climate::ClimateTraits traits() override {
        auto traits = climate::ClimateTraits();


        traits.set_supported_modes({
                                           climate::CLIMATE_MODE_OFF,
                                           climate::CLIMATE_MODE_HEAT_COOL,
                                           climate::CLIMATE_MODE_COOL,
                                           climate::CLIMATE_MODE_HEAT,
                                           climate::CLIMATE_MODE_FAN_ONLY,
                                           climate::CLIMATE_MODE_DRY
                                   });

        traits.set_supported_fan_modes({
                                               climate::CLIMATE_FAN_AUTO,
                                               climate::CLIMATE_FAN_LOW,
                                               climate::CLIMATE_FAN_MEDIUM,
                                               climate::CLIMATE_FAN_HIGH,
                                       });


        if (swing) {
            traits.set_supported_swing_modes({
                                                     climate::CLIMATE_SWING_OFF,
                                                     climate::CLIMATE_SWING_BOTH,
                                                     climate::CLIMATE_SWING_VERTICAL,
                                                     climate::CLIMATE_SWING_HORIZONTAL
                                             });
        }

        traits.set_visual_min_temperature(MIN_SET_TEMPERATURE);
        traits.set_visual_max_temperature(MAX_SET_TEMPERATURE);
        traits.set_visual_temperature_step(STEP_TEMPERATURE);
        traits.set_supports_current_temperature(true);
        traits.set_supports_two_point_target_temperature(false);


        traits.add_supported_preset(climate::CLIMATE_PRESET_NONE);
        traits.add_supported_preset(climate::CLIMATE_PRESET_COMFORT);


        return traits;
    }

public:

    void readData() {


        auto raw = getHex(data, sizeof(data));
        ESP_LOGD("Haier", "Readed message: %s ", raw.c_str());


        uint8_t check = data[CRC];

        getChecksum(data, sizeof(data));

        if (check != data[CRC]) {
            ESP_LOGD("Haier", "Invalid checksum");
            return;
        }


        lastCRC = check;


        if (MIN_VALID_TEMPERATURE < data[TEMPERATURE] && data[TEMPERATURE] < MAX_VALID_TEMPERATURE)
            current_temperature = data[TEMPERATURE];


        target_temperature = data[SET_TEMPERATURE] + 16;


        if (data[POWER] & DECIMAL_MASK)
            target_temperature += 0.5f;


        switch (data[MODE]) {
            case MODE_SMART:
                mode = climate::CLIMATE_MODE_HEAT_COOL;
                break;
            case MODE_COOL:
                mode = climate::CLIMATE_MODE_COOL;
                break;
            case MODE_HEAT:
                mode = climate::CLIMATE_MODE_HEAT;
                break;
            case MODE_ONLY_FAN:
                mode = climate::CLIMATE_MODE_FAN_ONLY;
                break;
            case MODE_DRY:
                mode = climate::CLIMATE_MODE_DRY;
                break;
            default:
                mode = climate::CLIMATE_MODE_HEAT_COOL;
        }


        switch (data[FAN_SPEED]) {
            case FAN_AUTO:
                fan_mode = climate::CLIMATE_FAN_AUTO;
                break;

            case FAN_MIN:
                fan_mode = climate::CLIMATE_FAN_LOW;
                break;

            case FAN_MIDDLE:
                fan_mode = climate::CLIMATE_FAN_MEDIUM;
                break;

            case FAN_MAX:
                fan_mode = climate::CLIMATE_FAN_HIGH;
                break;
        }


        if (data[SWING] == SWING_OFF) {
            if (data[SWING_MODE] & SWING_HORIZONTAL_MASK)
                swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
            else if (data[SWING_MODE] & SWING_VERTICAL_MASK)
                swing_mode = climate::CLIMATE_SWING_VERTICAL;
            else
                swing_mode = climate::CLIMATE_SWING_OFF;
        } else if (data[SWING] == SWING_BOTH)
            swing_mode = climate::CLIMATE_SWING_BOTH;


        if (data[POWER] & CONFORT_PRESET_MASK)
            preset = climate::CLIMATE_PRESET_COMFORT;
        else
            preset = climate::CLIMATE_PRESET_NONE;


        if ((data[POWER] & POWER_MASK) == 0) {
            mode = climate::CLIMATE_MODE_OFF;
        }


        this->publish_state();

    }


    void control(const climate::ClimateCall &call) override {
        if (call.get_mode().has_value()) {
            switch (call.get_mode().value()) {
                case climate::CLIMATE_MODE_OFF:
                    sendData(off, sizeof(off));
                    break;

                case climate::CLIMATE_MODE_HEAT_COOL:
                case climate::CLIMATE_MODE_AUTO:
                    data[POWER] |= POWER_MASK;
                    data[MODE] = MODE_SMART;
                    break;
                case climate::CLIMATE_MODE_HEAT:
                    data[POWER] |= POWER_MASK;
                    data[MODE] = MODE_HEAT;
                    break;
                case climate::CLIMATE_MODE_COOL:
                    data[POWER] |= POWER_MASK;
                    data[MODE] = MODE_COOL;
                    break;

                case climate::CLIMATE_MODE_FAN_ONLY:
                    data[POWER] |= POWER_MASK;
                    data[MODE] = MODE_ONLY_FAN;
                    break;

                case climate::CLIMATE_MODE_DRY:
                    data[POWER] |= POWER_MASK;
                    data[MODE] = MODE_DRY;
                    break;

            }

        }


        if (call.get_preset().has_value()) {


            if (call.get_preset().value() == climate::CLIMATE_PRESET_COMFORT)
                data[POWER] |= CONFORT_PRESET_MASK;
            else
                data[POWER] &= ~CONFORT_PRESET_MASK;


        }


        if (call.get_target_temperature().has_value()) {

            float target = call.get_target_temperature().value() - 16;

            data[SET_TEMPERATURE] = (uint16) target;

            if ((int) target == (int) (target + 0.5))
                data[POWER] &= ~DECIMAL_MASK;
            else
                data[POWER] |= DECIMAL_MASK;

        }

        if (call.get_fan_mode().has_value()) {
            switch (call.get_fan_mode().value()) {
                case climate::CLIMATE_FAN_AUTO:
                    data[FAN_SPEED] = FAN_AUTO;
                    break;
                case climate::CLIMATE_FAN_LOW:
                    data[FAN_SPEED] = FAN_MIN;
                    break;
                case climate::CLIMATE_FAN_MEDIUM:
                    data[FAN_SPEED] = FAN_MIDDLE;
                    break;
                case climate::CLIMATE_FAN_HIGH:
                    data[FAN_SPEED] = FAN_MAX;
                    break;

                case climate::CLIMATE_FAN_ON:
                case climate::CLIMATE_FAN_OFF:
                case climate::CLIMATE_FAN_MIDDLE:
                case climate::CLIMATE_FAN_FOCUS:
                case climate::CLIMATE_FAN_DIFFUSE:
                    break;
            }

        }


        if (call.get_swing_mode().has_value())
            switch (call.get_swing_mode().value()) {
                case climate::CLIMATE_SWING_OFF:
                    data[SWING] = SWING_OFF;
                    data[SWING_MODE] &= ~1;
                    break;
                case climate::CLIMATE_SWING_VERTICAL:
                    data[SWING] = SWING_OFF;
                    data[SWING_MODE] |= SWING_VERTICAL_MASK;
                    data[SWING_MODE] &= ~SWING_HORIZONTAL_MASK;
                    break;
                case climate::CLIMATE_SWING_HORIZONTAL:
                    data[SWING] = SWING_OFF;
                    data[SWING_MODE] |= SWING_HORIZONTAL_MASK;
                    data[SWING_MODE] &= ~SWING_VERTICAL_MASK;
                    break;
                case climate::CLIMATE_SWING_BOTH:
                    data[SWING] = SWING_BOTH;
                    data[SWING_MODE] &= ~SWING_HORIZONTAL_MASK;
                    data[SWING_MODE] &= ~SWING_VERTICAL_MASK;
                    break;
            }





        //Values for "send"
        data[COMMAND] = 0;
        data[9] = 1;
        data[10] = 77;
        data[11] = 95;

        sendData(data, sizeof(data));
    }


    void sendData(uint8_t *message, uint8_t size) {
        uint8_t crc = getChecksum(message, size);
        this->write_array(message, size - 1);
        this->write(crc);

        auto raw = getHex(message, size);
        ESP_LOGD("Haier", "Sended message: %s ", raw.c_str());
    }

    String getHex(uint8_t *message, uint8_t size) {
        String raw;

        for (int i = 0; i < size; i++) {
            raw += "\n" + String(i) + "-" + String(message[i]);

        }
        raw.toUpperCase();

        return raw;
    }

    uint8_t getChecksum(const uint8_t *message, size_t size) {
        uint8_t position = size - 1;
        uint8_t crc = 0;

        for (int i = 2; i < position; i++)
            crc += message[i];

        return crc;

    }
};


}  // namespace haier
}  // namespace esphome
