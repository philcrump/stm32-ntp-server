#include "main.h"

static const WDGConfig wdg_cfg = {
  // 40 KHz input clock from LSI
  .pr = STM32_IWDG_PR_256, // Prescaler = 256 => 156.25Hz
  .rlr = STM32_IWDG_RL(79), // Reload value = 79 (slightly over 500ms),
};

static bool wdg_initialised = false;
static uint32_t mask = WATCHDOG_MASK;
static uint32_t fed = 0;

void watchdog_init(void)
{
  if(wdg_initialised)
  {
    return;
  }

  wdgStart(&WDGD1, &wdg_cfg);
  wdg_initialised = true;
}

THD_FUNCTION(watchdog_service_thread, arg)
{
  (void)arg;

  /* Should have been init-ed by main(), but let's make sure */
  watchdog_init();

  while(1)
  {
    if((mask & fed) == mask)
    {
      fed = 0;

      // Feed the hardware dog
      wdgReset(&WDGD1);
    }

    chThdSleepMilliseconds(20);
  }
}

/* To be called by other threads */
void watchdog_feed(uint32_t dog)
{
  fed |= ((1 << dog) & 0xFFFFFFFF);
}