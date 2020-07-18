# NTP Server [![Build Status](https://travis-ci.org/philcrump/stm32-ntpd.svg?branch=master)](https://travis-ci.org/philcrump/stm32-ntpd)

This is a home NTP server written for the ST NUCLEO-STM32F429ZI Development Board.

A ublox GNSS receiver is used to receive the current time, and on the next rising edge of the timepulse signal, the RTC is set to (time+1s). The RTC only counts in milliseconds, so there is always up to +/-1ms of jitter.


With the use of the LSE crystal on the board holdover performance of the uncalibrated RTC has been measured as <1 millisecond / minute of drift. The holdover time is therefore set to 10 minutes by default to allow up to 10ms of drift before the NTP server marks itself as Stratum 16 ("Unsynchronised").


<p float="center">
  <img src="/images/web_status.png" width="70%" />
</p>

## Performance

This firmware currently takes around \~72µs to service each NTP request, with \~36µs of this spent in the service function.

This equates to a maximum capability of \~12,000 reqs / second. No effort has been taken to optimise this so greater performance would be possible, however this is far in excess of what is required on a home network.

The firmware currently switches on LED2 on the board when in the service function, this causes a single brief flash for each request, and also allows us to observe the timing performance:

<p float="center">
  <img src="/images/maximum_rate.png" width="70%" />
</p>

If you're after a high performance NTP server then I can personally recommend the [LeoNTP Time Server](https://store.uputronics.com/index.php?route=product/product&product_id=92), which can saturate 100Mb/s ethernet at 110,000 reqs / second.

## Wiring

* PC10 (TXD)
* PC11 (RXD)
* PC12 is configured to trigger synchronisation of the RTC on the rising edge of the GNSS Timepulse

Wiring is shown for direct 3.3V attachment. 5V GNSS receivers with onboard or inline regulators can also be used.

<p float="center">
  <img src="/images/gnss_receiver.png" width="60%" />
</p>

This project is built on ChibiOS 20.3.x with LwIP 2.1.x