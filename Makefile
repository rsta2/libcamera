#
# Makefile
#

.NOTPARALLEL:

CIRCLEHOME = circle

include $(CIRCLEHOME)/Config.mk
-include $(CIRCLEHOME)/Config2.mk

all:
	cd $(CIRCLEHOME) && ./makeall --nosample
	make -C lib

clean:
	cd $(CIRCLEHOME) && ./makeall --nosample clean
	make -C lib clean
