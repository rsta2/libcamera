#
# Makefile
#

LIBCAMERAHOME = ../..

OBJS	= main.o kernel.o

LIBS	= $(LIBCAMERAHOME)/lib/libcamera.a \
	  $(CIRCLEHOME)/lib/libcircle.a

include $(LIBCAMERAHOME)/Rules.mk

-include $(DEPS)
