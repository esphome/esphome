from tests.testing_helpers import ComponentManifestOverride, add_defines_to_code


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # datetime_entity.h only declares DateTimeEntity with the define the real to_code emits
    add_defines_to_code(manifest, "USE_DATETIME", "USE_DATETIME_DATETIME")
