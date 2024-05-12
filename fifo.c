#include "fifo.h"


void fifo_init(FIFO* const fifo, int32_t* const buf, const size_t size, const FIFOMode mode) {
    fifo->buf = buf;
    fifo->mode = mode;
    fifo->r_pos = size - 1;
    fifo->w_pos = 0;
    fifo->size = size;
}

int32_t fifo_read(FIFO* const fifo) {
    // pre-increment read pointer
    if((fifo->w_pos == 0 && fifo->r_pos == fifo->size) ||
       (fifo->w_pos > 0 && fifo->r_pos + 1 == fifo->w_pos)) return 0;
    fifo->r_pos++;
    if(fifo->r_pos == fifo->size)
        fifo->r_pos = 0;
    return fifo->buf[fifo->r_pos];
}

int32_t fifo_peek(FIFO* const fifo) {
    uint32_t r_pos_tmp = fifo->r_pos + 1;
    if(r_pos_tmp == fifo->size) r_pos_tmp = 0;
    return fifo->buf[r_pos_tmp];
}

uint8_t fifo_write(FIFO* const fifo, const int32_t val) {
    // post-increment write pointer
    switch(fifo->mode) {
        case FIFO_MODE_NO_OVERRUN:
            if(fifo->r_pos == fifo->w_pos)
                return 0;
            fifo->buf[fifo->w_pos++] = val;
            if(fifo->w_pos == fifo->size)
                fifo->w_pos = 0;
            break;
        case FIFO_MODE_OVERRUN:
            if(fifo->r_pos == fifo->w_pos) {
                fifo->r_pos++;
                if(fifo->r_pos == fifo->size)
                    fifo->r_pos = 0;
            }
            fifo->buf[fifo->w_pos++] = val;
            if(fifo->w_pos == fifo->size)
                fifo->w_pos = 0;
            break;
    }
    return 1;
}

size_t fifo_av_read(const FIFO* const fifo) {
    if(fifo->w_pos > fifo->r_pos)
        return fifo->w_pos - fifo->r_pos - 1;
    return fifo->size - fifo->r_pos + fifo->w_pos - 1;
}

size_t fifo_av_write(const FIFO *fifo) {
    return fifo->size - fifo_av_read(fifo) - 1;
}