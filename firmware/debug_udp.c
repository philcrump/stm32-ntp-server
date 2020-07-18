#include "main.h"

#include <lwip/tcpip.h>

#include <string.h>

typedef struct {
  bool waiting;
  uint8_t rawFrame[64];
  mutex_t mutex;
  condition_variable_t condition;
} udp_tx_queue_t;

static udp_tx_queue_t udp_tx_queue =
{
  .waiting = false,
  .mutex = _MUTEX_DATA(udp_tx_queue.mutex),
  .condition = _CONDVAR_DATA(udp_tx_queue.condition)
};

static struct udp_pcb *debug_udp_tx_pcb_ptr = NULL;
static struct pbuf *debug_udp_pbuf_ptr;

static void _udp_tx(void *arg)
{
  (void)arg;

  if(debug_udp_tx_pcb_ptr == NULL)
  {
    debug_udp_tx_pcb_ptr = udp_new();
  }

  udp_sendto(debug_udp_tx_pcb_ptr, debug_udp_pbuf_ptr, IP_ADDR_BROADCAST, 11234);
}


THD_FUNCTION(debug_udp_thread, arg)
{
  (void)arg;

  chRegSetThreadName("debug_udp");

  while(true)
  {
    watchdog_feed(WATCHDOG_DOG_IPSRVS);
    chMtxLock(&udp_tx_queue.mutex);

    while(!udp_tx_queue.waiting)
    {
      watchdog_feed(WATCHDOG_DOG_IPSRVS);
      
      if(chCondWaitTimeout(&udp_tx_queue.condition, TIME_MS2I(100)) == MSG_TIMEOUT)
      {
        /* Re-acquire Mutex */
        chMtxLock(&udp_tx_queue.mutex);
      }
    }

    debug_pbuf_payload_ptr[4] = 0xFF;
    memcpy(&(debug_pbuf_payload_ptr[5]), udp_tx_queue.rawFrame, 64);

    udp_tx_queue.waiting = false;
    chMtxUnlock(&udp_tx_queue.mutex);

    if(app_ip_link_status() != APP_IP_LINK_STATUS_BOUND)
    {
      /* Discard packet */
      continue;
    }

    rtcGetTime(&RTCD1, &debug_datetime); 
    *(uint32_t *)(&debug_pbuf_payload_ptr[0]) = debug_datetime.millisecond;

    tcpip_callback(_udp_tx, NULL);

    watchdog_feed(WATCHDOG_DOG_IPSRVS);
    chThdSleepMilliseconds(10);
  }
};