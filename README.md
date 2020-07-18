# NTP Server [![Build Status](https://travis-ci.org/philcrump/stm32-ntp-server.svg?branch=master)](https://travis-ci.org/philcrump/stm32-ntp-server)

This project is an embedded NTPv4 server using the ChibiOS (20.3.x) and lwIP (2.1.x) software stacks.

The hardware components are:

* ST STM32F429ZI ARM Cortex-M4 Development Board - [Product Link](https://www.st.com/en/evaluation-tools/nucleo-f429zi.html)
* ublox MAX-M8Q GNSS Receiver Breakout Board -  [Product Link](https://store.uputronics.com/index.php?route=product/product&product_id=84) (+ suitable antenna)

## GNSS Time Sync

The GNSS receiver is configured to send the current time over serial, and on the next rising edge of the timepulse signal the RTC is set to (time+1s). The STM32 RTC only counts in milliseconds, so there is always up to +/-1ms of jitter. (Future work may include using a GPT to measure sub-millisecond precision from last timepulse edge.)

Despite this, simple observation through `ntpq -p` stats against a known-good remote Stratum 1 NTP server shows favourable characteristics:

<p align="center">
  <img src="/images/ntpq.png" width="80%" />
</p>

## Clock Holdover

The 32.768KHz crystal oscillator on the ST board is specified to 20ppm of tolerance. This has been measured in this application as <1 millisecond of drift per minute. The holdover time is therefore set to 10 minutes by default to allow up to 10ms of drift before the NTP server marks itself as Stratum 16 ("Unsynchronised"). (Future work may use the calibration routines of the STM32 RTC during periods of GNSS lock to improve this.)

## Status Webpage

The software hosts a webpage showing the current status of the GNSS and NTP subsystems on port 80:

<p align="center">
  <img src="/images/web_status.png" width="75%" />
</p>

## NTP Request Rate Performance

The firmware takes \~72µs to service each NTP request, with \~36µs of this spent in the service function.

This equates to a maximum capability of \~12,000 reqs / second. No effort has been taken to optimise this so greater performance would be possible, however this is far in excess of what is required for the author's application.

The firmware lights up LED2 on the ST board when in the NTP service function, this causes a single brief flash for each request, and also allows us to observe the timing performance in hardware (HIGH == in service function):

<p align="center">
  <img src="/images/maximum_rate.png" width="70%" />
</p>

If you're after a high-performance embedded NTP server appliance then I can personally recommend the [LeoNTP Time Server](https://store.uputronics.com/index.php?route=product/product&product_id=92), hand-coded in PIC assembly to capably saturate 100Mb/s ethernet at 110,000 reqs / second with sub-µs jitter.

## Software Configuration

`firmware/config.h` contains config directives.

* `GNSS_AID_` directives supply a position and it's standard deviation error to the GNSS Receiver to aid the initial position fix. If these are wrong then the GNSS receiver may fail to get a fix. Comment them out to disable.

* `NTPD_STRATUM_DEMOTE_TIMER_PERIOD` Sets the time period after last timepulse sync that the NTP server will advertise Stratum 1 (GPS-synced) before dropping to Stratum 16 (Unsynchronized).

* `NTPD_STATUS_DEMOTE_TIMER_PERIOD` Sets the time period after last timepulse sync that the Webpage will show 'In Lock' before dropping to 'Holdover'. Does not affect NTP output.

## Software Compilation

This toolchain is designed for Linux.

You must have `arm-none-eabi-gcc` from [GNU ARM embedded toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads) in your path.

Running `firmware/build -M` will pack the web files and then compile all source files in parallel. The final linker stage can take a few seconds. Running without the `-M` flag will remove parallel compilation and may help errors to be more readable.

## Flashing

`firmware/flash` will flash a compiled firmware image. This first looks for a connected BlackMagic JTAG probe, and if unsuccessful will drop back to `st-flash` from [st-utils](https://github.com/stlink-org/stlink) to flash over the ST-Link USB.

`firmware/build -MF` will build, and if successful also attempt to flash.

## Wiring

The pins used on the STM32F429ZI are:

* PC10 (TXD)
* PC11 (RXD)
* PC12 is configured to trigger synchronisation of the RTC on the rising edge of the GNSS Timepulse

These are broken out in the first 3 pins of CN11 which is the unpopulated 0.1" pads in the upper left of the board. Power supply pins are available a few rows further down.

Wiring is shown below for direct 3.3V attachment. Some GNSS receivers have an integrated 3.3V regulator and so the Vcc must be fed from a 5V pin. The IO signals are still 3.3V.

<p align="center">
  <img src="/images/gnss_receiver.png" width="80%" />
</p>

# Copyright

GPLv3 licensed. © Phil Crump - phil@philcrump.co.uk

Derivations from other license-compatible works are acknowledged in the source.