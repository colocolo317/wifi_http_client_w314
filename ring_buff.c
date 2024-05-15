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
  rb->write = osSemaphoreNew(1, 1, NULL);
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

ringbuff_status ringBuffer_check_ready_to_write(RingBuffer *rb)
{
  ringbuff_status status = RINGBUFF_OK;
  osStatus_t write_Sem_Acq;
  if(ringBuffer_IsFull(rb))
  {
    do
    {
      for(uint8_t i = 0 ; i < RINGBUFF_ACQ_WRITE_RETRY ; i++)
      {
        write_Sem_Acq = osSemaphoreAcquire(rb->write, RINGBUFF_ACQ_WRITE_DELAY);
        if(write_Sem_Acq == osOK)
        {
          ringBuffer_debug("P");
          break;
        }
        ringBuffer_debug("p");
      }

      if(write_Sem_Acq != osOK)
      {
        ringBuffer_debug("N");
        status = RINGBUFF_FAILED;
        break;
      }
    }
    while(false);
  }
#if AMPAK_RINGBUFF_DEBUG_LOG
  else
  {
    ringBuffer_debug(">");
  }
#endif

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
        ringBuffer_debug("F");
        status = RINGBUFF_FAILED;
        break;
    }

    // expand slot if over increase level
    if(rb->data_len[rb->head] >= RING_BUFFER_INCREASE_LEVEL)
    {
      /* ring buffer expand */
      rb->head = ((rb->head + 1) % RING_BUFFER_SLOTS);
      /* reset new buffer data length*/
      rb->data_len[rb->head] = 0;

      /* ready to read*/
      osSemaphoreRelease(rb->read);
    }

    /* copy content */
    memcpy(rb->buffer[rb->head] + rb->data_len[rb->head] , data, len );
    rb->data_len[rb->head] += len;

    /* take write sem while full after write */
    if(ringBuffer_IsFull(rb))
    {
#if !AMPAK_RINGBUFF_DEBUG_LOG
      osSemaphoreAcquire(rb->write, 0);
#else
      if(osSemaphoreAcquire(rb->write, 0) == osOK)
      { ringBuffer_debug("A"); }
      else
      { ringBuffer_debug("a"); }
#endif
    }
  }
  while(false);

  osMutexRelease(rb->lock);

  //MUX_LOG("[RB write ok] H: %u,T: %u,HL: %u,TL: %u\r\n", rb->head, rb->tail, rb->data_len[rb->head], rb->data_len[rb->tail]);
  return status;
}

ringbuff_status ringBuffer_acquire_read(RingBuffer *rb)
{
  ringbuff_status status = RINGBUFF_OK;
  osStatus_t read_acq = osOK;

  for(uint8_t i = 0 ; i < RINGBUFF_ACQ_READ_RETRY ; i++)
  {
    read_acq = osSemaphoreAcquire(rb->read, RINGBUFF_ACQ_READ_DELAY);
    if(read_acq == osOK)
    { break; }
  }

  if(read_acq != osOK)
  { return RINGBUFF_FAILED; }

  return status;
}

ringbuff_status ringBuffer_readTailSlot(RingBuffer *rb, void* receive_buff, size_t *len)
{
  ringbuff_status status = RINGBUFF_OK;
  *len = 0;
  osMutexAcquire(rb->lock, osWaitForever);
  do
  {
      if(ringBuffer_IsOne(rb) == true)
      {
        status = RINGBUFF_FAILED;
        break;
      }
      /* copy content and data len */
      memcpy(receive_buff, pRingBuff->buffer[pRingBuff->tail], pRingBuff->data_len[pRingBuff->tail]);
      *len = pRingBuff->data_len[pRingBuff->tail];

      /* reduce ring buffer */
      rb->data_len[rb->tail] = 0;
      rb->tail = ((rb->tail + 1) % RING_BUFFER_SLOTS);

      osSemaphoreRelease(rb->write);
      ringBuffer_debug("R");
  }
  while(false);

  osMutexRelease(rb->lock);

  return status;
}

ringbuff_status ringBuffer_readLastData(RingBuffer *rb, void* receive_buff, size_t *len)
{
  ringbuff_status status = RINGBUFF_OK;
  *len = 0;
  osMutexAcquire(rb->lock, osWaitForever);
  do
  {
      if(ringBuffer_IsOne(rb) != true)
      {
        status = RINGBUFF_FAILED;
        break;
      }
      /* head == tail */
      /* copy content and data len */
      memcpy(receive_buff, pRingBuff->buffer[pRingBuff->tail], pRingBuff->data_len[pRingBuff->tail]);
      *len = pRingBuff->data_len[pRingBuff->tail];

      /* reduce ring buffer */
      rb->data_len[rb->tail] = 0;

      osSemaphoreRelease(rb->write);
      ringBuffer_debug("L");
  }
  while(false);

  osMutexRelease(rb->lock);

  return status;
}

#if AMPAK_VERIFY_THIS_SECTION
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
    ringBuffer_debug("R");
  }
  while(false);

  osMutexRelease(rb->lock);

  return status;
}
#endif
