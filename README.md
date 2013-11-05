qmi-dialer
==========

qmi-dialer is a (hopfully) easy-to-follow example of how to use QMI to establish a modem connection. It also outputs some information, some is acquired through indications and other using active polling. I have tried to avoid using deprecated system calls, but the modems I have tested with did not properly support verifying (unlocking) PIN through the UIM-service. Therefore, DMS is used.

This application is not meant to support all QMI devices, for developing something like that you should look into libqmi. Instead, it is meant to serve as an introduction to QMI. The core of the application is an event loop and each QMI service has a state machine. When certain criteria is met (modem has service and pin is unlocked), a connection attempt is made. A simple timeout ensures that connections are retried in case they fail (assuming conditions are still met).

Once a connection is established, QMI's AUTOCONNECT feature is used to ensure automatic re-establishment of connections. However, AUTOCONNECT has some weird behavior I don't quite understand. It seems to fail if the modem loses packet service. However, without disabling autoconnect, a connect (START_NETWORK_INTERFACE) call will still return NoEffect. Anyhow, the application disables autoconnect and falls back to "normal" connection establishment if packet service is lost.

The application has been tested with and works fine with ZTE MF821D, Alcatel OneTouch and Sierra Wireless MC7750. It has only been developed for UMTS (WCDMA) and LTE networks.

Command line arguments
----------------------

qmid supports the following command line arguments

* --device / -d : Path to QMI device (typically /dev/cdc-wdmX)
* --apn / -a : APN to connect to
* --pin / -p : PIN code (optional)
* --local / -l : Lock to UMTS (3G).
* -v : Verbosity level (three levels)
