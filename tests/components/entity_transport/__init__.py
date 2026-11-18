import esphome.codegen as cg
from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("ENTITY_TRANSPORT_REMOTE_BINARY_SENSOR_COUNT", 2)
        cg.add_define("ENTITY_TRANSPORT_REMOTE_SENSOR_COUNT", 2)
        cg.add_define("USE_PACKET_INTERFACE_BASIC_SESSION")
        # No entity platforms are configured, so count the test's own entities to emit USE_SENSOR and friends
        CORE.platform_counts["sensor"] += 2
        CORE.platform_counts["binary_sensor"] += 2

    manifest.to_code = to_code_testing
    # A multi_conf component gets an empty list in the test build, so to_code would not run
    manifest.multi_conf = False
    manifest.dependencies = manifest.dependencies + ["sensor", "binary_sensor"]
