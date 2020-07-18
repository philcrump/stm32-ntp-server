#include "main.h"

#include <lwip/dhcp.h>

static struct netif* _ip_netif_ptr = NULL;

static bool _ip_link_is_up = false;

uint32_t app_ip_link_status(void)
{
  if(_ip_link_is_up && _ip_netif_ptr != NULL)
  {
    if(netif_dhcp_data(_ip_netif_ptr)->state == 10) // DHCP_STATE_BOUND = 10
    {
      return APP_IP_LINK_STATUS_BOUND;
    }
    else
    {
      return APP_IP_LINK_STATUS_UPBUTNOIP;
    }
  }
  else
  {
    return APP_IP_LINK_STATUS_DOWN;
  }
}

void ip_link_up_cb(void *p)
{
  _ip_netif_ptr = (struct netif*)p;

  dhcp_start(_ip_netif_ptr);

  _ip_link_is_up = true;
  palSetLine(LINE_LED1);
}

void ip_link_down_cb(void *p)
{
  _ip_netif_ptr = (struct netif*)p;

  dhcp_stop(_ip_netif_ptr);

  _ip_link_is_up = false;
  palClearLine(LINE_LED1);
}