/**
 * @file web.c
 * @brief HTTP server wrapper thread code.
 * @addtogroup WEB_THREAD
 * @{
 */

#include "../main.h"

#include "lwip/api.h"
#include "lwip/netif.h"
#include "web_paths.h"

#include <string.h>

static char packet_buffer[WEB_MAX_PACKET_SIZE];
static char url_buffer[WEB_MAX_PATH_SIZE];
/**
 * @brief   Decodes an URL sting.
 * @note    The string is terminated by a zero or a separator.
 *
 * @param[in] url       encoded URL string
 * @param[out] buf      buffer for the processed string
 * @param[in] max       max number of chars to copy into the buffer
 * @return              The conversion status.
 * @retval false        string converted.
 * @retval true         the string was not valid or the buffer overflowed
 *
 * @notapi
 */
#define HEXTOI(x) (isdigit(x) ? (x) - '0' : (x) - 'a' + 10)
static bool decode_url(const char *url, char *buf, size_t max)
{
  while (true) {
    int h, l;
    unsigned char c = *url++;

    switch (c) {
    case 0:
    case '\r':
    case '\n':
    case '\t':
    case ' ':
    case '?':
      *buf = 0;
      return true;
    case '.':
      if (max <= 1)
        return false;

      h = *(url + 1);
      if (h == '.')
        return false;

      break;
    case '%':
      if (max <= 1)
        return false;

      h = tolower((int)*url++);
      if (h == 0)
        return false;
      if (!isxdigit(h))
        return false;

      l = tolower((int)*url++);
      if (l == 0)
        return false;
      if (!isxdigit(l))
        return false;

      c = (char)((HEXTOI(h) << 4) | HEXTOI(l));
      break;
    default:
      if (max <= 1)
        return false;

      if (!isalnum(c) && (c != '_') && (c != '-') && (c != '+') &&
          (c != '/'))
        return false;

      break;
    }

    *buf++ = c;
    max--;
  }
}

static void http_server_serve(struct netconn *conn)
{
  struct netbuf *inbuf;
  err_t err;
  u16_t packetlen;

  /* Read the data from the port, blocking if nothing yet there.
   We assume the request (the part we care about) is in one netbuf */
  err = netconn_recv(conn, &inbuf);

  if(err != ERR_OK)
  {
    netconn_close(conn);
    netbuf_delete(inbuf);
    return;
  }

  packetlen = netbuf_len(inbuf);
  /* Check we've got room for the packet */
  if(packetlen >= WEB_MAX_PACKET_SIZE)
  {
    /* We haven't, fail */
    netconn_close(conn);
    netbuf_delete(inbuf);
    return;
  }
  netbuf_copy(inbuf, packet_buffer, WEB_MAX_PACKET_SIZE);

  /* Is this an HTTP GET command? (only check the first 5 chars, since
  there are other formats for GET, and we're keeping it very simple )*/
  if(packetlen>=5 && (0 == memcmp("GET /", packet_buffer, 5)))
  {
    if(!decode_url(packet_buffer + (4 * sizeof(char)), url_buffer, WEB_MAX_PATH_SIZE))
    {
      /* URL decode failed.*/
      netconn_close(conn);
      netbuf_delete(inbuf);
      return;
    }

    web_paths_get(conn, url_buffer);
  }

  /* Close the connection (server closes in HTTP) */
  netconn_close(conn);

  /* Delete the buffer (netconn_recv gives us ownership,
   so we have to make sure to deallocate the buffer) */
  netbuf_delete(inbuf);
}

/**
 * Stack area for the http thread.
 */
THD_WORKING_AREA(wa_http_server, WEB_THREAD_STACK_SIZE);

/**
 * HTTP server thread.
 */
THD_FUNCTION(http_server, p) {
  struct netconn *conn, *newconn;
  err_t err;

  (void)p;
  chRegSetThreadName("http");

  conn = netconn_new(NETCONN_TCP);
  if(conn == NULL)
  {
    chThdExit(MSG_RESET);
  }

  netconn_bind(conn, IP_ADDR_ANY, WEB_THREAD_PORT);

  netconn_listen(conn);

  /* Goes to the final priority after initialization.*/
  chThdSetPriority(WEB_THREAD_PRIORITY);

  while (true) {
    err = netconn_accept(conn, &newconn);
    if (err != ERR_OK)
      continue;
    http_server_serve(newconn);
    netconn_delete(newconn);
  }
}

void web_init(void)
{
  chThdCreateStatic(wa_http_server, sizeof(wa_http_server), NORMALPRIO + 1, http_server, NULL);
}

/** @} */
