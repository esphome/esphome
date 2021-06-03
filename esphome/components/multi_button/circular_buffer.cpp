#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include "circular_buffer.h"


// The definition of our circular buffer structure is hidden from the user
struct circular_buf_t {
    void **buffer;
    uint32_t head;
    uint32_t tail;
    uint32_t max; //of the buffer
    bool full;
};


// static functions

static void advance_pointer(cbuf_handle_t cbuf) {
    assert(cbuf);
    if (cbuf->full) {
        cbuf->tail = (cbuf->tail + 1) % cbuf->max;
    }
    cbuf->head = (cbuf->head + 1) % cbuf->max;
    // We mark full because we will advance tail on the next time around
    cbuf->full = (cbuf->head == cbuf->tail);
}


static void retreat_pointer(cbuf_handle_t cbuf) {
    assert(cbuf);
    cbuf->tail = (cbuf->tail + 1) % cbuf->max;
    cbuf->full = false;
}


// public functions


 int circular_buf_init(uint32_t size, cbuf_handle_t *handle) {
     if (!size || !handle) {
         return -1;
     }
     circular_buf_t *inst = (circular_buf_t *)malloc(sizeof(circular_buf_t));
    if (!inst) {
        return -1;
    }
    inst->buffer = (void **)malloc(size * sizeof(void *));
    if (!inst->buffer) {
        free(inst);
        return -1;
    }
    inst->max = size;
    inst->head = 0;
    inst->tail = 0;
    inst->full = false;
    *handle = inst;
    return 0;
}


 int circular_buf_free(cbuf_handle_t cbuf, int(*freeItem)(void **)) {
    if (!cbuf) {
        return 0;
    }
    int status = circular_buf_reset(cbuf, freeItem);
    if (status != 0) {
        return status;
    }
    free(cbuf->buffer);
    cbuf->buffer = 0;
    free(cbuf);
    cbuf = 0;
    return 0;
}


int circular_buf_reset(cbuf_handle_t cbuf, int(*freeItem)(void **)) {
    if (!cbuf) {
        return -1;
    }
    if (freeItem) {
        while (circular_buf_size(cbuf)) {
            void *item;
            int status = circular_buf_get(cbuf, &item);
            if (status != 0) {
                return status;
            }
            (*freeItem)(&item);
        }
    }
    cbuf->head = 0;
    cbuf->tail = 0;
    cbuf->full = false;
    return 0;
}


uint32_t circular_buf_capacity(cbuf_handle_t cbuf) {
    if (!cbuf) return 0;
    return cbuf->max;
}


uint32_t circular_buf_size(cbuf_handle_t cbuf) {
    if (!cbuf) return 0;
    if (cbuf->full) return cbuf->max;
    if (cbuf->head >= cbuf->tail) return cbuf->head - cbuf->tail;
    else return cbuf->max + cbuf->head - cbuf->tail;
}


uint8_t circular_buf_empty(cbuf_handle_t cbuf) {
    if (!cbuf) return 1;
    return (!cbuf->full && (cbuf->head == cbuf->tail));
}


uint8_t circular_buf_full(cbuf_handle_t cbuf) {
    if (!cbuf) return 0;
    return cbuf->full;
}


int circular_buf_put(cbuf_handle_t cbuf, void *data) {
    if (!cbuf) return -1;
    if (circular_buf_full(cbuf)) {
        return -1;
    }
    cbuf->buffer[cbuf->head] = data;
    advance_pointer(cbuf);
    return 0;
}


int circular_buf_peek(cbuf_handle_t cbuf, void** data) {
    if (!cbuf) return -1;
    if (circular_buf_empty(cbuf)) {
        return -1;
    }
    *data = cbuf->buffer[cbuf->tail];
    return 0;
}


int circular_buf_get(cbuf_handle_t cbuf, void **data) {
    if (!cbuf) return -1;
    if (circular_buf_empty(cbuf)) {
        return -1;
    }
    *data = cbuf->buffer[cbuf->tail];
    retreat_pointer(cbuf);
    return 0;
}
