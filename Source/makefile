#
# Monolith 2 Copyright (C) 2017-2020 Jonas Mayr
# This file is part of Monolith.
#
# Monolith is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Monolith is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Monolith. If not, see <http://www.gnu.org/licenses/>.
#


# files

NAME   = Monolith
SOURCE = attack.cpp bench.cpp bit.cpp board.cpp eval.cpp magic.cpp main.cpp \
         misc.cpp move.cpp movegen.cpp movepick.cpp movesort.cpp polyglot.cpp \
         random.cpp search.cpp syzygy.cpp texel.cpp thread.cpp time.cpp \
         trans.cpp types.cpp uci.cpp zobrist.cpp

# basic flags

CFLAGS = -std=c++17 -O3
WFLAGS = -Wall -Wextra -Wpedantic
LFLAGS = -pthread -s

# targeted architecture

ifeq ($(ARCH),)
    CFLAGS += -march=native
    ARCH    = native
endif

ifeq ($(ARCH),x64)
    CFLAGS += -m64
endif

ifeq ($(ARCH),x64-popcnt)
    CFLAGS += -m64 -msse3 -mpopcnt -DPOPCNT
endif

ifeq ($(ARCH),x64-pext)
    CFLAGS += -m64 -msse3 -mpopcnt -DPOPCNT -mbmi2 -DPEXT
endif

ifeq ($(ARCH),x86)
    CFLAGS += -m32
endif

ifeq ($(ARCH),armv7)
    CFLAGS += -m32 -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 -mthumb
endif

ifeq ($(ARCH),armv8)
    CFLAGS += -m64 -march=armv8-a
endif

# targeted operating system

OS = $(shell uname -s)

ifneq ($(ARCH),native)
    NAME := $(NAME)-$(ARCH)
endif

ifeq ($(ARCH),$(filter $(ARCH),armv7 armv8))
    OS = Android
endif

ifeq ($(OS),Android)
    CFLAGS += -pie -fPIE -fno-strict-overflow -fno-strict-aliasing -ffunction-sections -fdata-sections -flto
    LFLAGS += -Wl,-pie -Wl,--gc-sections -static-libstdc++
endif

ifeq ($(OS),Linux)
    CFLAGS += -flto
endif

ifneq ($(OS),Android)
    ifeq ($(OS),Linux)
        RFLAGS += -static -lrt -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
    else
        CFLAGS += -static
        NAME   := $(NAME).exe
    endif
endif

# compiler specific additions

ifeq ($(COMP),)
    COMP = g++
endif

ifeq ($(COMP),icpc)
    WFLAGS += -pedantic -w3 -diag-disable=remark
endif

# targets

all:
	$(COMP) $(CFLAGS) $(WFLAGS) -DNDEBUG $(SOURCE) $(LFLAGS) -o $(NAME)

release:
	$(COMP) $(CFLAGS) $(WFLAGS) $(RFLAGS) -DNDEBUG $(SOURCE) $(LFLAGS) -o $(NAME)

debug:
	$(COMP) $(CFLAGS) $(WFLAGS) $(SOURCE) $(LFLAGS) -o $(NAME)

debug_deep:
	$(COMP) $(CFLAGS) $(WFLAGS) -DDEBUG_DEEP $(SOURCE) $(LFLAGS) -o $(NAME)

tune:
	$(COMP) $(CFLAGS) $(WFLAGS) -DNDEBUG -DTUNE $(SOURCE) $(LFLAGS) -o $(NAME)

help:
	@echo "make [ARCH=architecture] [COMP=compiler]"
	@echo "see readme.md for more details"
