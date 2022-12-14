#
# Makefile
#

LIBCAMERAHOME = ..

OBJS	= cameramodule2.o \
	  cameradevice.o csi2cameradevice.o \
	  cameracontrol.o camerabuffer.o

libcamera.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(LIBCAMERAHOME)/Rules.mk

-include $(DEPS)
