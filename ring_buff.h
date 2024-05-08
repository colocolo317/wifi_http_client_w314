/*
 * ring_buff.h
 *
 *  Created on: 2024/4/24
 *
 */

#ifndef RING_BUFF_H_
#define RING_BUFF_H_

#include "cmsis_os2.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define RING_BUFFER_SLOTS 2
#define RING_BUFFER_LENGTH 2048
#define MAX_WRITE_SIZE 512
#define RING_BUFFER_INCREASE_LEVEL (RING_BUFFER_LENGTH - MAX_WRITE_SIZE)


typedef struct {
    uint8_t buffer[RING_BUFFER_SLOTS][RING_BUFFER_LENGTH];
    uint16_t data_len[RING_BUFFER_SLOTS];
    uint8_t head;  /* head for write into */
    uint8_t tail;  /* tail for read from */
    osMutexId_t lock;
    osMutexId_t space;
} RingBuffer;

extern RingBuffer* pRingBuff;

void ringBuffer_Init(RingBuffer *rb);
/* head = tail && data len = 0*/
bool ringBuffer_IsEmpty(RingBuffer *rb);
/* head = tail */
bool ringBuffer_IsOne(RingBuffer *rb);
bool ringBuffer_MaxSlots(RingBuffer *rb);
bool ringBuffer_IsFull(RingBuffer *rb);

/* Increase head*/
bool ringBuffer_expand(RingBuffer *rb);
/* Drop tail item */
bool ringBuffer_reduce(RingBuffer *rb);
bool ringBuffer_write(RingBuffer *rb, const void* data, size_t len);


#endif /* RING_BUFF_H_ */
