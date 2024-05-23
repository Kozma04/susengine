#pragma once

#include <stdint.h>
#include <stdlib.h>

typedef enum FIFOModeEnum { FIFO_MODE_OVERRUN, FIFO_MODE_NO_OVERRUN } FIFOMode;

typedef struct FIFO {
    int32_t *buf;
    size_t size;
    uint32_t r_pos;
    uint32_t w_pos;
    FIFOMode mode;
} FIFO;

void fifo_init(FIFO *const fifo, int32_t *const buf, const size_t size,
               const FIFOMode mode);
int32_t fifo_read(FIFO *const fifo);
int32_t fifo_peek(FIFO *const fifo);
uint8_t fifo_write(FIFO *const fifo, const int32_t val);
size_t fifo_av_read(const FIFO *const fifo);
size_t fifo_av_write(const FIFO *fifo);