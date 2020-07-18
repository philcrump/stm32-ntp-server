#ifndef __GNSS_H__
#define __GNSS_H__

typedef struct {
    uint8_t gnss_id;
    uint8_t sv_id;
    uint8_t cn0;
    int8_t elevation;
    int16_t azimuth;
    int16_t psr_res;
    uint32_t flags;
} __attribute__((packed)) gnss_status_sv_t;
/* packet to allow memcpy from ubx */

typedef struct {
    bool fix;
    uint32_t time_accuracy_ns;
    int32_t time_nanosecond;

    uint64_t gnss_timestamp;
    int32_t lat;
    int32_t lon;
    int32_t alt;
    uint32_t h_acc;
    uint32_t v_acc;

    gnss_status_sv_t svs[32];
    uint8_t svs_count;
    uint8_t svs_acquired_count;
    uint8_t svs_locked_count;
    uint8_t svs_nav_count;
} gnss_status_t;

extern gnss_status_t gnss_status;

THD_FUNCTION(gnss_thread, arg);

#endif /* __GNSS_H__ */