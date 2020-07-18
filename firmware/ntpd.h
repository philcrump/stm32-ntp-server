#ifndef __NTPD_H__
#define __NTPD_H__

#define NTP_MS_TO_FS_U32  (4294967296.0 / 1000.0)
#define NTP_MS_TO_FS_U16  (65536.0 / 1000.0)

/* Number of seconds between 1970 and Feb 7, 2036 06:28:16 UTC (epoch 1) */
#define DIFF_SEC_1970_2036          ((uint32_t)2085978496L)

typedef enum {
	NTPD_UNSYNC = 0,
	NTPD_IN_LOCK = 1,
	NTPD_IN_HOLDOVER = 2,
	NTPD_DEGRADED = 3
} ntpd_status_status_t;

typedef struct {
	ntpd_status_status_t status;
	uint32_t requests_count;
	uint8_t stratum;
} ntpd_status_t;

extern ntpd_status_t ntpd_status;

THD_FUNCTION(ntpd_thread, arg);

#endif /* __NTPD_H__ */