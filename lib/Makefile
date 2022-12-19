#
# Makefile
#

LIBCAMERAHOME = ..

OBJS	= cameramodule1.o cameramodule2.o cameramanager.o \
	  cameradevice.o csi2cameradevice.o \
	  cameracontrol.o camerabuffer.o camerainfo.o

libcamera.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(LIBCAMERAHOME)/Rules.mk

-include $(DEPS)
