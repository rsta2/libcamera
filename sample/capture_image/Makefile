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

EXTRACLEAN += data2png

include $(LIBCAMERAHOME)/Rules.mk

data2png: data2png.c ../config.h
	gcc -O2 -o data2png data2png.c -lgd

-include $(DEPS)
