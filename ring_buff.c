/*
 * ring_buff.c
 *
 *  Created on: 2024/4/24
 *
 */

#include "ring_buff.h"
#include "mux_debug.h"
#include <string.h>

RingBuffer ringBuff;
RingBuffer* pRingBuff = &ringBuff;

void ringBuffer_Init(RingBuffer *rb) {
  rb->head = 0;
  rb->tail = 0;
  for(int i=0;i < RING_BUFFER_SLOTS; i++){
      rb->data_len[i] = 0;
  }
  rb->lock = osMutexNew(NULL);
  if(rb->lock == NULL)
  {
    MUX_LOG("Fail to new rb->lock Mux\r\n");
    return;
  }

  rb->space = osMutexNew(NULL);
  if(rb->space == NULL)
  {
    MUX_LOG("Fail to new rb->space Mux\r\n");
    return;
  }

  MUX_LOG("Ring buffer init ok\r\n");
}

bool ringBuffer_IsEmpty(RingBuffer *rb) {
  return ((rb->head == rb->tail) && rb->data_len[rb->head]==0);
}

bool ringBuffer_IsOne(RingBuffer *rb) {
  return (rb->head == rb->tail);
}

bool ringBuffer_MaxSlots(RingBuffer *rb) {
  return ((rb->head + 1) % RING_BUFFER_SLOTS == rb->tail);
}

bool ringBuffer_IsFull(RingBuffer *rb) {
  return ((((rb->head + 1) % RING_BUFFER_SLOTS) == rb->tail) && (rb->data_len[rb->head] >= RING_BUFFER_INCREASE_LEVEL));
}

/* Increase head*/
bool ringBuffer_expand(RingBuffer *rb)
{
  if(ringBuffer_MaxSlots(rb))
  { return false; }

  osMutexAcquire(rb->lock, osWaitForever);
  rb->head = ((rb->head + 1) % RING_BUFFER_SLOTS);
  /* reset new buffer data length*/
  rb->data_len[rb->head] = 0;
  osMutexRelease(rb->lock);

  return true;
}

bool ringBuffer_reduce(RingBuffer *rb)
{
  if(ringBuffer_IsOne(rb))
  { return false; }

  osMutexAcquire(rb->lock, osWaitForever);
  rb->data_len[rb->tail] = 0;
  rb->tail = ((rb->tail + 1) % RING_BUFFER_SLOTS);
  //osMutexRelease(rb->space);
  osMutexRelease(rb->lock);

  MUX_LOG("reduce_rb\r\n");
  return true;
}

bool ringBuffer_write(RingBuffer *rb, const void* data, size_t len)
{
  if(ringBuffer_IsFull(rb))
  {
      //if(osMutexAcquire(rb->space, 300) != osOK)
      //{
      //  MUX_LOG("[ring buff] failed to lock mutex space\r\n");
      //}
      return false;
  }

  osMutexAcquire(rb->lock, osWaitForever);
  if(rb->data_len[rb->head] >= RING_BUFFER_INCREASE_LEVEL)
  {
    /* ring buffer expand */
    rb->head = ((rb->head + 1) % RING_BUFFER_SLOTS);
    /* reset new buffer data length*/
    rb->data_len[rb->head] = 0;
  }
  memcpy(rb->buffer[rb->head] + rb->data_len[rb->head] , data, len );

  rb->data_len[rb->head] += len;
  osMutexRelease(rb->lock);

  //MUX_LOG("[RB write ok] H: %u,T: %u,HL: %u,TL: %u\r\n", rb->head, rb->tail, rb->data_len[rb->head], rb->data_len[rb->tail]);
  return true;
}

#if 0
void ringBuffer_getTailBuff(RingBuffer *rb, size_t* len)
{
  // No implement, directly memcpy to sdcard
}
#endif
