from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The C++ tests build the hub directly and drive its sensors
    manifest.dependencies = manifest.dependencies + ["sensor"]
