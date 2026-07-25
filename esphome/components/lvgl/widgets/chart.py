import esphome.codegen as cg
from esphome.components import sensor, time
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLOR,
    CONF_FORMAT,
    CONF_HEIGHT,
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_SENSOR,
    CONF_TIME_ID,
    CONF_TYPE,
    CONF_UPDATE_INTERVAL,
    CONF_WIDTH,
)
from esphome.helpers import fnv1_hash

from ..defines import (
    CONF_CURSOR,
    CONF_DECIMALS,
    CONF_INDICATOR,
    CONF_ITEMS,
    CONF_MAIN,
    CONF_PERSIST,
    CONF_POINT_COUNT,
    CONF_POINT_RADIUS,
    CONF_SERIES,
    CONF_Y_AXIS,
    LV_CHART_TYPE,
    add_lv_use,
    literal,
)
from ..lv_validation import lv_color, lv_float, pixels, size
from ..lvcode import lv, lv_add
from ..types import LvCompound, LvType
from . import Widget, WidgetType

CONF_CHART = "chart"

# Bump when the ChartStore blob layout changes so stale NVS blobs are
# ignored instead of read back into an incompatible struct. This should match
# the runtime constant in lvgl_esphome.h/ChartStore
CHART_PERSIST_VERSION = 1

lv_chart_t = LvType("LvChartType", parents=(LvCompound,))

SERIES_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_COLOR, default=0): lv_color,
        cv.Optional(CONF_FORMAT): cv.string,
    }
)


def validate_persist(value):
    value = cv.boolean(value)
    if value:
        # Preference blobs big enough for chart history only fit ESP32's NVS; ESP8266's
        # flash-preference region is a few hundred bytes total, shared by every component.
        return cv.only_on_esp32(value)
    return value


def validate_min_max(config):
    if (CONF_MIN_VALUE in config) != (CONF_MAX_VALUE in config):
        raise cv.Invalid(
            f"If either {CONF_MIN_VALUE} or {CONF_MAX_VALUE} is set, both must be set"
        )
    return config


def validate_chart(config):
    validate_min_max(config)
    # Bakes this chart's own series count/point_count into its declared C++ type as template
    # arguments (LvChartType<N_SERIES, N_POINTS>), so its persisted-history blob is sized to
    # exactly what this instance needs instead of sharing one struct/size with every other chart.
    # Must happen here (at config-validation time), not in to_code(): the generic widget-creation
    # framework (WidgetType.create_to_code) already instantiates the object from config[CONF_ID].type
    # before ChartType.to_code() ever runs.
    config[CONF_ID].type = lv_chart_t.template(
        len(config[CONF_SERIES]), config[CONF_POINT_COUNT]
    )
    # add_series()/enable_y_axis() create plain lv_label_create() labels directly (legend text,
    # y-axis min/max) without going through the "label" WidgetType -- so LV_USE_LABEL must be
    # requested explicitly here, same as button.py does for its own auto-created text label.
    if CONF_Y_AXIS in config or any(CONF_FORMAT in s for s in config[CONF_SERIES]):
        add_lv_use("label")
    return config


CHART_SCHEMA = cv.Schema(
    {
        # LVGL's native lv_chart default size (~2*LV_DPI_DEF per axis, ~260px) overflows small
        # displays; default to filling the parent instead, like container.py does, and let users
        # override with an explicit width/height as with any other widget.
        cv.Optional(CONF_HEIGHT, default="100%"): size,
        cv.Optional(CONF_WIDTH, default="100%"): size,
        cv.Optional(CONF_TYPE, default="LINE"): LV_CHART_TYPE.one_of,
        # Upper bound matches LvChartType's uint16_t N_POINTS template parameter
        cv.Optional(CONF_POINT_COUNT, default=100): cv.int_range(min=1, max=65535),
        cv.Optional(CONF_DECIMALS, default=1): cv.int_range(min=0, max=4),
        cv.Optional(CONF_MIN_VALUE): lv_float,
        cv.Optional(CONF_MAX_VALUE): lv_float,
        cv.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
        # LVGL draws point markers the same size for every series on a chart (LV_PART_INDICATOR is
        # a single chart-wide style, not settable per series) -- so this applies to all of them.
        cv.Optional(CONF_POINT_RADIUS): pixels,
        cv.Optional(CONF_PERSIST, default=False): validate_persist,
        cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        # When set, shows the current auto-rounded (or explicit) Y-range bounds as two labels,
        # formatted with this format string (e.g. "%.0f°").
        cv.Optional(CONF_Y_AXIS): cv.Schema({cv.Required(CONF_FORMAT): cv.string}),
        # max=255 matches LvChartType's uint8_t N_SERIES template parameter
        cv.Required(CONF_SERIES): cv.All(
            cv.ensure_list(SERIES_SCHEMA), cv.Length(min=1, max=255)
        ),
    }
).add_extra(validate_chart)

# Only the Y-range can be changed after creation: series, point_count, persist, decimals and
# update_interval are all set up once at creation time (see ChartType.to_code below).
CHART_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_MIN_VALUE): lv_float,
        cv.Optional(CONF_MAX_VALUE): lv_float,
    }
).add_extra(validate_min_max)


class ChartType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_CHART,
            lv_chart_t,
            parts=(CONF_MAIN, CONF_ITEMS, CONF_INDICATOR, CONF_CURSOR),
            schema=CHART_SCHEMA,
            modify_schema=CHART_MODIFY_SCHEMA,
        )

    async def to_code(self, w: Widget, config: dict):
        # to_code() also runs for `lvgl.chart.update`, whose config only ever contains keys from
        # CHART_MODIFY_SCHEMA (min/max) -- CONF_SERIES's presence is what distinguishes the
        # one-time creation setup below from a plain range update.
        if CONF_SERIES in config:
            lv.chart_set_type(w.obj, literal(config[CONF_TYPE]))
            lv_add(w.var.set_decimals(config[CONF_DECIMALS]))
            lv_add(w.var.set_point_count(config[CONF_POINT_COUNT]))
            if (radius := config.get(CONF_POINT_RADIUS)) is not None:
                lv_add(w.var.set_point_radius(await pixels.process(radius)))
            series = config[CONF_SERIES]
            lv_add(w.var.init_series(len(series)))
            for s in series:
                svar = await cg.get_variable(s[CONF_SENSOR])
                color = await lv_color.process(s[CONF_COLOR])
                fmt = s.get(CONF_FORMAT, "")
                lv_add(w.var.add_series(svar, color, fmt))
            if (interval := config.get(CONF_UPDATE_INTERVAL)) is not None:
                lv_add(w.var.set_update_interval(interval.total_milliseconds))
            if config[CONF_PERSIST]:
                # Key depends on the full storage layout (series count + point count) and
                # a format version, so changing any of them yields a new preference key
                # instead of restoring an incompatible saved blob. Stable across reboots.
                hash_ = fnv1_hash(
                    f"lv_chart/{CHART_PERSIST_VERSION}/{config[CONF_ID]}/"
                    f"{len(config[CONF_SERIES])}/{config[CONF_POINT_COUNT]}"
                )
                if (time_id := config.get(CONF_TIME_ID)) is not None:
                    lv_add(w.var.set_time(await cg.get_variable(time_id)))
                lv_add(w.var.enable_persistence(hash_))
            if (y_axis := config.get(CONF_Y_AXIS)) is not None:
                lv_add(w.var.enable_y_axis(y_axis[CONF_FORMAT]))
        if CONF_MIN_VALUE in config:
            lv_add(
                w.var.set_y_range(
                    await lv_float.process(config[CONF_MIN_VALUE]),
                    await lv_float.process(config[CONF_MAX_VALUE]),
                )
            )
        if CONF_SERIES in config:
            # Must run after set_y_range so an explicit min/max at creation isn't immediately
            # overwritten by the first auto-range recompute.
            lv_add(w.var.start_updates())


chart_spec = ChartType()
