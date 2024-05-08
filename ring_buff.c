/*
 * ring_buff.c
 *
 *  Created on: 2024/4/24
 *
 */

#include "ring_buff.h"
#include "mux_debug.h"
#include <string.h>

void ringBuffer_Init(RingBuffer *rb) {
  rb->head = 0;
  rb->tail = 0;
  for(int i=0;i < RING_BUFFER_SLOTS; i++){
      rb->data_len[i] = 0;
  }
  rb->lock = osSemaphoreNew(1, 1, NULL);
  if(rb->lock == NULL)
  {
    MUX_LOG("Fail to new rb->lock sem\r\n");
    return;
  }

  rb->space = osSemaphoreNew(1, 0, NULL);
  if(rb->space == NULL)
  {
    MUX_LOG("Fail to new rb->space sem\r\n");
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
  return (((rb->head + 1) % RING_BUFFER_SLOTS == rb->tail) && (rb->data_len[rb->head] >= RING_BUFFER_INCREASE_LEVEL));
}

/* Increase head*/
bool ringBuffer_expand(RingBuffer *rb)
{
  if(ringBuffer_MaxSlots(rb))
  { return false; }

  osSemaphoreAcquire(rb->lock, osWaitForever);
  rb->head = ((rb->head + 1) % RING_BUFFER_SLOTS);
  /* reset new buffer data length*/
  rb->data_len[rb->head] = 0;
  osSemaphoreRelease(rb->lock);

  return true;
}

bool ringBuffer_reduce(RingBuffer *rb)
{
  if(ringBuffer_IsOne(rb))
  { return false; }

  /* reset leave buffer data length*/
  bool past_full = ringBuffer_IsFull(rb);

  osSemaphoreAcquire(rb->lock, osWaitForever);
  rb->data_len[rb->tail] = 0;
  rb->tail = ((rb->tail + 1) % RING_BUFFER_SLOTS);
  osSemaphoreRelease(rb->lock);

  if(past_full)
  {
    osSemaphoreRelease(rb->space);
  }
  //MUX_LOG("Ring buffer Reduce\r\n");
  return true;
}

bool ringBuffer_write(RingBuffer *rb, const void* data, size_t len)
{
  if(ringBuffer_IsFull(rb))
  { return false; }

  osSemaphoreAcquire(rb->lock, osWaitForever);
  memcpy(rb->buffer[rb->head] + rb->data_len[rb->head] , data, len );

  rb->data_len[rb->head] += len;
  osSemaphoreRelease(rb->lock);

  if(rb->data_len[rb->head] >= RING_BUFFER_INCREASE_LEVEL)
  {
      if(!ringBuffer_expand(rb))
      {
        /*Is full*/
        MUX_LOG("Ring buffer is Full\r\n");
        MUX_LOG("space count: %lu\r\n",osSemaphoreGetCount(rb->space));
        if( osSemaphoreGetCount(rb->space) > 0 )
        {

            osSemaphoreAcquire(rb->space, 0);
        }
      }
  }

  return true;
}

#if 0
void ringBuffer_getTailBuff(RingBuffer *rb, size_t* len)
{
  // No implement, directly memcpy to sdcard
}
#endif
