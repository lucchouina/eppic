#
# Copyright 2001 Silicon Graphics, Inc. All rights reserved.
#
#
# Makefile for LIBEPPIC
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
#
# Must be berkeley yacc.  Bison will not cut it
YACC = bison

LDIRT    = lex.eppic.c lex.eppicpp.c eppic.tab.c eppic.tab.h eppicpp.tab.c \
	eppicpp.tab.h y.output mkbaseop baseops.c y.tab.c y.tab.h \
	libeppic.so* *.output

LIBDIR	 = /usr/lib
TARGETS  = libeppic.a

# NOTE : -O2 crashes in the eppic_stats.c:eppic_dofor(). 
#        Disassembly shows invalid code for UNSETVS
CFLAGS += -g -O0 -fno-omit-frame-pointer -fPIC
ifeq ($(TARGET), PPC64)
	CFLAGS += -m64
endif
ifeq ($(TARGET), ARM)
	CFLAGS += -m32
endif
ifeq ($(TARGET), X86)
  CFLAGS += -m32
endif
ifeq ($(TARGET), MIPS)
       CFLAGS += -m32
endif

CFILES   = eppic_util.c eppic_node.c eppic_var.c eppic_func.c eppic_str.c \
	eppic_op.c eppic_num.c eppic_stat.c eppic_builtin.c eppic_type.c \
	eppic_case.c eppic_api.c eppic_member.c eppic_alloc.c eppic_define.c \
	eppic_input.c eppic_print.c

OFILES   = $(CFILES:.c=.o) eppicpp.tab.o eppic.tab.o lex.eppic.o lex.eppicpp.o \
	baseops.o

HFILES   = eppic.h eppic_api.h

LSOURCES = eppic-lsed eppicpp-lsed eppic.l eppicpp.l eppic.y eppicpp.y mkbaseop.c

all: default

showfiles:
	@echo $(RELDIR)/$(CFILES) $(RELDIR)/$(HFILES) $(RELDIR)/$(LSOURCES)

exports: all
	install $(TARGETS) $(ROOT)$(LIBDIR)

headers:
	install -m 644 $(HFILES) $(ROOT)/usr/include

install: headers exports
	(cd scripts ; $(MAKE) install )

baseops.o: mkbaseop.c
	gcc $(CFLAGS) -o mkbaseop mkbaseop.c
	./mkbaseop > baseops.c
	gcc $(CFLAGS) -c baseops.c

mkbaseop.c eppic_member.o eppic_op.o eppic_stat.o eppic_type.o y.tab.o : eppic.tab.h

lex.eppic.o: lex.eppic.c eppic.tab.c eppic.h
	gcc $(CFLAGS) -c lex.eppic.c

lex.eppic.c: eppic.l
	flex -L -Peppic -t eppic.l > lex.eppic.c

eppic.tab.c: eppic.tab.h

eppicpp.tab.o: eppicpp.tab.c
	gcc $(CFLAGS) -c eppicpp.tab.c

eppic.tab.o: eppic.tab.c
	gcc $(CFLAGS) -c eppic.tab.c

eppic.tab.h : eppic.y
	$(YACC) -peppic -v -t -d eppic.y

lex.eppicpp.o: lex.eppicpp.c eppicpp.tab.c eppic.h
	gcc $(CFLAGS) -c lex.eppicpp.c

lex.eppicpp.c: eppicpp.l
	flex -Peppicpp -t eppicpp.l  > lex.eppicpp.c

eppicpp.tab.c: eppicpp.tab.h eppic.tab.h

eppicpp.tab.h : eppicpp.y eppic.tab.h
	$(YACC) -peppicpp -v -t -d eppicpp.y

default: $(TARGETS)

# Explicit compilation rules for C files to use gcc instead of $(CC)
%.o: %.c $(HFILES) eppic.tab.h
	gcc $(CFLAGS) -c $< -o $@

$(TARGETS): $(OFILES)
	$(AR) cur $(TARGETS) $(OFILES)

check: all
	$(MAKE) -C tests check

clean: 
	-/bin/rm -f *.o $(TARGETS) $(LDIRT)
	-$(MAKE) -C tests clean

clobber: clean

.PHONY: all default check clean
