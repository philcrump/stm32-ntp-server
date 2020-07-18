#include "main.h"

#include <lwip/tcpip.h>
#include <lwip/udp.h>

#include <string.h>

gnss_status_t gnss_status = { 0 };

int32_t gnss_aid_position_latitude = GNSS_AID_POS_LATITUDE;
int32_t gnss_aid_position_longitude = GNSS_AID_POS_LONGITUDE;
int32_t gnss_aid_position_altitude = GNSS_AID_POS_ALTITUDE;
int32_t gnss_aid_position_stddev = GNSS_AID_POS_STDDEV;

void gnss_parse(uint8_t *buffer);

static const SerialConfig serial_gnss_config = {
  .speed = 9600,
  .cr2 = USART_CR2_CPOL
};

static uint16_t ubx_crc(const uint8_t *input, uint16_t len)
{
  uint8_t a = 0;
  uint8_t b = 0;
  uint32_t i;
  for(i = 0; i < len; i++)
  {
    a = a + input[i];
    b = b + a;
  }
  return (a << 8) | b;
}

static bool ubx_crc_verify(const uint8_t *buffer, int32_t buffer_size)
{
  uint16_t ck = ubx_crc(&buffer[2], (buffer_size-4));

  return (((ck >> 8) & 0xFF) == buffer[buffer_size-2]) && ((ck & 0xFF) == buffer[buffer_size-1]);
}

static void uart_send_blocking_len_ubx(const uint8_t *_buff, uint16_t len)
{
    uint16_t checksum;
    uint8_t checksum_u8[2];
    checksum = ubx_crc(&_buff[2], (len-2));
    checksum_u8[0] = (checksum >> 8) & 0xFF;
    checksum_u8[1] = checksum & 0xFF;

    sdWrite(&SD3, _buff, len);
    sdWrite(&SD3, checksum_u8, 2);
}

const uint8_t enable_galileo[] = { 0xb5, 0x62,
  0x06, 0x3e,
  0x3c, 0x00, // Length: 60 bytes
  0x00, 0x00, 0xff, 0x07,
  //  GPS   min   max   res   x1    x2    x3,   x4
      0x00, 0x0A, 0x10, 0x00, 0x01, 0x00, 0x01, 0x01,
  //  SBAS  min   max   res   x1    x2    x3    x4
      0x01, 0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x01,
  //  GAL   min   max   res   x1    x2    x3,   x4
      0x02, 0x04, 0x08, 0x00, 0x01, 0x00, 0x01, 0x01,
  //  BEI   min   max   res   x1    x2    x3,   x4
      0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
  //  IMES  min   max   res   x1    x2    x3,   x4
      0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03,
  //  QZSS  min   max   res   x1    x2    x3,   x4
      0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x05,
  //  GLO   min   max   res   x1    x2    x3,   x4
      0x06, 0x0A, 0x10, 0x00, 0x01, 0x00, 0x01, 0x01,
};

const uint8_t disable_nmea_gpgga[] = {0xb5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t disable_nmea_gpgll[] = {0xb5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t disable_nmea_gpgsa[] = {0xb5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t disable_nmea_gpgsv[] = {0xb5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t disable_nmea_gprmc[] = {0xb5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t disable_nmea_gpvtg[] = {0xb5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

const uint8_t enable_navpvt[] =      {0xb5, 0x62, 0x06, 0x01, 0x08, 0x00, 0x01, 0x07, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};
const uint8_t enable_navsat[] =      {0xb5, 0x62, 0x06, 0x01, 0x08, 0x00, 0x01, 0x35, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};

uint8_t gnss_aid_position_msg[] =
{
  0x13, 0x40, // UBX-MGA-INI
  0x14, 0x00, // Length
  0x01, // Type: UBX-MGA-INI-POS_LLH
  0x00, // Version: 0
  0x00, 0x00, // Reserved
  0x00, 0x00, 0x00, 0x00, // Latitude 1e-7
  0x00, 0x00, 0x00, 0x00, // Longitude 1e-7
  0x00, 0x00, 0x00, 0x00, // Altitude cm
  0x00, 0x00, 0x00, 0x00  // Std Dev cm
};

static void gnss_configure(void)
{
  /* Enable Galileo */
  uart_send_blocking_len_ubx(enable_galileo, sizeof(enable_galileo));

  /* Disable NMEA Outputs */
  uart_send_blocking_len_ubx(disable_nmea_gpgga, sizeof(disable_nmea_gpgga));
  uart_send_blocking_len_ubx(disable_nmea_gpgll, sizeof(disable_nmea_gpgll));
  uart_send_blocking_len_ubx(disable_nmea_gpgsa, sizeof(disable_nmea_gpgsa));
  uart_send_blocking_len_ubx(disable_nmea_gpgsv, sizeof(disable_nmea_gpgsv));
  uart_send_blocking_len_ubx(disable_nmea_gprmc, sizeof(disable_nmea_gprmc));
  uart_send_blocking_len_ubx(disable_nmea_gpvtg, sizeof(disable_nmea_gpvtg));

  /* Enable UBX Outputs */
  uart_send_blocking_len_ubx(enable_navpvt, sizeof(enable_navpvt));
  uart_send_blocking_len_ubx(enable_navsat, sizeof(enable_navsat));

  #ifdef GNSS_AID_POSITION
    memcpy(&gnss_aid_position_msg[8], &gnss_aid_position_latitude, sizeof(int32_t));
    memcpy(&gnss_aid_position_msg[12], &gnss_aid_position_longitude, sizeof(int32_t));
    memcpy(&gnss_aid_position_msg[16], &gnss_aid_position_altitude, sizeof(int32_t));
    memcpy(&gnss_aid_position_msg[20], &gnss_aid_position_stddev, sizeof(int32_t));

    uart_send_blocking_len_ubx(gnss_aid_position_msg, sizeof(gnss_aid_position_msg));
  #endif
}

static uint8_t buffer[1024];

static const uint8_t msg_header[2] = { 0xb5, 0x62 };
static uint32_t response_index = 0;
static uint32_t response_length = 0;
static void gnss_ubx_rx(uint8_t response_byte)
{
  if(response_index < 2 && response_byte == msg_header[response_index])
  {
    /* Header */
    buffer[response_index] = response_byte;
    response_index++;
  }
  else if(response_index >= 2 && response_index < 5)
  {
    /* Message Class, ID, first byte of length */
    buffer[response_index] = response_byte;
    response_index++;
  }
  else if(response_index == 5)
  {
    /* Final Length byte */
    buffer[response_index] = response_byte;
    response_length = buffer[5] << 8 | buffer[4];
    response_index++;
  }
  else if(response_index > 5 && response_index < (6+2+response_length-1))
  {
    /* Data payload and first byte of checksum */
    buffer[response_index] = response_byte;
    response_index++;
  }
  else if(response_index == (6+2+response_length-1))
  {
    /* Last byte of checksum */
    buffer[response_index] = response_byte;

    if(ubx_crc_verify(buffer, (6+2+response_length)))
    {
      gnss_parse(buffer);
    }

    response_index = 0;
  }
  else
  {
    response_index = 0;
  }
}

bool pps_set_time = false;
RTCDateTime time_set_timespec;

/* To be copied over on PPS pulse */
struct tm time_ref_candidate_tm;
uint32_t time_ref_candidate_tm_ms;
uint32_t time_ref_candidate_s;
uint32_t time_ref_candidate_f;

/* Copied over to on PPS pulse */
struct tm time_ref_tm = { 0 };
uint32_t time_ref_tm_ms = 0;
uint32_t time_ref_s = 0;
uint32_t time_ref_f = 0;

extern RTCDateTime time_ref_timespec;

static virtual_timer_t status_demote_timer;
static virtual_timer_t stratum_demote_timer;

static void cb_status_demote(void *arg)
{
  (void)arg;

  ntpd_status.status = NTPD_IN_HOLDOVER;
}

static void cb_stratum_demote(void *arg)
{
  (void)arg;

  ntpd_status.status = NTPD_DEGRADED;
  ntpd_status.stratum = 16;
}

void pps_set_time_cb(void *arg)
{
  (void)arg;

  if(!pps_set_time)
  {
    return;
  }
  
  rtcSetTime(&RTCD1, &time_set_timespec);

  RTCD1.rtc->CR &= ~RTC_CR_ALRAIE;
  RTCD1.rtc->CR &= ~RTC_CR_ALRAE;

  pps_set_time = false;

  time_ref_tm = time_ref_candidate_tm;
  time_ref_tm_ms = time_ref_candidate_tm_ms;
  time_ref_s = time_ref_candidate_s;
  time_ref_f = time_ref_candidate_f;

  ntpd_status.status = NTPD_IN_LOCK;
  ntpd_status.stratum = 1;

  /* Reset stratum demote timer */
  chSysLockFromISR();
  chVTSetI(&status_demote_timer, NTPD_STATUS_DEMOTE_TIMER_PERIOD, cb_status_demote, NULL);
  chVTSetI(&stratum_demote_timer, NTPD_STRATUM_DEMOTE_TIMER_PERIOD, cb_stratum_demote, NULL);
  chSysUnlockFromISR();
}


static uint8_t gnss_input_buffer[512];

THD_FUNCTION(gnss_thread, arg)
{
  (void)arg;

  chRegSetThreadName("gnss");

  /* Start GNSS UART */
  sdStart(&SD3, &serial_gnss_config);

  /* Register for DATA IN event */
  event_listener_t serial_gnss_listener;
  chEvtRegisterMaskWithFlags(chnGetEventSource(&SD3),
                             &serial_gnss_listener,
                             EVENT_MASK(1),
                             CHN_INPUT_AVAILABLE);

  gnss_configure();

  /* Set up timer to drop stratum */
  chVTSet(&stratum_demote_timer, NTPD_STRATUM_DEMOTE_TIMER_PERIOD, cb_stratum_demote, NULL);

  palEnableLineEvent(LINE_GNSS_PPS, PAL_EVENT_MODE_RISING_EDGE);
  palSetLineCallback(LINE_GNSS_PPS, pps_set_time_cb, NULL);

  uint32_t i;
  uint32_t read_length;

  eventflags_t flags;
  while(true)
  {
    if(0 != chEvtWaitOneTimeout(EVENT_MASK(1), TIME_MS2I(100)))
    {
      /* Event Received */
      flags = chEvtGetAndClearFlags(&serial_gnss_listener);
      if (flags & CHN_INPUT_AVAILABLE)
      {
        /* Data available read here.*/
        read_length = sdReadTimeout(&SD3, gnss_input_buffer, sizeof(gnss_input_buffer), TIME_IMMEDIATE);

        i = 0;
        while(read_length--)
        {
          gnss_ubx_rx(gnss_input_buffer[i++]);
        }
      }
    }

    //watchdog_feed(WATCHDOG_DOG_GNSS);
  }
};