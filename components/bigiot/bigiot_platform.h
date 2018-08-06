#ifndef __BIGIOT_H
#define __BIGIOT_H


#define BIGIOT_HOST "www.bigiot.net"
#define BIGIOT_PORT 8282
//鉴权
#define BIGIOT_DEV_APIKEY "xxxxx"
//设备id
#define BIGIOT_DEV_ID "xxxx"
//数据接口id
#define BIGIOT_DEV_DATA_POINT_ID "xxxx"

void BIGIOT_Task(void *param);

#endif /*__BIGIOT_H*/