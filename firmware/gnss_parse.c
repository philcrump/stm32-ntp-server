#include "main.h"

#include "lwip/def.h"

#include <string.h>

typedef struct
{
    uint32_t itow;
    uint16_t year;
    uint8_t month; // jan = 1
    uint8_t day;
    uint8_t hour; // 24
    uint8_t min;
    uint8_t sec;
    uint8_t valid;
    uint32_t tAcc;
    int32_t nano;
    uint8_t fixtype;
    uint8_t flags;
    uint8_t flags2;
    uint8_t numsv;
    //      1e-7       mm       mm
    int32_t lon, lat, height, hMSL;
    //        mm     mm
    uint32_t hAcc, vAcc;
    //      mm/s   mm/s  mm/s  mm/s
    int32_t velN, velE, velD, gSpeed; // millimeters
} __attribute__((packed)) nav_pvt_t;

typedef struct
{
    uint32_t itow;
    uint8_t version;
    uint8_t num_svs;
    uint8_t _reserved1;
    uint8_t _reserved2;
} __attribute__((packed)) nav_sat_header_t;

typedef struct
{
    uint8_t gnss_id;
    uint8_t sv_id;
    uint8_t cn0;
    int8_t elevation;
    int16_t azimuth;
    int16_t psr_res;
    uint32_t flags;
} __attribute__((packed)) nav_sat_sv_t;

extern bool pps_set_time;
extern RTCDateTime time_set_timespec;

extern struct tm time_ref_candidate_tm;
extern uint32_t time_ref_candidate_tm_ms;
extern uint32_t time_ref_candidate_s;
extern uint32_t time_ref_candidate_f;

static virtual_timer_t cancel_pps_set_timer;

static void cb_cancel_pps_set(void *arg)
{
  (void)arg;

  pps_set_time = false;
}

void gnss_parse(uint8_t *buffer)
{
    if(buffer[2] == 0x01 && buffer[3] == 0x07) /* NAV-PVT */
    {
        nav_pvt_t *pvt = (nav_pvt_t *)(&buffer[6]);

        struct tm tm;
        memset(&tm, 0, sizeof(tm));
        tm.tm_year = pvt->year - 1900;
        tm.tm_mon = pvt->month - 1;
        tm.tm_mday = pvt->day;
        tm.tm_hour = pvt->hour;
        tm.tm_min = pvt->min;
        tm.tm_sec = pvt->sec;

        gnss_status.fix = (pvt->fixtype == 0x03) || (pvt->fixtype == 0x04);
        gnss_status.time_accuracy_ns = pvt->tAcc;

        gnss_status.gnss_timestamp = mktime(&tm);
        gnss_status.lat = pvt->lat;
        gnss_status.lon = pvt->lon;
        gnss_status.alt = pvt->height;
        gnss_status.h_acc = pvt->hAcc;
        gnss_status.v_acc = pvt->vAcc;

        if(gnss_status.fix)
        {
            pps_set_time = false;
            rtcConvertStructTmToDateTime(&tm, 0, &time_set_timespec);
            /* Don't set time in the 1.5s before midnight - TODO: Handle rollover properly instead */
            if(time_set_timespec.millisecond < 86398500)
            {
                /* Next second */
                time_set_timespec.millisecond += 1000;

                /* Set candidate reference timestamps */
                rtcConvertDateTimeToStructTm(&time_set_timespec, &time_ref_candidate_tm, &time_ref_candidate_tm_ms);
                time_ref_candidate_s = htonl((uint32_t)mktime(&time_ref_candidate_tm) - DIFF_SEC_1970_2036);
                time_ref_candidate_f = htonl((NTP_MS_TO_FS_U32 * time_ref_candidate_tm_ms));

                pps_set_time = true;
                /* Set up timer to cancel if the PPS is overdue (>1000ms after we've received this) */
                chVTSet(&cancel_pps_set_timer, TIME_MS2I(1000), cb_cancel_pps_set, NULL);
            }
        }
    }
    else if(buffer[2] == 0x01 && buffer[3] == 0x35) /* NAV-SAT */
    {
        nav_sat_header_t *sat_header = (nav_sat_header_t *)(&buffer[6]);

        /* Copy SVs into status struct */
        memcpy((void *)gnss_status.svs, &buffer[6+8], (sat_header->num_svs * 12));
        gnss_status.svs_count = sat_header->num_svs;

        nav_sat_sv_t *sat_sv;

        gnss_status.svs_acquired_count = 0;
        gnss_status.svs_locked_count = 0;
        gnss_status.svs_nav_count = 0;

        for(uint32_t i = 0; i < sat_header->num_svs; i++)
        {
            sat_sv = (nav_sat_sv_t *)(&buffer[6+8+(12*i)]);

            if((sat_sv->flags & 0x7) >= 2)
            {
                /* SV acquired */
                gnss_status.svs_acquired_count++;
            }

            if((sat_sv->flags & 0x7) >= 4)
            {
                /* SV locked (note: acquired are included) */
                gnss_status.svs_locked_count++;
            }

            if(((sat_sv->flags & 0x8) >> 3) == 1)
            {
                /* Used in navigation solution */
                gnss_status.svs_nav_count++;
            }
        }
    }
}