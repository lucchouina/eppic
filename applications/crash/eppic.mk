#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

ifeq ($(TARGET), PPC64)
        TARGET_FLAGS = -D$(TARGET) -m64
else
        TARGET_FLAGS = -D$(TARGET)
endif

all:
	@if [ -f /usr/bin/flex ] && [ -f /usr/bin/bison ]; then \
	  if [ -f ../$(GDB)/crash.target ]; then \
	    make -f eppic.mk eppic.so; else \
	  echo "eppic.so: build failed: requires the crash $(GDB) module"; fi \
	else \
	  echo "eppic.so: build failed: requires /usr/bin/flex and /usr/bin/bison"; fi

lib-eppic: 
	cd libeppic && make
        
eppic.so: ../defs.h eppic.c lib-eppic
	gcc -g -Ilibeppic -I../$(GDB)/bfd -I../$(GDB)/include -I../$(GDB)/gdb -I../$(GDB)/gdb/config -I../$(GDB)/gdb/common -I../$(GDB) -nostartfiles -shared -rdynamic -o eppic.so eppic.c -fPIC $(TARGET_FLAGS) $(GDB_FLAGS) -Llibeppic -leppic 

clean:
	cd libeppic && make clean
