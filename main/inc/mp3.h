#pragma once
#include <stdint.h>
#include <stdbool.h>

void song_playback_task(void *arg);
void mp3_task(void *arg);

void mp3_init(void);
void mp3_cmd(int8_t command, int16_t dat);
bool mp3_track_finished(uint16_t *track_out);
bool mp3_is_music_playing(void);
void mp3_set_music_playing(bool state);

//TX to DF_Player Commands
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

//RX FROM DFPLAYER
#define DFPLAYER_FRAME_SIZE      10
//responds with track number, which also indicates that the track has finished:
#define DFPLAYER_TF_FINISHED_CMD 0x3D   


//For Command Queue, Requests from other Tasks
typedef enum {
    MP3_REQ_PLAY_INDEX,
    MP3_REQ_STOP,
    MP3_REQ_SET_VOLUME,
    MP3_REQ_START_ALARM,
    MP3_REQ_SNOOZE_STOP
} mp3_req_type_t;

typedef struct {
    mp3_req_type_t type;
    uint16_t value;
} mp3_request_t;

bool mp3_request(mp3_req_type_t type, uint16_t value);
void mp3_task(void *args);
