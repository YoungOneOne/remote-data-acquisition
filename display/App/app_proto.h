#ifndef __APP_PROTO_H
#define __APP_PROTO_H

#include <stdint.h>
#include "app_gfx.h"

#define PROTO_FRAME_HEAD0           SCOPE_HEAD0
#define PROTO_FRAME_HEAD1           SCOPE_HEAD1
#define PROTO_FRAME_TYPE_WAVE       SCOPE_WAVE_TYPE
#define PROTO_FRAME_POINTS          SCOPE_FRAME_POINTS
#define PROTO_BIN_FRAME_LEN         SCOPE_FRAME_BYTES

typedef struct {
    uint8_t  timebase;
    uint8_t  mode;
    uint32_t gen_freq;
    uint8_t  run;
    uint8_t  trig_src;
    uint8_t  trig_edge;
    uint8_t  trig_mode;
    uint8_t  trig_level;
    uint8_t  triggered;
    uint8_t  valid;
} proto_state_t;

typedef enum {
    PROTO_RES_NONE = 0,
    PROTO_RES_WAVE,
    PROTO_RES_TEXT
} proto_res_t;

void                 proto_init(void);
void                 proto_reset(void);
void                 proto_feed(const uint8_t *data, uint16_t len);

proto_res_t          proto_poll(void);

uint8_t              proto_get_wave(wave_window_t *w);

const char*          proto_get_text(void);

const proto_state_t* proto_get_remote_state(void);

typedef struct {
    uint16_t lost_frames;
    uint16_t bad_frames;
} proto_stats_t;
const proto_stats_t *proto_get_stats(void);

#endif
