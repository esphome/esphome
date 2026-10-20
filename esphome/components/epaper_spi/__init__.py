from dataclasses import dataclass, field

from esphome.core import CORE, EsphomeError

CODEOWNERS = ["@esphome/core"]
DOMAIN = "epaper_spi"

# The source file that every model needs.
BASE_SOURCE_FILE = "epaper_spi.cpp"

# Maps each C++ driver class to its source file and the class it derives from.
# A parent of None means the class derives directly from EPaperBase.
# Every driver class used by a model, and every .cpp file in this directory
# except epaper_spi.cpp, must have an entry, or its file is left out of the build.
CLASS_SOURCES: dict[str, tuple[str, str | None]] = {
    "EPaper4bpp": ("epaper_spi_4bpp.cpp", None),
    "EPaperInkplate2": ("epaper_spi_inkplate2.cpp", None),
    "EPaperInkplate6Color": ("epaper_spi_inkplate6color.cpp", "EPaper4bpp"),
    "EPaperJD79660": ("epaper_spi_jd79660.cpp", None),
    "EPaperMono": ("epaper_spi_mono.cpp", None),
    "EPaperSpectraE6": ("epaper_spi_spectra_e6.cpp", "EPaper4bpp"),
    "EPaperSSD1677": ("epaper_spi_ssd1677.cpp", "EPaperMono"),
    "EPaperSSD1677Gray4": ("epaper_spi_ssd1677_gray4.cpp", "EPaperSSD1677"),
    "EPaperSSD1683": ("epaper_spi_ssd1683.cpp", "EPaperMono"),
    "EPaperT133A01": ("epaper_spi_t133a01.cpp", None),
    "EPaperUC8179": ("epaper_spi_uc8179.cpp", "EPaperUltraChip"),
    "EPaperUC8279": ("epaper_spi_uc8279.cpp", "EPaperUltraChip"),
    "EPaperUltraChip": ("epaper_spi_ultrachip.cpp", None),
    "EpaperWaveshare": ("epaper_waveshare.cpp", "EPaperMono"),
    "EpaperWaveshareB": ("epaper_waveshare_b.cpp", "EPaperWeAct3C"),
    "EPaperWaveshareBWR": ("epaper_waveshare_bwr.cpp", None),
    "EPaperWeAct3C": ("epaper_weact_3c.cpp", None),
}


@dataclass
class EpaperSpiData:
    needed_sources: set[str] = field(default_factory=set)


def _get_data() -> EpaperSpiData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = EpaperSpiData()
    return CORE.data[DOMAIN]


def register_class(class_name: str) -> None:
    """Record the source files that a configured model's C++ driver class needs."""
    needed = _get_data().needed_sources
    current: str | None = class_name
    while current is not None:
        if current not in CLASS_SOURCES:
            raise EsphomeError(
                f"epaper_spi driver class '{current}' is missing from CLASS_SOURCES in "
                "esphome/components/epaper_spi/__init__.py. Add an entry mapping it to "
                "its .cpp file and the class it derives from (None for EPaperBase)."
            )
        source, current = CLASS_SOURCES[current]
        needed.add(source)


def FILTER_SOURCE_FILES() -> list[str]:  # noqa: N802
    """Exclude the driver sources that no configured model needs."""
    if not (needed := _get_data().needed_sources):
        return []
    return sorted(
        source for source, _ in CLASS_SOURCES.values() if source not in needed
    )
