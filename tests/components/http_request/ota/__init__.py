import esphome.config_validation as cv
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The real schema rejects the host target by framework version; the update platform's
    # unit tests only need this platform's sources on the host.
    manifest.config_schema = cv.Schema({}, extra=cv.ALLOW_EXTRA)
