#ifndef __NVS_STORE_H
#define __NVS_STORE_H

#include <stdint.h>
#include "esp_wifi_types.h"



uint8_t load_wifi_info(wifi_config_t *config);
uint8_t save_wifi_info(wifi_config_t *config);


#endif /*__NVS_STORE_H*/