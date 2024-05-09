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

  rb->write = osSemaphoreNew((RING_BUFFER_SLOTS -1), (RING_BUFFER_SLOTS -1), NULL);
  if(rb->write == NULL)
  {
    MUX_LOG("Fail to new rb->write Mux\r\n");
    return;
  }
  rb->read = osSemaphoreNew((RING_BUFFER_SLOTS -1), 0, NULL);
  if(rb->read == NULL)
  {
    MUX_LOG("Fail to new rb->read Mux\r\n");
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
  osSemaphoreAcquire(rb->write, 100);

  rb->head = ((rb->head + 1) % RING_BUFFER_SLOTS);
  /* reset new buffer data length*/
  rb->data_len[rb->head] = 0;
  osMutexRelease(rb->lock);

  return true;
}

ringbuff_status ringBuffer_reduce(RingBuffer *rb)
{
  ringbuff_status status = RINGBUFF_OK;

  osMutexAcquire(rb->lock, osWaitForever);
  do
  {
    if(ringBuffer_IsOne(rb))
    {
      status = RINGBUFF_FAILED;
      break;
    }
    rb->data_len[rb->tail] = 0;
    rb->tail = ((rb->tail + 1) % RING_BUFFER_SLOTS);
    osSemaphoreRelease(rb->write);
    printf("R");
  }
  while(false);

  osMutexRelease(rb->lock);

  return status;
}

ringbuff_status ringBuffer_write(RingBuffer *rb, const void* data, size_t len)
{
  ringbuff_status status = RINGBUFF_OK;


  osMutexAcquire(rb->lock, osWaitForever);
  do
  {
    if(ringBuffer_IsFull(rb))
    {
        printf("F");
        status = RINGBUFF_FAILED;
        break;
    }

    // expand slot if over increase level
    if(rb->data_len[rb->head] >= RING_BUFFER_INCREASE_LEVEL)
    {
      osStatus_t write_Sem_Acq;
      for(uint8_t i = 0 ; i < 5 ; i++)
      {
          write_Sem_Acq = osSemaphoreAcquire(rb->write, 100);
          if(write_Sem_Acq == osOK)
          {  break; }
          printf("E");
      }

      if(write_Sem_Acq != osOK)
      {
          status = RINGBUFF_FAILED;
          break;
      }

      /* ring buffer expand */
      rb->head = ((rb->head + 1) % RING_BUFFER_SLOTS);
      /* reset new buffer data length*/
      rb->data_len[rb->head] = 0;

      /* ready to read*/
      osSemaphoreRelease(rb->read);
    }
    memcpy(rb->buffer[rb->head] + rb->data_len[rb->head] , data, len );

    rb->data_len[rb->head] += len;
  }
  while(false);

  osMutexRelease(rb->lock);

  //MUX_LOG("[RB write ok] H: %u,T: %u,HL: %u,TL: %u\r\n", rb->head, rb->tail, rb->data_len[rb->head], rb->data_len[rb->tail]);
  return status;
}

#if 0
void ringBuffer_getTailBuff(RingBuffer *rb, size_t* len)
{
  // No implement, directly memcpy to sdcard
}
#endif
