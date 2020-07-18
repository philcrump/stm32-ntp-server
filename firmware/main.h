#ifndef __MAIN_H__
#define __MAIN_H__

#include "ch.h"
#include "hal.h"

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

/* Round up N to the next multiple of S */
#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))

/* Mark binary constants as known GCC extension to suppress warning */
#define GCC_BINARY(x)   (__extension__ (x))

#define _token_concat(x,y) x##y
#define token_concat(x,y) _token_concat(x,y)

#define nibble_to_hex(i) ((i) <= 9 ? '0' + (i) : 'A' - 10 + (i))

#define ITERATOR_DEF()    uint8_t iterator = 0
#define ITERATOR_VAR()    iterator < 1 ? "/" : iterator < 2 ? "-" : "\\"
#define ITERATOR_INC()    iterator = iterator < 2 ? iterator + 1 : 0

#include "config.h"

#include "watchdog.h"
#include "ip_link.h"
#include "ntpd.h"
#include "gnss.h"
#include "web/web.h"

#endif /* __MAIN_H__ */