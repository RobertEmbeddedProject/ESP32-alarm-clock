#pragma once
#include <stdint.h>
#include <stdbool.h>

void song_playback_task(void *arg);

void mp3_init(void);
void init_dfplayer_and_pam();
void mp3_play_now(void);
void mp3_stop(void);
void mp3_cmd(int8_t command, int16_t dat);
bool mp3_player_get_state(void);
void mp3_player_set_state(bool state);

//DF_Player Commands, used by rotary and in general..
#define CMD_PLAY_NEXT      0x01
#define CMD_PLAY_PREV      0x02
#define CMD_PLAY_W_INDEX   0x03
#define CMD_STOP           0x16  
#define CMD_SET_VOLUME     0x06
#define CMD_SET_PRESET     0x07  //Add this feature
/*  0 = Normal
    1 = Pop
    2 = Rock
    3 = Jazz
    4 = Classic
    5 = Blues     */
#define CMD_SLEEP_MODE     0x0A  
#define CMD_SEL_DEV        0x09  
#define DEV_TF             0x02  
