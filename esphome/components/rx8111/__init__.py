from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, time

CODEOWNERS = ["@techdregs"]
DOMAIN = "rx8111"

CONF_RX8111_ID = "rx8111_id"

rx8111_ns = cg.esphome_ns.namespace("rx8111")

RX8111Component = rx8111_ns.class_("RX8111Component", time.RealTimeClock, i2c.I2CDevice)

WriteAction = rx8111_ns.class_("WriteAction", automation.Action)
ReadAction = rx8111_ns.class_("ReadAction", automation.Action)
ClearAlarmFlagAction = rx8111_ns.class_("ClearAlarmFlagAction", automation.Action)
ClearTimerFlagAction = rx8111_ns.class_("ClearTimerFlagAction", automation.Action)
ClearEventFlagAction = rx8111_ns.class_("ClearEventFlagAction", automation.Action)
SetAlarmAction = rx8111_ns.class_("SetAlarmAction", automation.Action)
UnsetAlarmAction = rx8111_ns.class_("UnsetAlarmAction", automation.Action)
ScheduleAlarmInAction = rx8111_ns.class_("ScheduleAlarmInAction", automation.Action)

AlarmTrigger = rx8111_ns.class_("AlarmTrigger", automation.Trigger.template())
TimerTrigger = rx8111_ns.class_("TimerTrigger", automation.Trigger.template())
EventTrigger = rx8111_ns.class_("EventTrigger", automation.Trigger.template())

FoutFrequency = rx8111_ns.enum("FoutFrequency")
TimerClock = rx8111_ns.enum("TimerClock")
EventLevel = rx8111_ns.enum("EventLevel")
TimestampRecordMode = rx8111_ns.enum("TimestampRecordMode")
EvinPull = rx8111_ns.enum("EvinPull")
