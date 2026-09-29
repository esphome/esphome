"""ESP-IDF direct build support.

Deliberately light: the upload fast path imports submodules of this
package without the esp32 component package, so nothing here may pull
in codegen or validation.
"""


def variant_to_idf_target(variant: str) -> str:
    """Map an esp32 variant name (e.g. "ESP32S3") to its ESP-IDF target name."""
    return variant.lower().replace("-", "")
