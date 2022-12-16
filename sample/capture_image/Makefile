#
# Makefile
#

LIBCAMERAHOME = ../..

OBJS	= main.o kernel.o

LIBS	= $(LIBCAMERAHOME)/lib/libcamera.a \
	  $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
	  $(CIRCLEHOME)/addon/fatfs/libfatfs.a \
	  $(CIRCLEHOME)/lib/usb/libusb.a \
	  $(CIRCLEHOME)/lib/fs/libfs.a \
	  $(CIRCLEHOME)/lib/input/libinput.a \
	  $(CIRCLEHOME)/lib/libcircle.a

include $(LIBCAMERAHOME)/Rules.mk

-include $(DEPS)
