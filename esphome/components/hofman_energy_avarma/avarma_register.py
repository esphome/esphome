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
        start_address: int,
        register_offset: int,
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
        step=1.0,
        deactivated=False,
    ):
        self.start_address = start_address
        self.register_offset = register_offset
        self.address = self.start_address + self.register_offset
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
        self.step = step
        self.value_type = value_type
        self.name = f"{parameter_id} - {description}"
        self.deactivated = deactivated


class TempRegister(AvarmaRegister):
    def __init__(
        self,
        address,
        register_offset,
        parameter_id,
        description="",
        min=-30.0,
        max=97.0,
        deactivated=False,
    ):
        super().__init__(
            address,
            register_offset,
            parameter_id,
            description,
            min,
            max,
            register_factor=0.1,
            deactivated=deactivated,
        )


class NumberRegister(AvarmaRegister):
    def __init__(
        self,
        address,
        register_offset,
        parameter_id,
        description,
        min,
        max,
        step=1.0,
        register_factor=1.0,
        deactivated=False,
    ):
        self.step = step
        super().__init__(
            address,
            register_offset,
            parameter_id,
            description,
            min,
            max,
            register_factor=register_factor,
            deactivated=deactivated,
        )
