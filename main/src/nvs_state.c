#include "nvs_state.h"
#include "rotary.h"
#include "master_alarm.h"
#include "nvs_flash.h"
#include "nvs.h"

#define NVS_NAMESPACE "alarm_clock"

void nvs_state_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_i32(h, "alarm_hour", alarm_hour_get_index());
    nvs_set_i32(h, "alarm_min",  alarm_minute_get_index());
    nvs_set_u8 (h, "armed",      alarm_get_state() == ALARM_ARMED ? 1 : 0);
    nvs_set_i32(h, "song_idx",   songs_get_index());
    nvs_commit(h);
    nvs_close(h);
}

void nvs_state_load(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return;

    int32_t val;
    uint8_t armed;

    if (nvs_get_i32(h, "alarm_hour", &val) == ESP_OK) alarm_hour_set_index(val);
    if (nvs_get_i32(h, "alarm_min",  &val) == ESP_OK) alarm_minute_set_index(val);
    if (nvs_get_i32(h, "song_idx",   &val) == ESP_OK) songs_set_index(val);
    if (nvs_get_u8 (h, "armed", &armed)    == ESP_OK && armed) alarm_restore_armed();

    nvs_close(h);
}
