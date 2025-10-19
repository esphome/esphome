from esphome.components.modbus_controller import SensorValueType
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_TEMPERATURE


class AvarmaRegisterBinaryRegisterBit:
    def __init__(self, name, bit_mumber=0):
        self.bitmask = 0 + 1 << bit_mumber
        self.name = name


class AvarmaRegister:
    def __init__(
        self,
        address,
        parameter_id,
        description="",
        min=None,
        max=None,
        register_factor=1.0,
        unit_of_measurement="°C",
        accuracy_decimals=1,
        entity_category=cv.ENTITY_CATEGORY_NONE,
        device_class=DEVICE_CLASS_TEMPERATURE,
        flags=None,
        value_type=SensorValueType.S_WORD,
    ):
        self.address = address
        self.parameter_id = parameter_id
        self.description = description
        self.min = min
        self.max = max
        self.register_factor = register_factor
        self.entity_category = entity_category
        self.unit_of_measurement = unit_of_measurement
        self.accuracy_decimals = accuracy_decimals
        self.device_class = device_class
        self.flags = flags
        self.value_type = value_type
        self.name = f"{parameter_id} - {description}"


class TempRegister(AvarmaRegister):
    def __init__(
        self,
        address,
        parameter_id,
        description="",
        min=None,
        max=None,
    ):
        super().__init__(
            address, parameter_id, description, min, max, register_factor=0.1
        )
