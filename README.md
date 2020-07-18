# NTP Server [![Build Status](https://travis-ci.org/philcrump/stm32-ntpd.svg?branch=master)](https://travis-ci.org/philcrump/stm32-ntpd)

This project is an embedded NTPv4 server using the latest-stable ChibiOS and lwIP software stacks.

The hardware components are:

* ST STM32F429ZI ARM Cortex-M4 Microcontroller Development Board [Product](https://www.st.com/en/evaluation-tools/nucleo-f429zi.html)
* ublox MAX-M8Q GNSS Receiver Breakout Board [Product](https://store.uputronics.com/index.php?route=product/product&product_id=84) + suitable antenna

## GNSS Time Sync

The GNSS receiver is configured to send the current time over serial, and on the next rising edge of the timepulse signal the RTC is set to (time+1s). The STM32 RTC only counts in milliseconds, so there is always up to +/-1ms of jitter. (Future work may include using a GPT to measure sub-millisecond intervals from last timepulse edge.)

Despite this, simple observation through `ntpq -p` stats against a known-good remote Stratum 1 NTP server shows similar characteristics:

<p float="center">
  <img src="/images/ntpq.png" width="80%" />
</p>

## Clock Holdover

The LSE crystal oscillator on the ST board is specced to 20ppm of tolerance. This has been measured in this application as <1 millisecond / minute of drift. The holdover time is therefore set to 10 minutes by default to allow up to 10ms of drift before the NTP server marks itself as Stratum 16 ("Unsynchronised"). (Future work may incude the auto-calibration of STM32 RTC to improve this for a specific environment.)

## Status Webpage

The software hosts a webpage showing the current status of the GNSS and NTPD subsystems on port 80:

<p float="center">
  <img src="/images/web_status.png" width="70%" />
</p>

## NTP Request Rate Performance

This firmware currently takes around \~72µs to service each NTP request, with \~36µs of this spent in the service function.

This equates to a maximum capability of \~12,000 reqs / second. No effort has been taken to optimise this so greater performance would be possible, however this is far in excess of what is required for the author's application.

The firmware currently lights up LED2 on the ST board when in the NTP service function, this causes a single brief flash for each request, and also allows us to observe the timing performance in hardware (HIGH == in service function):

<p float="center">
  <img src="/images/maximum_rate.png" width="70%" />
</p>

If you're after a high-performance embedded NTP server appliance then I can personally recommend the [LeoNTP Time Server](https://store.uputronics.com/index.php?route=product/product&product_id=92) product, hand-coded in PIC assembly to capably saturate 100Mb/s ethernet at 110,000 reqs / second.

## Wiring

* PC10 (TXD)
* PC11 (RXD)
* PC12 is configured to trigger synchronisation of the RTC on the rising edge of the GNSS Timepulse

The signals (TXD, RXD, PPS) are 3.3V.

Wiring is shown for direct 3.3V attachment. Some GNSS receivers have an integrated 3.3V regulator and so the Vcc must be fed from a 5V pin.

<p float="center">
  <img src="/images/gnss_receiver.png" width="60%" />
</p>

# Copyright

GPLv3 licensed. © Phil Crump - phil@philcrump.co.uk

Derivations from other license-compatible works are acknowledged in the source.