from esphomeyaml import config_validation as cv

CONFIG_SCHEMA = cv.invalid("The 'esp32_ble' component has been renamed to the 'esp32_ble_tracker' "
                           "component in order to avoid confusion with the new 'esp32_ble_beacon' "
                           "component.")
