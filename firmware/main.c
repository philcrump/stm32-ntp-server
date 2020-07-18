#include "main.h"

#include <math.h>

#include "lwipthread.h"

static THD_WORKING_AREA(watchdog_service_wa, 128);
static THD_WORKING_AREA(ntpd_wa, 4096);
static THD_WORKING_AREA(gnss_wa, 128);

static uint8_t _macAddress[] = {0xC2, 0xAF, 0x51, 0x03, 0xCF, 0x47};
static const lwipthread_opts_t lwip_opts = {
  .macaddress = _macAddress,
  .address = 0,
  .netmask = 0,
  .gateway = 0,
  .addrMode = NET_ADDRESS_DHCP,
  .ourHostName = "stm32ntpd",
  .link_up_cb = ip_link_up_cb,
  .link_down_cb = ip_link_down_cb
};

int main(void)
{
  halInit();
  chSysInit();

  /* For some really weird reason the ethernet peripheral requires the PLL SAI clock to be on (both STM32F4 & STM32F7) */
  // Enable PLLSAI
  RCC->CR |= RCC_CR_PLLSAION;
  // Wait for PLLSAI to lock
  while(!(RCC->CR & RCC_CR_PLLSAIRDY));     // wait for PLLSAI to lock

  /* Set up watchdog */
  watchdog_init();
  chThdCreateStatic(watchdog_service_wa, sizeof(watchdog_service_wa), HIGHPRIO, watchdog_service_thread, NULL);

  /* Set up IP stack */
  lwipInit(&lwip_opts);

  web_init();

  /* NTPD Thread */
  chThdCreateStatic(ntpd_wa, sizeof(ntpd_wa), NORMALPRIO, ntpd_thread, NULL);

  /* GNSS Thread */
  chThdCreateStatic(gnss_wa, sizeof(gnss_wa), LOWPRIO, gnss_thread, NULL);

  while(true)
  {
    watchdog_feed(WATCHDOG_DOG_MAIN);
    chThdSleepMilliseconds(100);
  }
}
