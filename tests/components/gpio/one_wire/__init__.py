import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Test builds generate esphome/core/defines.h from cg.add_define(). The tests build the
    # engine directly, so nothing of the platform's own to_code is needed.
    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("USE_ONE_WIRE_RMT")

    manifest.to_code = to_code_testing
    # Only the hardware-free layers exist on the host: the bus classes need ISR pin methods
    # and the RMT driver that the host platform does not have.
    manifest.resources = [
        resource
        for resource in manifest.resources
        if resource.resource.startswith("rmt_one_wire_")
    ]
