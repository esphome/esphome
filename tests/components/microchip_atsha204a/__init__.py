import esphome.config_validation as cv
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The unit tests construct the hub with a fake transport.
    manifest.config_schema = cv.Schema({})
    manifest.final_validate_schema = None
