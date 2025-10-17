from .avarma_register import AvarmaRegister, AvarmaRegisterBinaryRegisterBit

AVARMA_SENSOR_REGISTERS = [
    AvarmaRegister(0x1106, "C00", "Coil temp", -30, 97, register_factor=0.1),
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
    AvarmaRegister(0x1000, "P00", "ON/OFF"),
]
