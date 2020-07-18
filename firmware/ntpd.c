#include "main.h"

#include "lwip/api.h"
#include "lwip/netif.h"

#include <string.h>

ntpd_status_t ntpd_status = {
  .status = NTPD_UNSYNC,
  .requests_count = 0,
  .stratum = 16
};

typedef struct
{
  uint8_t li_vn_mode;

  uint8_t stratum;
  uint8_t poll;
  uint8_t precision;

  uint32_t rootDelay;

  uint16_t rootDispersion_s;
  uint16_t rootDispersion_f;

  uint32_t refId;

  uint32_t refTm_s;
  uint32_t refTm_f;

  uint32_t origTm_s; 
  uint32_t origTm_f;

  uint32_t rxTm_s;
  uint32_t rxTm_f;

  uint32_t txTm_s;
  uint32_t txTm_f;

} ntp_packet_t;

/* From GNSS PPS */
extern uint32_t time_ref_s;
extern uint32_t time_ref_f;

THD_FUNCTION(ntpd_thread, arg)
{
  (void)arg;
  struct netconn *conn;
  struct netbuf *buf;
  err_t err;
  uint16_t buf_data_len;

  ntp_packet_t *ntp_packet_ptr;
  RTCDateTime ntpd_datetime;
  struct tm tm_;
  uint32_t tm_ms_;

  chRegSetThreadName("ntpd");

  /* Create a new UDP connection handle */
  conn = netconn_new(NETCONN_UDP);
  if(conn == NULL)
  {
    chThdExit(MSG_RESET);
  }

  netconn_bind(conn, IP_ADDR_ANY, 123);

  while (true)
  {
    err = netconn_recv(conn, &buf);
    if (err == ERR_OK)
    {
      palSetLine(LINE_LED2);

      netbuf_data(buf, (void **)&ntp_packet_ptr, &buf_data_len);

      if(buf_data_len < 48 || buf_data_len > 2048)
      {
        netbuf_delete(buf);
        continue;
      }

      ntp_packet_ptr->li_vn_mode = (0 << 6) | (4 << 3) | (4); // Leap Warning: None, Version: NTPv4, Mode: 4 - Server

      ntp_packet_ptr->stratum = ntpd_status.stratum;
      ntp_packet_ptr->poll = 5; // 32s
      ntp_packet_ptr->precision = -10; // ~1ms

      ntp_packet_ptr->rootDelay = 0; // Delay from GPS clock is ~zero
      ntp_packet_ptr->rootDispersion_s = 0;
      ntp_packet_ptr->rootDispersion_f = htonl(NTP_MS_TO_FS_U16 * 1.0); // 1ms
      ntp_packet_ptr->refId = ('G') | ('P' << 8) | ('S' << 16) | ('\0' << 24);

      /* Move client's transmit timestamp into origin fields */
      ntp_packet_ptr->origTm_s = ntp_packet_ptr->txTm_s;
      ntp_packet_ptr->origTm_f = ntp_packet_ptr->txTm_f;

      ntp_packet_ptr->refTm_s = time_ref_s;
      ntp_packet_ptr->refTm_f = time_ref_f;

      rtcGetTime(&RTCD1, &ntpd_datetime);
      rtcConvertDateTimeToStructTm(&ntpd_datetime, &tm_, &tm_ms_);

      ntp_packet_ptr->rxTm_s = htonl(mktime(&tm_) - DIFF_SEC_1970_2036);
      ntp_packet_ptr->rxTm_f = htonl((NTP_MS_TO_FS_U32 * tm_ms_));

      /* Copy into transmit timestamp fields */
      ntp_packet_ptr->txTm_s = ntp_packet_ptr->rxTm_s;
      ntp_packet_ptr->txTm_f = ntp_packet_ptr->rxTm_f;

      netconn_send(conn, buf);
      netbuf_delete(buf);

      ntpd_status.requests_count++;

      palClearLine(LINE_LED2);
    }
  }
};