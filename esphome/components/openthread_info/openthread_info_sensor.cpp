#include "openthread_info_sensor.h"
#if defined(USE_OPENTHREAD) && defined(USE_SENSOR)
#include "esphome/core/log.h"

namespace esphome::openthread_info {

static const char *const TAG = "openthread_info";

void ParentAverageRssiOpenThreadInfo::dump_config() { LOG_SENSOR("", "Parent Average RSSI", this); }
void ParentLastRssiOpenThreadInfo::dump_config() { LOG_SENSOR("", "Parent Last RSSI", this); }
void ParentLinkQualityInOpenThreadInfo::dump_config() { LOG_SENSOR("", "Parent Link Quality In", this); }
void ParentLinkQualityOutOpenThreadInfo::dump_config() { LOG_SENSOR("", "Parent Link Quality Out", this); }
void TxTotalOpenThreadInfo::dump_config() { LOG_SENSOR("", "TX Total", this); }
void TxRetriesOpenThreadInfo::dump_config() { LOG_SENSOR("", "TX Retries", this); }
void TxErrCcaOpenThreadInfo::dump_config() { LOG_SENSOR("", "TX CCA Errors", this); }
void TxErrAbortOpenThreadInfo::dump_config() { LOG_SENSOR("", "TX Abort Errors", this); }
void RxTotalOpenThreadInfo::dump_config() { LOG_SENSOR("", "RX Total", this); }
void RxErrFcsOpenThreadInfo::dump_config() { LOG_SENSOR("", "RX FCS Errors", this); }
void AttachAttemptsOpenThreadInfo::dump_config() { LOG_SENSOR("", "Attach Attempts", this); }
void ParentChangesOpenThreadInfo::dump_config() { LOG_SENSOR("", "Parent Changes", this); }
void PartitionIdChangesOpenThreadInfo::dump_config() { LOG_SENSOR("", "Partition ID Changes", this); }

}  // namespace esphome::openthread_info
#endif
