import esphome.config_validation as cv
from esphome.const import CONF_USE_ADDRESS
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Only the reply parsers are tested; the build still needs the address a modem config provides.
    manifest.dependencies = []
    manifest.config_schema = cv.Schema(
        {cv.Optional(CONF_USE_ADDRESS, default="test.local"): cv.string_strict}
    )
