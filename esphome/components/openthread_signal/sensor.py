import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_DECIBEL_MILLIWATT,
)

# Parent RSSI
CONF_PARENT_AVG_RSSI = "parent_avg_rssi"
CONF_PARENT_LAST_RSSI = "parent_last_rssi"

# Link counters: Enumeration table to ease systematic generation
# Key: Config name (public, do not change!)
# Value: Tuple
#   C++ Class Name sensor prefix (internal)
#   Human readable description for logging (internal)
CONFLIST_LC = {
    "lc_rx_addr_filtered": ("RxAddressFiltered", "RX address filtered"),
    "lc_rx_err_fcs": ("RxErrFcs", "RX FCS errors"),
    "lc_rx_err_noframe": ("RxErrNoFrame", "RX No Frame errors"),
    "lc_rx_err_other": ("RxErrOther", "RX other errors"),
    "lc_rx_err_sec": ("RxErrSec", "RX SEC errors"),
    "lc_rx_err_unknownneighbor": ("RxErrUnknownNeighbor", "RX unknown neighbor errors"),
    "lc_rx_total": ("RxTotal", "RX total"),
    "lc_tx_err_abort": ("TxErrAbort", "TX abort errors"),
    "lc_tx_err_busychannel": ("TxErrBusyChannel", "TX busy channel errors"),
    "lc_tx_err_cca": ("TxErrCca", "TX CCA errors"),
    "lc_tx_retry": ("TxRetry", "TX retries"),
    "lc_tx_total": ("TxTotal", "TX total"),
}

# Suffix for counters
SUFFIX_COUNTER = "Counter"
# Common C++ Class Name suffix added
SUFFIX_OTSIGNAL = "OpenThreadSignal"


DEPENDENCIES = ["openthread"]

openthread_signal_ns = cg.esphome_ns.namespace("openthread_signal")


# Helper to make C++ class representation object for provided prefix
def MakeClass(class_prefix):
    return openthread_signal_ns.class_(
        class_prefix + SUFFIX_OTSIGNAL, sensor.Sensor, cg.PollingComponent
    )


CONFIG_SCHEMA = cv.Schema(
    {
        # Parent RSSI
        cv.Optional(CONF_PARENT_AVG_RSSI): sensor.sensor_schema(
            MakeClass("ParentAverageRssi"),
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("60s")),
        cv.Optional(CONF_PARENT_LAST_RSSI): sensor.sensor_schema(
            MakeClass("ParentLastRssi"),
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("60s")),
        # Link counters
        **{
            cv.Optional(conf_key): sensor.sensor_schema(
                MakeClass(conf_value[0] + SUFFIX_COUNTER),
                accuracy_decimals=0,
                state_class=STATE_CLASS_TOTAL_INCREASING,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ).extend(cv.polling_component_schema("60s"))
            for conf_key, conf_value in CONFLIST_LC.items()
        },
    }
)


async def setup_conf(config: dict, key: str, *initarg):
    if conf := config.get(key):
        var = await sensor.new_sensor(conf, *initarg)
        await cg.register_component(var, conf)


async def to_code(config):
    # Parent RSSI
    await setup_conf(config, CONF_PARENT_AVG_RSSI)
    await setup_conf(config, CONF_PARENT_LAST_RSSI)

    # Link counters
    for conf_key, conf_value in CONFLIST_LC.items():
        await setup_conf(config, conf_key, conf_value[1])
