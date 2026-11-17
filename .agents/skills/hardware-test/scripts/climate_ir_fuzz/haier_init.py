from esphome.components import remote_base
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.auto_load = ["climate_ir", "climate"]

    async def to_code_testing(config: ConfigType) -> None:
        remote_base.request_protocol("haier")

    manifest.to_code = to_code_testing
