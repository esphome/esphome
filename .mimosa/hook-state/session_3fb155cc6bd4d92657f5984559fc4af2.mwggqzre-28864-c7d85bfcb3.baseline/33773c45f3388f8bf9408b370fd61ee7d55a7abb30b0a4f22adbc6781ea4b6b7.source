from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # core (esphome/core/config.py) must run its to_code during builds
    # because it bootstraps the fundamental application infrastructure.
    manifest.enable_codegen()
