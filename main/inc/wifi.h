#pragma once
#include <stdint.h>
#include <stdbool.h>

void wifi_init(void);
bool wifi_is_connected(void);
bool wifi_time_is_synced(void);
void wifi_reconnect_task(void *arg);
