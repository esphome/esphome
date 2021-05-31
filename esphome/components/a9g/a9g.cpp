#include "a9g.h"
#include "esphome/core/log.h"

namespace esphome {
namespace a9g {

static const char *TAG = "a9g";

void A9G::setup() {

    pinMode(this->a9g_power_on_pin_, OUTPUT);
    pinMode(this->a9g_power_off_pin_, OUTPUT);
    pinMode(this->a9g_low_power_pin_, OUTPUT);

    digitalWrite(this->a9g_power_on_pin_, HIGH);
    digitalWrite(this->a9g_power_off_pin_, LOW);
    digitalWrite(this->a9g_low_power_pin_, HIGH);

    ESP_LOGD(TAG, "Powering on A9G...");
    delay(2000);

    digitalWrite(this->a9g_power_on_pin_, LOW);
    delay(3000);
    digitalWrite(this->a9g_power_on_pin_, HIGH);
    delay(5000);

    this->send_command("AT+GPS=1");

    this->a9g_initialized_ = true;
}

bool A9G::send_command(const char *command) {
    this->send_command(command, NULL, 0);
    return true;
}

bool A9G::send_command(const char *command, uint8_t *response, size_t length) {
    uint8_t newline[2] = {0x0D, 0x0A};
    long int time = millis();
    size_t bytesRead = 0;

    while (this->available()) this->read();

    this->write_str(command);
    this->write_array(newline, sizeof(newline));
    this->flush();

    if (response == nullptr)
    {
        return true;
    }

    while ((time + 200) > millis())
    {
        while (this->available() && (bytesRead < length)) {
            uint8_t c = this->read();
            response[bytesRead] = c;
            bytesRead++;
        }
    }

    return true;
}

A9GCoordinate* A9G::parse_location_response(uint8_t *response) {
    A9GCoordinate *coordinates;
    char lat[12], lon[12];

    coordinates = (A9GCoordinate *)malloc(sizeof(A9GCoordinate));
    coordinates->latitude = -100;
    coordinates->longitude = -100;

    if (strlen((const char *)response) < 17)
    {
        return coordinates;
    }

    if (strncmp("AT+LOCATION=2\r\n\r\n", (const char *)response, 17) == 0)
    {
        if (strlen((const char *)response) >= 38)
        {
            for(int idx = 17; idx < strlen((const char *) response); idx++) {
                char c = response[idx];

                if (c == ',') {
                    size_t lat_size = idx - 17;

                    if (lat_size + 1 <= sizeof(lat)) {
                        int idx2;
                        size_t lon_size;
                        

                        ESP_LOGD(TAG, "Latitude Str Size: %d", lat_size);
                        strncpy(&lat[0], (const char *)&response[17], lat_size);

                        for(idx2 = idx; 
                            (idx2 < strlen((const char *)response)) && (response[idx2] != '\r'); 
                            idx2++);

                        lon_size = idx2 - idx -1;

                        ESP_LOGD(TAG, "Longitude Str Size: %d", lon_size);

                        strncpy(&lon[0], (const char *)&response[idx + 1], lon_size);

                        coordinates->latitude = strtof(&lat[0], NULL);
                        coordinates->longitude = strtof(&lon[0], NULL);

                        return coordinates;
                    }
                    else
                    {
                        ESP_LOGD(TAG, "Could not parse latitude (invalid string size)");
                    }
                }
            }
        }
        else
        {
            ESP_LOGD(TAG, "Invalid length, cannot parse latitude and logitude");
        }
    }
    return coordinates;
}

void A9G::update() {
    uint8_t readBuffer[100];
    A9GCoordinate *coordinates;
    
    if (!this->a9g_initialized_)
    {
        return;
    }

    this->send_command("AT+LOCATION=2", &readBuffer[0], sizeof(readBuffer));

    coordinates = this->parse_location_response(&readBuffer[0]);
    
    if ((coordinates->latitude != -100) && (coordinates->longitude != -100)) {
        this->latitude_ = coordinates->latitude;
        this->longitude_ = coordinates->longitude;

        if (this->latitude_sensor_ != nullptr)
            this->latitude_sensor_->publish_state(this->latitude_);

        if (this->longitude_sensor_ != nullptr)
            this->longitude_sensor_->publish_state(this->longitude_);
    }

    free(coordinates);

}

void A9G::loop() {

}

} // namespace a9g
} // namespace esphome
