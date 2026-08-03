import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_DECIBEL_MILLIWATT,
    UNIT_EMPTY,
)

CONF_PARENT_AVERAGE_RSSI = "parent_average_rssi"
CONF_PARENT_LAST_RSSI = "parent_last_rssi"
CONF_PARENT_LINK_QUALITY_IN = "parent_link_quality_in"
CONF_PARENT_LINK_QUALITY_OUT = "parent_link_quality_out"
CONF_TX_TOTAL = "tx_total"
CONF_TX_RETRIES = "tx_retries"
CONF_TX_ERR_CCA = "tx_err_cca"
CONF_TX_ERR_ABORT = "tx_err_abort"
CONF_RX_TOTAL = "rx_total"
CONF_RX_ERR_FCS = "rx_err_fcs"
CONF_ATTACH_ATTEMPTS = "attach_attempts"
CONF_PARENT_CHANGES = "parent_changes"
CONF_PARTITION_ID_CHANGES = "partition_id_changes"

DEPENDENCIES = ["openthread"]

openthread_info_ns = cg.esphome_ns.namespace("openthread_info")
ParentAverageRssiOpenThreadInfo = openthread_info_ns.class_(
    "ParentAverageRssiOpenThreadInfo", sensor.Sensor, cg.PollingComponent
)
ParentLastRssiOpenThreadInfo = openthread_info_ns.class_(
    "ParentLastRssiOpenThreadInfo", sensor.Sensor, cg.PollingComponent
)
ParentLinkQualityInOpenThreadInfo = openthread_info_ns.class_(
    "ParentLinkQualityInOpenThreadInfo", sensor.Sensor, cg.PollingComponent
)
ParentLinkQualityOutOpenThreadInfo = openthread_info_ns.class_(
    "ParentLinkQualityOutOpenThreadInfo", sensor.Sensor, cg.PollingComponent
)
TxTotalOpenThreadInfo = openthread_info_ns.class_(
    "TxTotalOpenThreadInfo", sensor.Sensor, cg.PollingComponent
)
TxRetriesOpenThreadInfo = openthread_info_ns.class_(
    "TxRetriesOpenThreadInfo", sensor.Sensor, cg.PollingComponent
)
TxErrCcaOpenThreadInfo = openthread_info_ns.class_(
    "TxErrCcaOpenThreadInfo", sensor.Sensor, cg.PollingComponent
)
TxErrAbortOpenThreadInfo = openthread_info_ns.class_(
    "TxErrAbortOpenThreadInfo", sensor.Sensor, cg.PollingComponent
)
RxTotalOpenThreadInfo = openthread_info_ns.class_(
    "RxTotalOpenThreadInfo", sensor.Sensor, cg.PollingComponent
)
RxErrFcsOpenThreadInfo = openthread_info_ns.class_(
    "RxErrFcsOpenThreadInfo", sensor.Sensor, cg.PollingComponent
)
AttachAttemptsOpenThreadInfo = openthread_info_ns.class_(
    "AttachAttemptsOpenThreadInfo", sensor.Sensor, cg.PollingComponent
)
ParentChangesOpenThreadInfo = openthread_info_ns.class_(
    "ParentChangesOpenThreadInfo", sensor.Sensor, cg.PollingComponent
)
PartitionIdChangesOpenThreadInfo = openthread_info_ns.class_(
    "PartitionIdChangesOpenThreadInfo", sensor.Sensor, cg.PollingComponent
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PARENT_AVERAGE_RSSI): sensor.sensor_schema(
            ParentAverageRssiOpenThreadInfo,
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("5s")),
        cv.Optional(CONF_PARENT_LAST_RSSI): sensor.sensor_schema(
            ParentLastRssiOpenThreadInfo,
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("5s")),
        cv.Optional(CONF_PARENT_LINK_QUALITY_IN): sensor.sensor_schema(
            ParentLinkQualityInOpenThreadInfo,
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("5s")),
        cv.Optional(CONF_PARENT_LINK_QUALITY_OUT): sensor.sensor_schema(
            ParentLinkQualityOutOpenThreadInfo,
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("5s")),
        cv.Optional(CONF_TX_TOTAL): sensor.sensor_schema(
            TxTotalOpenThreadInfo,
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("30s")),
        cv.Optional(CONF_TX_RETRIES): sensor.sensor_schema(
            TxRetriesOpenThreadInfo,
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("30s")),
        cv.Optional(CONF_TX_ERR_CCA): sensor.sensor_schema(
            TxErrCcaOpenThreadInfo,
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("30s")),
        cv.Optional(CONF_TX_ERR_ABORT): sensor.sensor_schema(
            TxErrAbortOpenThreadInfo,
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("30s")),
        cv.Optional(CONF_RX_TOTAL): sensor.sensor_schema(
            RxTotalOpenThreadInfo,
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("30s")),
        cv.Optional(CONF_RX_ERR_FCS): sensor.sensor_schema(
            RxErrFcsOpenThreadInfo,
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("30s")),
        cv.Optional(CONF_ATTACH_ATTEMPTS): sensor.sensor_schema(
            AttachAttemptsOpenThreadInfo,
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("30s")),
        cv.Optional(CONF_PARENT_CHANGES): sensor.sensor_schema(
            ParentChangesOpenThreadInfo,
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("30s")),
        cv.Optional(CONF_PARTITION_ID_CHANGES): sensor.sensor_schema(
            PartitionIdChangesOpenThreadInfo,
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(cv.polling_component_schema("30s")),
    }
)


async def setup_conf(config: dict, key: str):
    if conf := config.get(key):
        var = await sensor.new_sensor(conf)
        await cg.register_component(var, conf)


async def to_code(config):
    await setup_conf(config, CONF_PARENT_AVERAGE_RSSI)
    await setup_conf(config, CONF_PARENT_LAST_RSSI)
    await setup_conf(config, CONF_PARENT_LINK_QUALITY_IN)
    await setup_conf(config, CONF_PARENT_LINK_QUALITY_OUT)
    await setup_conf(config, CONF_TX_TOTAL)
    await setup_conf(config, CONF_TX_RETRIES)
    await setup_conf(config, CONF_TX_ERR_CCA)
    await setup_conf(config, CONF_TX_ERR_ABORT)
    await setup_conf(config, CONF_RX_TOTAL)
    await setup_conf(config, CONF_RX_ERR_FCS)
    await setup_conf(config, CONF_ATTACH_ATTEMPTS)
    await setup_conf(config, CONF_PARENT_CHANGES)
    await setup_conf(config, CONF_PARTITION_ID_CHANGES)
