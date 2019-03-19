#ifndef __SMYLES_MOS_LIB_MQTT_QUEUE_H
#define __SMYLES_MOS_LIB_MQTT_QUEUE_H

#include <stdbool.h>

#include "common/mg_str.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

bool mgos_mqtt_queue_gcp_send_event_subf(const char *subfolder, const char *json_fmt, ...);

#ifdef __cplusplus
}
#endif

#endif