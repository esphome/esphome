import esphome.config_validation as cv
from esphome.core import HexInt
from esphome.cpp_generator import RawExpression
from esphome.yaml_util import ESPHomeDumper


class ServiceKey:
    def __init__(self, *parts):
        if len(parts) != 8:
            raise ValueError("Service Key must consist of 8 items")
        self.parts = parts

    def __str__(self):
        return ':'.join(f'{part:02X}' for part in self.parts)

    @property
    def as_hex(self):
        num = ''.join(f'{part:02X}' for part in self.parts)
        return RawExpression(f'0x{num}ULL')

    @property
    def as_hex_int(self):
        return [HexInt(i) for i in self.parts]


def create_service_key(value):
    value = cv.string_strict(value)
    parts = value.split(':')
    if len(parts) != 8:
        raise cv.Invalid("Service Key must consist of 8 : (colon) separated parts")
    parts_int = []
    if any(len(part) != 2 for part in parts):
        raise cv.Invalid("Service Key must be format XX:XX:XX:XX:XX:XX:XX:XX")
    for part in parts:
        try:
            parts_int.append(int(part, 8))
        except ValueError:
            raise cv.Invalid("Service Key parts must be hexadecimal values from 00 to FF")
    return ServiceKey(*parts_int)


ESPHomeDumper.add_multi_representer(ServiceKey, ESPHomeDumper.represent_stringify)
