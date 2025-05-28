libcamera
=========

> Raspberry Pi is a trademark of Raspberry Pi Ltd.

This project provides camera support for Circle-based applications. Only cameras with CSI-2 interface are supported. The drivers in this project directly access the hardware, without using MMAL / VCHIQ software components. There are no specific settings in the file *config.txt* necessary.

Status
------

Currently the Raspberry Pi Camera Module 1 (with OV5647 sensor) and Camera Module 2 (with IMX219 sensor) are supported, connected to the camera connector of the following Raspberry Pi models:

* Raspberry Pi Model B (Release 2 with 512 MB)
* Raspberry Pi Model A+
* Raspberry Pi Model B+
* Raspberry Pi Zero (v1.3 only)
* Raspberry Pi Zero W
* Raspberry Pi Zero 2 W
* Raspberry Pi 2 Model B
* Raspberry Pi 3 Model B
* Raspberry Pi 3 Model A+
* Raspberry Pi 3 Model B+
* Raspberry Pi 4 Model B

For the Raspberry Pi Zero 2 W you need the file *bcm2710-rpi-zero-2-w.dtb* on the SD card, to be able to use it with a camera. This file was not used with Circle before. Go to the directory *circle/boot/* in this project and enter `make` to download the Raspberry Pi firmware with this file included.

Getting
-------

Normally you need a *git* client to get the libcamera source code. Go to the directory where you want to place libcamera on your hard disk and enter:

	git clone https://github.com/rsta2/libcamera.git
	cd libcamera
	git submodule update --init

This will place the source code in the subdirectory *libcamera/* and clones the submodule *circle* into the *libcamera/circle/* subdirectory.

Building
--------

libcamera uses the Circle bare metal build environment for the Raspberry Pi. You need an appropriate compiler toolchain for ARM processors to build it. Have a look at the Circle *README.md* file (in *circle/*) for further information on this (section *Building*).

When the toolchain is installed on your computer you can build libcamera using the following commands:

	./configure 4 arm-none-eabi-
	./make -j

The `configure` command writes a *Config.mk* file for Circle. "4" is the major revision number of your Raspberry Pi (1, 2, 3 or 4). The second (optional) parameter is the prefix of the commands of your toolchain and can be preceded with a path. Do not forget the dash at the end of the prefix! An additional third parameter (32 or 64) may be specified to select the AArch target architecture.

If the build was successful, you find the library file of libcamera in the *lib/* subdirectory with the name *libcamera.a*.

Samples
-------

If you want to try one of the provided sample programs, now go its subdirectory in *sample/* and do:

	make

The built kernel image can be installed as described in the main Circle *README.md* file. Please read the file *README* in the subdirectory of the sample for more info!

Documentation
-------------

The camera API is defined by the public interface of the following classes:

* CCameraManager (Camera initialization and auto-probing)
* CCameraBuffer (Manages access to a captured frame (image) from a camera)
* CCameraDevice (everything else)

If you have Doxygen installed on your computer, you can build the libcamera documentation with:

	./makedoc

Then open the file *doc/html/index.html* in your web browser!
