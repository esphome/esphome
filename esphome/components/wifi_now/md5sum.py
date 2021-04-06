from esphome.core import HexInt
from hashlib import md5

def get_md5sum(text):
    m = md5()
    m.update(text.encode())
    return m.digest()

def get_md5sum_hexint(text, length=None):
    parts = get_md5sum(text)
    if length is not None:
        parts = parts[:length]
    return [HexInt(i) for i in parts]
