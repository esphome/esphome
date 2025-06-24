# Sourced from https://gist.github.com/agners/0338576e0003318b63ec1ea75adc90f9
import binascii
import ipaddress

from esphome.const import CONF_CHANNEL

from . import (
    CONF_EXT_PAN_ID,
    CONF_MESH_LOCAL_PREFIX,
    CONF_NETWORK_KEY,
    CONF_NETWORK_NAME,
    CONF_PAN_ID,
    CONF_PSKC,
)

TLV_TYPES = {
    0: CONF_CHANNEL,
    1: CONF_PAN_ID,
    2: CONF_EXT_PAN_ID,
    3: CONF_NETWORK_NAME,
    4: CONF_PSKC,
    5: CONF_NETWORK_KEY,
    7: CONF_MESH_LOCAL_PREFIX,
}


def parse_tlv(tlv) -> dict:
    data = binascii.a2b_hex(tlv)
    output = {}
    pos = 0
    while pos < len(data):
        tag = data[pos]
        pos += 1
        _len = data[pos]
        pos += 1
        val = data[pos : pos + _len]
        pos += _len
        if tag in TLV_TYPES:
            if tag == 3:
                output[TLV_TYPES[tag]] = val.decode("utf-8")
            elif tag == 7:
                mesh_local_prefix = binascii.hexlify(val).decode("utf-8")
                mesh_local_prefix_str = f"{mesh_local_prefix}0000000000000000"
                ipv6_bytes = bytes.fromhex(mesh_local_prefix_str)
                ipv6_address = ipaddress.IPv6Address(ipv6_bytes)
                output[TLV_TYPES[tag]] = f"{ipv6_address}/64"
            else:
                output[TLV_TYPES[tag]] = int.from_bytes(val)
    return output


def main():
    import sys

    args = sys.argv[1:]
    parsed = parse_tlv(args[0])
    # print the parsed TLV data
    for key, value in parsed.items():
        if isinstance(value, bytes):
            value = value.hex()
        print(f"{key}: {value}")


if __name__ == "__main__":
    main()
