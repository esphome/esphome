from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # touchscreen.h includes display.h
    manifest.dependencies = manifest.dependencies + ["display"]
