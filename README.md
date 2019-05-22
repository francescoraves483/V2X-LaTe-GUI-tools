V2X-LaTe-GUI-tools
==================

Simple Linux GUI tools to perform tests with [OpenWrt-V2X](https://github.com/francescoraves483/OpenWrt-V2X) and iPerf, set channel and physical data rate on any board running a 802.11p patched version of OpenWrt and connected to the PC, running the tool, with Ethernet (using ssh) and to automatically plot CSV results from [LaTe](https://github.com/francescoraves483/LaMP_LaTe) using MatLab.

This repository is organized in different folders, which are described below.

_iperfLauncher_
===============

iPerf test launcher
-------------------

This GUI tool has been written in C, using GTK+ 3.0 and the [slope](https://github.com/bytebrew/slope) library.

It aims at simplifying the execution of iPerf tests and demos on embedded boards equipped with [OpenWrt-V2X](https://github.com/francescoraves483/OpenWrt-V2X), which includes a patched version of iPerf able to support the four EDCA Access Categories, allowing the user to manage both an iPerf client and a server from a PC connected to the board(s) through Ethernet, in a graphical way.

Each instance of _iperfLauncher_ can connect to a single board, managing at most one server and one client on the same board. The client controls allow the user to start a test on a given Access Category, connecting to another server with a given IP address, while the server controls allow the user to manage a server running on the same board, which will update the GUI as an incoming traffic, from another client, is detected.

Internally, it invokes `ssh`.

For instance, if you want to test the wireless connectivity between two boards, you can connect two PCs (or even the same PC, for example using a switch in between), launch one instance of _iperfLauncher_ on each PC (one for each board) and start your tests. If a server is listening on _PC 1_ and you launch a test on a certain AC on _PC 2_, you will see the live test results in the GUI running on _PC 1_, which is receiving the packets from _PC 2_ over a certain traffic class.

GUI description
---------------

![iPerf test launcher screenshot](/doc/images/GUI_help_iperfLauncher.png)

1. **iPerf client**: current (or last used) AC when launching an iPerf client
2. **iPerf server**: server status (not yet checked/on/off); it should be checked by the user before doing any other operation, for proper GUI operations
3. **iPerf client**: buttons to start a client on a given AC, using the options specified in **(10)**
4. **iPerf server**: buttons to manage a server on the connected board, communicating through SSH; when **(9)** is changed, if the board is the same as before, it is always mandatory to stop and start again the server using these buttons, as the new server should run over a different port
5. **iPerf client/server**: IP address of the board that should be used to connect to it through SSH, from the PC running the GUI
6. **iPerf client**: IP address the client should send the test traffic at (i.e. the address specified after _iperf -c_
7. **iPerf client/server**: username of the account to be used, on the target board, for the connection through SSH
8. **iPerf client/server**: password of the account to be used, on the target board, for the connection through SSH
9. **iPerf client/server**: ports to be used for both the client and the server; _server:_ specifies the port the server will listen on, _client->_ specifies the port the client will connect to (a server, on another board, should be listening on that port and using the IP specified in **(6)**)
10. **iPerf client**: client options, including test duration (ranging from 5 s to 120 s - this may change in the future)
11. **iPerf client/server**: information text is displayed here, including possible errors and SSH connection status
12. **iPerf server**: indication about which plot, out of the four stored each time, is being currently updated or it has been updated last
13. **iPerf server**: information coming from the server, when an incoming traffic from another client (possibly launched from another instance of _iperfLauncher_) is detected - current value of "bandwidth" reported by iPerf and updated each second
14. **iPerf server**: information coming from the server, when an incoming traffic from another client (possibly launched from another instance of _iperfLauncher_) is detected - average "bandwidth" value, reported when the server detects the end of the test from a connected client
15. **iPerf server**: information coming from the server, when an incoming traffic from another client (possibly launched from another instance of _iperfLauncher_) is detected - connection stability, reported at the end of a test, as the maximum percentage variation in the values reported each second in **(13)**
16. **iPerf server**: live plot of the values reported in **(13)**; up to four plots are stored each time, then the least recent plot is overwritten (this may change in the future)

How to build
------------

```console
$ git clone https://github.com/francescoraves483/V2X-LaTe-GUI-tools.git
$ cd V2X-LaTe-GUI-tools/iperfLauncher
$ make
```

**Required packages**: `libgtk-3-dev`, the [slope](https://github.com/bytebrew/slope) library, by Elvis M. Teixeira and Remi Bertho

_APUbitrateset_
===============

APU data rates selector
-----------------------

This simple GUI tool, written in C using GTK+, can be used to graphically set, from a Linux PC, the wireless physical data rate on a connected embedded board supporting 802.11p and running a patched OpenWrt version supporting this standard (such as [OpenC2X-embedded](https://github.com/florianklingler/OpenC2X-embedded) and [OpenWrt-V2X](https://github.com/francescoraves483/OpenWrt-V2X)) 

The connection happens through SSH and the selectable data rates, as of now, are: 3 Mbit/s, 6 Mbit/s and 12 Mbit/s.

Even though the name says "APU", because we actually developed and tested it for PC Engines APU1D boards, it should work on any board running one of the aforementioned patched OpenWrt versions.

How to build
------------

```console
$ git clone https://github.com/francescoraves483/V2X-LaTe-GUI-tools.git
$ cd V2X-LaTe-GUI-tools/APUbitrateset
$ make
```

**Required packages**: `libgtk-3-dev`

_APUchannelset_
===============

APU channel selector
--------------------

This simple GUI is very similar to _APUbitrateset_, but is allows the selection of one of the seven IEEE 802.11p DSRC channels (from channel 172 to 184, 10 MHz wide, OCB mode) on the connected embedded board. Indeed, it shares most of the code with the data rates selector, with few important modifications.

How to build
------------

```console
$ git clone https://github.com/francescoraves483/V2X-LaTe-GUI-tools.git
$ cd V2X-LaTe-GUI-tools/APUchannelset
$ make
```

**Required packages**: `libgtk-3-dev`

_LaTe_plotter_GUI_
==================

LaTe plotter MATLAB GUI
-----------------------

This tool is a MATLAB GUI, developed on MATLAB R2018a (but tested also on R2018b and R2019a, both on Linux and Windows), which is able to create live plots from the CSV files generated by the [LaTe](https://github.com/francescoraves483/LaMP_LaTe) tool. It is still in an early beta stage.

When started, through the "Start" button, it will monitor the path specified by the user and, when a change in the target CSV file is detected, it will parse the data from last added line and use it to add a point to the plot. 

As the CSV file can be created by LaTe later on, the path should not necessarily correspond to an existing file.

**Warning:** as this is meant for demo purposes, it should **never** be used when important tests are being performed. Also, when the "Start" button is clicked, if there is an already existing CSV file on the path specified by the user, it will be replaced with an empty one, for proper operations of the tool (which, again, is meant for demo purposes). Before clicking "Start", always **make backups**! If you need to keep the old CSV file, after selecting "Stop", remember to change the path to point to another CSV file before starting again a new session with the "Start" button.

How to build
------------

Just launch `LaTe_plotter.m` from MATLAB.
