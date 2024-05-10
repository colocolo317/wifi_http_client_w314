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

#define RINGBUFF_OK       0
#define RINGBUFF_FAILED   1

#define RINGBUFF_ACQ_WRITE_TIME  2
#define RINGBUFF_ACQ_WRITE_RETRY 10

#define MAX_WRITE_SIZE 512
#define RING_BUFFER_SLOTS 3
#define RING_BUFFER_LENGTH (10240 + MAX_WRITE_SIZE)

#define RING_BUFFER_INCREASE_LEVEL (RING_BUFFER_LENGTH - MAX_WRITE_SIZE)

typedef uint8_t ringbuff_status;
/*// alternative struct for easily return.
typedef struct {
  uint8_t buffer[RING_BUFFER_LENGTH];
  uint16_t data_len;
} ringBuffUnit;
*/

typedef struct {
    uint8_t buffer[RING_BUFFER_SLOTS][RING_BUFFER_LENGTH];
    uint16_t data_len[RING_BUFFER_SLOTS];
    //ringBuffUnit ringBufU[RING_BUFFER_SLOTS];
    uint8_t head;  /* head for write into */
    uint8_t tail;  /* tail for read from */
    osMutexId_t lock;
    osSemaphoreId_t write;
    osSemaphoreId_t read;
} RingBuffer;

extern RingBuffer* pRingBuff;

void ringBuffer_Init(RingBuffer *rb);
/* head = tail && data len = 0*/
bool ringBuffer_IsEmpty(RingBuffer *rb);
/* head = tail */
bool ringBuffer_IsOne(RingBuffer *rb);
bool ringBuffer_MaxSlots(RingBuffer *rb);
bool ringBuffer_IsFull(RingBuffer *rb);

#if AMPAK_VERIFY_THIS_SECTION
/* Increase head*/
bool ringBuffer_expand(RingBuffer *rb);
/* Drop tail item */
ringbuff_status ringBuffer_reduce(RingBuffer *rb);
#endif

/*
 * TODO implement acquire read function
 * For acquire read semaphore and retry
 */
ringbuff_status ringBuffer_acquire_read(RingBuffer *rb);
ringbuff_status ringBuffer_check_ready_to_write(RingBuffer *rb);
ringbuff_status ringBuffer_write(RingBuffer *rb, const void* data, size_t len);
ringbuff_status ringBuffer_readTailSlot(RingBuffer *rb, void* receive_buff, size_t *len);

#endif /* RING_BUFF_H_ */
