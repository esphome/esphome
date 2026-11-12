BOOTLOADER_ADAFRUIT = "adafruit"
BOOTLOADER_ADAFRUIT_NRF52_SD132 = "adafruit_nrf52_sd132"
BOOTLOADER_ADAFRUIT_NRF52_SD132_V2 = "adafruit_nrf52_sd132_v2"
BOOTLOADER_ADAFRUIT_NRF52_SD132_V6 = "adafruit_nrf52_sd132_v6"
BOOTLOADER_ADAFRUIT_NRF52_SD140_V6 = "adafruit_nrf52_sd140_v6"
BOOTLOADER_ADAFRUIT_NRF52_SD140_V7 = "adafruit_nrf52_sd140_v7"
BOOTLOADER_NRF = "nrf"

VARIANT_NRF51822 = "NRF51822"
VARIANT_NRF52832 = "NRF52832"
VARIANT_NRF52840 = "NRF52840"
VARIANT_NRF54L15 = "NRF54L15"
VARIANT_NRF54LM20 = "NRF54LM20"

# Number of pins on each GPIO port (P0, P1, ...)
VARIANT_PORT_PINS = {
    VARIANT_NRF51822: (32,),
    VARIANT_NRF52832: (32,),
    VARIANT_NRF52840: (32, 16),
    VARIANT_NRF54L15: (7, 17, 11),
    VARIANT_NRF54LM20: (10, 32, 11, 13),
}

# Devicetree label of the console UART, which the logger calls UART0
VARIANT_CONSOLE_UART = {VARIANT_NRF54L15: "uart20", VARIANT_NRF54LM20: "uart20"}
# Devicetree label of the I2C (TWIM) instance for each GPIO port. A TWIM uses the pins of its own power
# domain: P0 is in the low power domain (instance 30), P1 and P3 in the peripheral domain (instances 2x,
# of which 20 holds the console UART); P2 has no TWIM
VARIANT_I2C_BY_PORT = {
    VARIANT_NRF54L15: {0: "i2c30", 1: "i2c22"},
    VARIANT_NRF54LM20: {0: "i2c30", 1: "i2c22", 3: "i2c22"},
}
VARIANTS_WITHOUT_USB = frozenset({VARIANT_NRF52832, VARIANT_NRF54L15})
# Devicetree label of the main regulator, reg1 on the others
VARIANT_REGULATOR = {VARIANT_NRF52832: "reg"}
VARIANT_FLASH_SIZE = {VARIANT_NRF52832: 0x80000, VARIANT_NRF52840: 0x100000}

CONF_DCDC = "dcdc"

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

FLASH_SECTOR_SIZE = 0x1000  # nRF52 flash page

# zephyr_mcumgr OTA keys, shared with the upload path
CONF_TRANSPORT = "transport"
CONF_UDP = "udp"

# SMP over UDP fits each frame in Thread's IPv6 minimum link MTU. The host passes it to
# smpclient, which subtracts the IP and UDP headers; the device sizes its receive buffer
# for the largest payload that leaves. The port and header sizes match smpclient's.
SMP_UDP_LINK_MTU = 1280
SMP_UDP_PORT = 1337  # smpclient and MCUMGR_TRANSPORT_UDP_PORT default
IPV4_UDP_HEADERS = 28
IPV6_UDP_HEADERS = 48
