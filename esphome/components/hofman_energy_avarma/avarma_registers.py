from esphome.const import (
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_SWITCH,
    DEVICE_CLASS_VOLUME_FLOW_RATE,
)

from .avarma_register import (
    AvarmaRegister,
    AvarmaRegisterBinaryRegisterBit,
    TempRegister,
)

AVARMA_SENSOR_REGISTERS = [
    TempRegister(0x1106, "C00", "Coil temp", -30, 97),
    TempRegister(0x1107, "C01", "Discharge temp", -30, 128),
    TempRegister(0x1108, "C02", "Ambient temp", -30, 97),
    TempRegister(0x1109, "C03", "Suction temp", -30, 97),
    TempRegister(0x110A, "C04", "EVI inlet temp", -30, 97),
    TempRegister(0x110B, "C05", "EVI outlet temp", -30, 97),
    TempRegister(0x110C, "C06", "Refrigerant liquid temperature", -30, 97),
    TempRegister(0x110D, "C07", "Water inlet temperature", -30, 97),
    TempRegister(0x110E, "C08", "Water outlet temperature", -30, 97),
    TempRegister(0x110F, "C09", "DHW tank temperature", -30, 97),
    AvarmaRegister(
        0x1110,
        "C10",
        "Water flow",
        0,
        100,
        unit_of_measurement="L/min",
        device_class=DEVICE_CLASS_VOLUME_FLOW_RATE,
        register_factor=0.1,
    ),
    TempRegister(0x1111, "C11", "Main circulation temperature differential", -30, 97),
    TempRegister(0x1112, "C12", "EVI circulation temperature differential", -30, 97),
    AvarmaRegister(
        0x1113,
        "C13",
        "High pressure",
        -100,
        100,
        unit_of_measurement="MPa",
        device_class=DEVICE_CLASS_PRESSURE,
        accuracy_decimals=2,
        register_factor=0.01,
    ),
    AvarmaRegister(
        0x1114,
        "C14",
        "Low pressure",
        -100,
        100,
        unit_of_measurement="MPa",
        device_class=DEVICE_CLASS_PRESSURE,
        accuracy_decimals=2,
        register_factor=0.01,
    ),
    AvarmaRegister(
        0x1115,
        "C15",
        "Compressor running frequency",
        unit_of_measurement="HZ",
        device_class=DEVICE_CLASS_FREQUENCY,
    ),
    AvarmaRegister(
        0x1116,
        "C16",
        "Fan motor 1 speed",
        unit_of_measurement="rpm",
        device_class=DEVICE_CLASS_FREQUENCY,
    ),
    AvarmaRegister(
        0x111A,
        "C20",
        "Compressor target frequency",
        unit_of_measurement="HZ",
        device_class=DEVICE_CLASS_FREQUENCY,
    ),
    AvarmaRegister(
        0x111B,
        "C21",
        "Compressor input current",
        unit_of_measurement="A",
        device_class=DEVICE_CLASS_CURRENT,
        register_factor=0.1,
    ),
]

AVARMA_BINARY_REGISTERS = [
    AvarmaRegister(
        0x1102,
        "C31-C35",
        "Operating State",
        flags=[
            AvarmaRegisterBinaryRegisterBit("Standby / Shutdown", 0),
            AvarmaRegisterBinaryRegisterBit("Power on State", 1),
            AvarmaRegisterBinaryRegisterBit("Downtime State", 2),
            AvarmaRegisterBinaryRegisterBit("Alarm Power on State", 3),
            AvarmaRegisterBinaryRegisterBit("Defrosting State", 4),
            AvarmaRegisterBinaryRegisterBit("Sterilization Status", 8),
            AvarmaRegisterBinaryRegisterBit("Antifreeze State", 9),
            AvarmaRegisterBinaryRegisterBit("Floor Drying", 10),
            AvarmaRegisterBinaryRegisterBit("PV Mode", 11),
        ],
        device_class=None,
    ),
    AvarmaRegister(
        0x1103,
        "C36-C44",
        "Output State",
        flags=[
            AvarmaRegisterBinaryRegisterBit("Four-way Valve", 0),
            AvarmaRegisterBinaryRegisterBit("Crankshaft Heating Belt", 1),
            AvarmaRegisterBinaryRegisterBit("C1 Pump", 3),
            AvarmaRegisterBinaryRegisterBit("C2 Pump", 4),
            AvarmaRegisterBinaryRegisterBit("C3 Pump", 5),
            AvarmaRegisterBinaryRegisterBit("E1 Electrical Heating", 6),
            AvarmaRegisterBinaryRegisterBit("E2 Electrical Heating", 7),
            AvarmaRegisterBinaryRegisterBit("G1 Valve", 8),
            AvarmaRegisterBinaryRegisterBit("G2 Valve", 9),
        ],
        device_class=None,
    ),
]

AVARMA_SWITCH_REGISTERS = [
    AvarmaRegister(0x1000, "P00", "ON/OFF", device_class=DEVICE_CLASS_SWITCH),
]


AVARMA_NUMBER_REGISTERS = [
    AvarmaRegister(
        8198,
        "P08",
        "A/C Heating AU maximum temperature",
        device_class=None,
        register_factor=10.0,
        min=35,
        max=75,
    ),
]
