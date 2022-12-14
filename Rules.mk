#
# Rules.mk
#

LIBCAMERAHOME ?= ../..

-include $(LIBCAMERAHOME)/Config.mk

CIRCLEHOME ?= $(LIBCAMERAHOME)/circle

INCLUDE += -I $(LIBCAMERAHOME)/include

include $(CIRCLEHOME)/Rules.mk
