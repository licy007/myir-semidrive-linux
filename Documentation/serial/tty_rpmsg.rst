.. SPDX-License-Identifier: GPL-2.0

=============
The rpmsg TTY
=============

The rpmsg tty driver implements serial communication on the RPMsg bus to makes possible for user-space programs to send and receive rpmsg messages as a standard tty protocol.

The remote processor can instantiate a new tty by requesting:
- a "rpmsg-tty-raw" RPMsg service, for TTY raw data support without flow control
- a "rpmsg-tty-ctrl" RPMSg service, for TTY support with flow control.

Information related to the RPMsg and associated tty device is available in
/sys/bus/rpmsg/devices/.

RPMsg TTY without control
---------------------

The default end point associated with the "rpmsg-tty-raw" service is directly
used for data exchange. No flow control is available.

To be compliant with this driver, the remote firmware must create its data end point associated with the "rpmsg-tty-raw" service.

RPMsg TTY with control
---------------------

The default end point associated with the "rpmsg-tty-ctrl" service is reserved for
the control. A second endpoint must be created for data exchange.

The control channel is used to transmit to the remote processor the CTS status,
as well as the end point address for data transfer.

To be compatible with this driver, the remote firmware must create or use its end point associated with "rpmsg-tty-ctrl" service, plus a second endpoint for the data flow.
On Linux rpmsg_tty probes, the data endpoint address and the CTS (set to disable)
is sent to the remote processor.
The remote processor has to respect following rules:
- It only transmits data when Linux remote cts is enable, otherwise message
  could be lost.
- It can pause/resume reception by sending a control message (rely on CTS state).

Control message structure:
struct rpmsg_tty_ctrl {
	u8 cts;			/* remote reception status */
	u16 d_ept_addr;		/* data endpoint address */
};
