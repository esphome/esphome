import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        # The gtests attach every entity kind the hub can publish to; the entities are not
        # registered with the application, so the controller tables only need to exist
        for kind in ("SENSOR", "BINARY_SENSOR", "TEXT_SENSOR"):
            cg.add_define(f"USE_{kind}")
            cg.add_define(f"ESPHOME_ENTITY_{kind}_COUNT", 1)

    manifest.to_code = to_code_testing
    # The frames come from the test, not from a receiver on a GPIO
    manifest.dependencies = ["remote_base", "sensor", "binary_sensor", "text_sensor"]
