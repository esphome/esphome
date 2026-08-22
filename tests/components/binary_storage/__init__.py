import esphome.codegen as cg
from esphome.core import CORE
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # inplace_kv.cpp is gated by USE_BINARY_STORAGE_INPLACE_KV, normally emitted by a kv region.
    #  The C++ unit test drives InplaceKVStore over a mock RawStorage, so emit the define
    # here and keep only that source: adding "inplace_kv" to the device-types set makes
    # FILTER_SOURCE_FILES exclude the hardware device drivers (SPI/I2C/OneWire), which do not build
    # on the host.
    async def to_code_testing(config):
        cg.add_define("USE_BINARY_STORAGE_INPLACE_KV")
        CORE.data.setdefault("binary_storage_device_types", set()).add("inplace_kv")

    manifest.to_code = to_code_testing
