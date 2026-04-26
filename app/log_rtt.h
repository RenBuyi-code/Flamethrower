#ifndef APP_LOG_RTT_H
#define APP_LOG_RTT_H

#include "../middleware/RTT/RTT/SEGGER_RTT.h"

#define APP_LOGI(fmt, ...) SEGGER_RTT_printf(0, "[I] " fmt "\r\n", ##__VA_ARGS__)
#define APP_LOGW(fmt, ...) SEGGER_RTT_printf(0, "[W] " fmt "\r\n", ##__VA_ARGS__)
#define APP_LOGE(fmt, ...) SEGGER_RTT_printf(0, "[E] " fmt "\r\n", ##__VA_ARGS__)

#endif
