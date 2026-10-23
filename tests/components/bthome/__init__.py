import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:

    # This function is executed instead of to_code() during c++ testing
    async def to_code_testing(config):
        # During testing, enable all sensor types unconditionally.
        # The entity count defines are normally generated from CORE.platform_counts by
        # _add_platform_defines(). Sensor counts are set via the sensor.bthome platform component.
        cg.add_define("USE_SENSOR")
        cg.add_define("USE_EVENT")

    manifest.to_code = to_code_testing
    manifest.auto_load = manifest.auto_load + ["sensor", "event"]
