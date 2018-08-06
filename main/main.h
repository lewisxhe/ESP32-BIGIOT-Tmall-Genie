#ifndef __MAIN_H
#define __MAIN_H

#include "freertos/queue.h"
#include "freertos/event_groups.h"

#define HAVE_WIFI_INFO_BIT BIT0
#define CONNECTED_BIT BIT1
#define ESPTOUCH_DONE_BIT BIT2
#define BT_CONNECTED_BIT BIT3
#define BT_DISCOVER_BIT BIT4

#define APP_ERROR_CHECK(code, info, go)                                                                                                           \
    do                                                                                                                                            \
    {                                                                                                                                             \
        if (code)                                                                                                                                 \
        {                                                                                                                                         \
            ets_printf("err_code : %d file: \"%s\" line %d\nfunc: %s\nexpression: %s\ninfo:%s\n", code, __FILE__, __LINE__, __ASSERT_FUNC, info); \
            go;                                                                                                                                   \
        }                                                                                                                                         \
    } while (0)

typedef struct
{
    QueueHandle_t q_handle;         /*消息队列*/
    EventGroupHandle_t event_group; /*事件任务标记*/
} task_ctx_t;

#endif