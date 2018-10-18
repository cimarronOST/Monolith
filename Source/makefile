#
# Monolith 1.0  Copyright (C) 2017 Jonas Mayr
#
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

EXE     = Monolith
APPEND  =
SOURCE := $(wildcard *.cpp)

# flags

MACROS   =
WFLAGS   = -Wall -Wextra
CXXFLAGS = -s -O2
LDFLAGS  = -pthread

# operating system

ifneq ($(OS),Windows_NT)
    OS = $(shell uname -s)
    ifeq ($(OS),Linux)
        CXXFLAGS += -flto
    endif
endif

# architecture

ifeq ($(ARCH),x64-pext)
    APPEND    = _x64_pext
    CXXFLAGS += -m64
    MACROS   += -D PEXT -D POPCNT
endif

ifeq ($(ARCH),x64-popcnt)
    APPEND    = _x64_popcnt
    CXXFLAGS += -m64
    MACROS   += -D POPCNT
endif

ifeq ($(ARCH),x64)
    APPEND    = _x64
    CXXFLAGS += -m64
endif

ifeq ($(ARCH),x86)
    APPEND    = _x86
    CXXFLAGS += -m32
endif

ifeq ($(ARCH),android)
	APPEND    = _android
	CXXFLAGS +=
endif

# compiler

ifeq ($(COMP),)
    COMP = clang
endif

ifeq ($(COMP),clang)
    CXX       = clang++
    CXXFLAGS += -std=c++11
    WFLAGS   += -Wpedantic
endif

ifeq ($(COMP),gcc)
    CXX       = g++
    CXXFLAGS += -std=c++11
    WFLAGS   += -Wpedantic
endif

ifeq ($(COMP),icc)
	CXX       = icpc
    CXXFLAGS += -std=c++14
	WFLAGS   += -pedantic -w3 -diag-disable=remark
endif

# targets

all: release

release:
	$(CXX) $(CXXFLAGS) $(WFLAGS) $(MACROS) -D NDEBUG $(SOURCE) $(LDFLAGS) -o $(EXE)$(APPEND)

release_log:
	$(CXX) $(CXXFLAGS) $(WFLAGS) $(MACROS) -D NDEBUG -D LOG $(SOURCE) $(LDFLAGS) -o $(EXE)$(APPEND)

tune:
	$(CXX) $(CXXFLAGS) $(WFLAGS) $(MACROS) -D NDEBUG -D TUNE $(SOURCE) $(LDFLAGS) -o $(EXE)$(APPEND)

debug_assert:
	$(CXX) $(CXXFLAGS) $(WFLAGS) $(MACROS) $(SOURCE) $(LDFLAGS) -o $(EXE)

debug_exp:
	$(CXX) $(CXXFLAGS) $(WFLAGS) $(MACROS) -D DEBUG_EXP $(SOURCE) $(LDFLAGS) -o $(EXE)

help:
	@echo "make [target] [ARCH=architecture] [COMP=compiler]"
	@echo "see readme.md for more details"
