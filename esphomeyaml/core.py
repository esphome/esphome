class ESPHomeYAMLError(Exception):
    """General esphomeyaml exception occurred."""
    pass


class HexInt(long):
    def __str__(self):
        return "0x{:X}".format(self)


class IPAddress(object):
    def __init__(self, *args):
        if len(args) != 4:
            raise ValueError(u"IPAddress must consist up 4 items")
        self.args = args

    def __str__(self):
        return '.'.join(str(x) for x in self.args)


CONFIG_PATH = None
SIMPLIFY = True
ESP_PLATFORM = ''
BOARD = ''
RAW_CONFIG = None
