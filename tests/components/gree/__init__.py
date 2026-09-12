from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The gree root manifest only defines the namespace. Its C++ implementation
    # is used by the climate platform, so host unit tests must request the
    # platform's base components explicitly.
    manifest.dependencies = manifest.dependencies + ["climate", "climate_ir", "switch"]
