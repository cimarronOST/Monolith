#
# Monolith Copyright (C) 2017-2026 Jonas Mayr
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
SOURCE = attack.cpp bench.cpp bit.cpp board.cpp eval.cpp history.cpp \
         magic.cpp main.cpp misc.cpp move.cpp movegen.cpp movepick.cpp \
         movesort.cpp score.cpp search.cpp syzygy.cpp thread.cpp \
         time.cpp trans.cpp tune.cpp types.cpp uci.cpp zobrist.cpp
     
# basic flags

CFLAGS  = -std=c++23 -O3 -pthread -Wall -Wextra -Wpedantic
RFLAGS  = -DNDEBUG
MFLAGS  =

# targeted architecture

ifeq ($(ARCH),)
    ARCH    = native
    CFLAGS += -march=native
endif

ifeq ($(ARCH),x86-64)
    CFLAGS += -m64
    MFLAGS += -target x86_64-apple-macos11.0
endif

ifeq ($(ARCH),x86-64-pext)
    CFLAGS += -m64 -msse3 -mpopcnt -mbmi2 -DPEXT
    MFLAGS += -target x86_64-apple-macos11.0
endif

ifeq ($(ARCH),armv64)
    MFLAGS += -target arm64-apple-macos11.0
endif

ifeq ($(ARCH),armv7)
    CFLAGS += -m32 -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 -mthumb
endif

ifeq ($(ARCH),armv8)
    CFLAGS += -m64 -march=armv8-a
endif

# determining operating system

ifneq ($(OS),Windows_NT) 
    OS = $(shell uname -s)
endif

ifeq ($(ARCH),$(filter $(ARCH),armv7 armv8))
    OS = Android
endif

# if no compiler is specified, g++ is used except on MacOS

ifeq ($(COMP),)
    ifeq ($(OS),Darwin)
        COMP = clang++
    else
        COMP = g++
    endif
endif

# adding flags depending on the operating system

ifeq ($(OS),Android)
    CFLAGS += -pie -fPIE -fno-strict-overflow -fno-strict-aliasing -ffunction-sections -fdata-sections -flto
    CFLAGS += -Wl,-pie -Wl,--gc-sections -static-libstdc++
endif

ifeq ($(OS),Linux)
    CFLAGS += -flto=auto
    RFLAGS += -static
endif

ifeq ($(OS),Darwin)
    CFLAGS += -flto=auto
    RFLAGS += $(MFLAGS)
endif

ifeq ($(OS),Windows_NT) 
    RFLAGS += -static
    NAME   += $(NAME).exe
endif

# targets

all:
	$(COMP) $(CFLAGS) -DNDEBUG $(SOURCE) -o $(NAME)

release:
	$(COMP) $(CFLAGS) $(RFLAGS) $(SOURCE) -o $(NAME)

debug:
	$(COMP) $(CFLAGS) $(SOURCE) -o $(NAME)

help:
	@echo "make [ARCH=architecture] [COMP=compiler]"
	@echo "see readme.md for more details"
