#include "bigiot_platform.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <getopt.h>
#include <lwip/sockets.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "../../main/main.h"
#include "cJSON.h"
#include "esp_log.h"

typedef struct
{
    int sockfd;
    const char *name;
    const char *deviceID;
    const char *apikey;
    const char *host;
    uint16_t port;
} bigiot_struct_t;

#define ESP_FAIL -1
#define ESP_SUCESS 0
#define BLINK_GPIO 2



#define BIGIOT_WELCOME 1
#define BIGIOT_CHECK_IN_OK 2

#define BIGIOT_HEATRATE_TIMER_NAME "heatrate_timer"
#define BIGIOT_HEATRATE_TIMER_PERIOD 3000
#define BIGIOT_HEATRATE_TIMER_ID 2

bigiot_struct_t bigiot = {.name = "sw00", .apikey = BIGIOT_DEV_APIKEY, .deviceID = BIGIOT_DEV_ID, .host = BIGIOT_HOST, .port = BIGIOT_PORT, .sockfd = -1};
static char recvbuffer[4096];



/** 
 * @brief  esp_create_tcp_connect
 * @note   创建TCP连接
 * @param  *host: 主机IP，或者域名
 * @param  port: 端口
 * @retval Success return file description,else return -1
 */
static int esp_create_tcp_connect(const char *host, unsigned short port)
{
    int fd, flags, ret;
    struct sockaddr_in add;
    struct hostent *server;

    bzero(&add, sizeof(add));
    add.sin_family = AF_INET;
    add.sin_port = htons(port);

    server = gethostbyname(host);
    APP_ERROR_CHECK(!server, " Get host ip fail", goto ERR0);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    APP_ERROR_CHECK(fd < 0, "Create socket connect fail", goto ERR0);

    bcopy((char *)server->h_addr, (char *)&add.sin_addr.s_addr, server->h_length);

    ret = connect(fd, (struct sockaddr *)&add, sizeof(add));
    APP_ERROR_CHECK(ret < 0, "Connect host fail", goto ERR1);

    flags = fcntl(fd, F_GETFL, 0);
    APP_ERROR_CHECK(ESP_FAIL == flags, "Failed to get the socket file flags", goto ERR1);

    ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    APP_ERROR_CHECK(ret < 0, "Failed to set the socket to nonblock mode", goto ERR1);

    return fd;
ERR1:
    close(fd);
ERR0:
    return -1;
}

/** 
 * @brief  esp_send_packet
 * @note   发送数据包
 * @param  fd: 
 * @param  buf: 
 * @param  size: 
 * @retval 
 */
static int esp_send_packet(int fd, const char *buf, uint32_t size)
{
    struct msghdr msg;
    struct iovec iov;

    memset(&msg, 0, sizeof(msg));
    iov.iov_base = (char*)buf;
    iov.iov_len = size;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    return sendmsg(fd, &msg, 0);
}

/** 
 * @brief  BIGIOT_login_parse
 * @note   登陆数据解析
 * @param  *buf: 
 * @retval 
 */
static int BIGIOT_login_parse(const char *buf)
{
    printf("login: %s\n", buf);
    cJSON *root = cJSON_Parse(buf);
    APP_ERROR_CHECK(!root, "Checkinok parse malloc fail", goto ERR0);

    cJSON *item = cJSON_GetObjectItem(root, "M");
    APP_ERROR_CHECK(!item, "Checkinok parse malloc fail", goto ERR1);

    if (!strcmp(item->valuestring, "WELCOME TO BIGIOT"))
    {
        return BIGIOT_WELCOME;
    }
    else if (!strcmp(item->valuestring, "checkinok"))
    {
        return BIGIOT_CHECK_IN_OK;
    }
    cJSON_Delete(root);
    return 0;
ERR1:
    cJSON_Delete(root);
ERR0:
    return -1;
}

/** 
 * @brief  BIGIOT_login_begin
 * @note   登陆到贝壳
 * @param  *bigiot: 
 * @retval int
 * @Note
 */
static int BIGIOT_login_begin(bigiot_struct_t *bigiot)
{
    int ret;
    char buf[512];
    int timestamp = 0;

    bigiot->sockfd = esp_create_tcp_connect(bigiot->host, bigiot->port);
    if (bigiot->sockfd == -1)
    {
        ets_printf("Create tcp fail at host:%s  port:%u\n", bigiot->host, bigiot->port);
        goto ERR0;
    }

    cJSON *root = cJSON_CreateObject();
    APP_ERROR_CHECK(!root, "Create login packet fail", goto ERR1);

    cJSON_AddStringToObject(root, "M", "checkin");
    cJSON_AddStringToObject(root, "ID", bigiot->deviceID);
    cJSON_AddStringToObject(root, "K", bigiot->apikey);
    char *s = cJSON_PrintUnformatted(root);
    sprintf(buf, "%s\n", s);
    vPortFree(s);
    cJSON_Delete(root);

    ret = esp_send_packet(bigiot->sockfd, buf, strlen(buf));
    APP_ERROR_CHECK(ret == ESP_FAIL, "Send login packet fail", goto ERR1);

    bzero(buf, sizeof(buf));
    for (;;)
    {
        ret = read(bigiot->sockfd, buf, sizeof(buf));
        if (ret > 0)
        {
            if (BIGIOT_login_parse(buf) == BIGIOT_CHECK_IN_OK)
            {
                break;
            }
            bzero(buf, sizeof(buf));
        }
        if (++timestamp >= 5)
        {
            ets_printf("waiting login ok time out...\n");
            goto ERR1;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ets_printf("login success\n");
    return 0;
ERR1:
    close(bigiot->sockfd);
ERR0:
    return -1;
}

/** 
 * @brief  BIGIO_data_parse
 * @note   天猫指令解析
 * @param  *buf: 
 * @retval 
 */
static int BIGIO_data_parse( char *buf)
{
    cJSON *root = cJSON_Parse(buf);
    APP_ERROR_CHECK(!root, "malloc memory fail", goto ERR0);
    cJSON *m = cJSON_GetObjectItem(root, "M");
    APP_ERROR_CHECK(!m, "get obj fail", goto ERR1);
    if (strcmp(m->valuestring, "say"))
    {
        cJSON_Delete(root);
        return 0;
    }
    cJSON *com = cJSON_GetObjectItem(root, "C");
    if (!strcmp(com->valuestring, "play"))
    {
        gpio_set_level(BLINK_GPIO, 1);
        printf("play .......\n");
    }
    else if (!strcmp(com->valuestring, "stop"))
    {
        gpio_set_level(BLINK_GPIO, 0);
        printf("stop....\n");
    }
    cJSON_Delete(root);
    return 0;
ERR1:
    cJSON_Delete(root);
ERR0:
    return -1;
}


/** 
 * @brief  BIGIOT_heatrate_callback
 * @note   定时心率回掉函数
 * @param  *param: 
 * @retval None
 */
void BIGIOT_heatrate_callback(void *param)
{
    int ret;
    char *packet = "{\"M\",\"b\"}\n";

    ret = write(bigiot.sockfd, packet, strlen(packet));
    if (ret < 0)
    {
        printf("Send heat fail\n");
    }
}

/** 
 * @brief  BIGIO_update_data
 * @note   数据更新
 * @retval 
 */
int BIGIO_update_data(void)
{
    char buf[512];
    cJSON *root = cJSON_CreateObject();
    cJSON *v = cJSON_CreateObject();
    uint16_t ret = rand() % INT16_MAX;
    sprintf(buf, "%u", ret);
    cJSON_AddStringToObject(v, BIGIOT_DEV_DATA_POINT_ID, buf);
    cJSON_AddStringToObject(root, "M", "update");
    cJSON_AddStringToObject(root, "ID", BIGIOT_DEV_ID);
    cJSON_AddItemToObject(root, "V", v);
    char *s = cJSON_PrintUnformatted(root);
    sprintf(buf, "%s\n", s);
    printf("send : %s", buf);
    free(s);
    cJSON_Delete(root);
    return esp_send_packet(bigiot.sockfd, buf, strlen(buf));
}

/** 
 * @brief  BIGIO_update_task
 * @note   数据上传任务
 * @param  *param: 
 * @retval None
 */
void BIGIO_update_task(void *param)
{
    for (;;)
    {
        BIGIO_update_data();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

/** 
 * @brief  BIGIOT_Task
 * @note   
 * @param  *param: 
 * @retval None
 */
void BIGIOT_Task(void *param)
{
    int ret;
    gpio_pad_select_gpio(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    while (1)
    {
        if (BIGIOT_login_begin(&bigiot) == ESP_SUCESS)
        {
            break;
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }

    /* 创建定时心跳包处理回掉 */
    TimerHandle_t heatrate_timer = xTimerCreate(BIGIOT_HEATRATE_TIMER_NAME,
                                                BIGIOT_HEATRATE_TIMER_PERIOD,
                                                pdTRUE,
                                                (void*)BIGIOT_HEATRATE_TIMER_ID,
                                                BIGIOT_heatrate_callback);
    APP_ERROR_CHECK(!heatrate_timer, "Create heatrate timer fail", esp_restart());
    xTimerStart(heatrate_timer, 100);

    //创建消息上传任务
    xTaskCreate(BIGIO_update_task, "update_task", 4096, NULL, 3, NULL);

    for (;;)
    {
        ret = read(bigiot.sockfd, recvbuffer, sizeof(recvbuffer));
        if (ret > 0)
        {
            BIGIO_data_parse(recvbuffer);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
