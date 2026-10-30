BOOTLOADER_ADAFRUIT = "adafruit"
BOOTLOADER_ADAFRUIT_NRF52_SD132 = "adafruit_nrf52_sd132"
BOOTLOADER_ADAFRUIT_NRF52_SD140_V6 = "adafruit_nrf52_sd140_v6"
BOOTLOADER_ADAFRUIT_NRF52_SD140_V7 = "adafruit_nrf52_sd140_v7"
BOOTLOADER_NRF = "nrf"

EXTRA_ADC = [
    "VDD",
    "VDDHDIV5",
]
AIN_TO_GPIO = {
    "AIN0": 2,
    "AIN1": 3,
    "AIN2": 4,
    "AIN3": 5,
    "AIN4": 28,
    "AIN5": 29,
    "AIN6": 30,
    "AIN7": 31,
}

# SMP over UDP fits each frame in Thread's IPv6 minimum link MTU. The host passes it to
# smpclient, which subtracts the IP and UDP headers; the device sizes its receive buffer
# for the largest payload that leaves.
SMP_UDP_LINK_MTU = 1280
SMP_UDP_PORT = 1337  # smpclient and MCUMGR_TRANSPORT_UDP_PORT default
IPV4_UDP_HEADERS = 28
IPV6_UDP_HEADERS = 48
