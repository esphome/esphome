from esphome.components import remote_base
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The harness walks the component manifest, not the climate platform, so declare its auto-load here
    manifest.auto_load = ["climate_ir", "climate"]

    async def to_code_testing(config: ConfigType) -> None:
        # The platform's to_code never runs here; without this request remote_base drops aeha_protocol.cpp
        remote_base.request_protocol("aeha")

    manifest.to_code = to_code_testing
