# ESP-IDF 5.5.x workaround for ESP32-P4 MIPI DSI + DMA2D.
#
# The DPI panel driver's async framebuffer copy path calls esp_cache_msync()
# for every LVGL draw buffer. The optimized MIPI DSI component performs the
# required cache writeback before handing an internal draw buffer to ESP-IDF, so
# this patch skips ESP-IDF's duplicate sync for internal memory buffers.
#
# Remove this once the framework package contains the same guard upstream.

# ruff: noqa: F821
# pylint: disable=undefined-variable
from pathlib import Path

Import("env")  # type: ignore[name-defined]


def main() -> None:
    framework_dir = env.PioPlatform().get_package_dir("framework-espidf")
    if not framework_dir:
        raise RuntimeError("framework-espidf package directory not found")

    target = (
        Path(framework_dir) / "components" / "esp_lcd" / "dsi" / "esp_lcd_panel_dpi.c"
    )
    text = target.read_text(encoding="utf-8")

    for include in (
        '#include "esp_memory_utils.h"',
        '#include "hal/cache_ll.h"',
        '#include "soc/soc_caps.h"',
    ):
        if include not in text:
            text = text.replace(
                '#include "esp_cache.h"\n', f'#include "esp_cache.h"\n{include}\n'
            )

    helper = """
static bool dpi_panel_skip_draw_buffer_msync(const void *draw_buffer)
{
    if (esp_ptr_internal(draw_buffer)) {
        return true;
    }
#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE && defined(CACHE_LL_L2MEM_CACHE_ADDR)
    const void *cache_addr = (const void *)CACHE_LL_L2MEM_CACHE_ADDR(draw_buffer);
    if (esp_ptr_internal(cache_addr)) {
        return true;
    }
#endif
    return false;
}
"""
    if "dpi_panel_skip_draw_buffer_msync" not in text:
        anchor = (
            "static esp_err_t dpi_panel_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, "
            "int x_end, int y_end, const void *color_data);\n"
        )
        if anchor not in text:
            raise RuntimeError(
                "ESP-IDF DSI draw_bitmap declaration not found; patch needs review"
            )
        text = text.replace(anchor, f"{anchor}{helper}", 1)

    old = (
        "        esp_cache_msync(draw_buffer, color_data_size, "
        "ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);"
    )
    old_guard = (
        "        if (!esp_ptr_internal(draw_buffer)) {\n"
        "            esp_cache_msync(draw_buffer, color_data_size, "
        "ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);\n"
        "        }"
    )
    new = (
        "        if (!dpi_panel_skip_draw_buffer_msync(draw_buffer)) {\n"
        "            esp_cache_msync(draw_buffer, color_data_size, "
        "ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);\n"
        "        }"
    )

    if new in text:
        print(
            "MIPI DSI patch: ESP-IDF DMA2D internal-buffer cache sync guard already present"
        )
        return
    if old_guard in text:
        text = text.replace(old_guard, new)
    elif old in text:
        text = text.replace(old, new)
    else:
        raise RuntimeError(
            "ESP-IDF DSI DMA2D cache sync line not found; patch needs review"
        )

    target.write_text(text, encoding="utf-8")
    print("MIPI DSI patch: applied ESP-IDF DMA2D internal-buffer cache sync guard")


main()
