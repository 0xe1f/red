## Copyright (c) 2024 Akop Karapetyan
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##    http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.

MINIZIP_ROOT=minizip
LIB7Z_ROOT=lib7z
UN7Z_ROOT=un7z
LIBRETRO_ROOT=libretro-common
RGBSERVER_ROOT=../rgbserver
INCLUDE=-I$(LIBRETRO_ROOT)/include \
	-I$(MINIZIP_ROOT) \
	-I$(LIB7Z_ROOT) \
	-I$(UN7Z_ROOT) \
	-I$(RGBSERVER_ROOT)
DEFINE=
CFLAGS=-Wall -O2 `sdl2-config --cflags`
CXXFLAGS=$(CFLAGS) 
LDFLAGS=`sdl2-config --libs` -lz
EXE=rh

all: $(EXE)

$(EXE): kv_store.o args.o files.o sound_queue.o video.o main.o \
	$(MINIZIP_ROOT)/unzip.o $(MINIZIP_ROOT)/ioapi.o \
	$(UN7Z_ROOT)/un7z.o \
	$(LIB7Z_ROOT)/7zStream.o $(LIB7Z_ROOT)/7zCrc.o $(LIB7Z_ROOT)/7zArcIn.o \
	$(LIB7Z_ROOT)/7zBuf.o $(LIB7Z_ROOT)/7zDec.o $(LIB7Z_ROOT)/CpuArch.o \
	$(LIB7Z_ROOT)/LzmaDec.o $(LIB7Z_ROOT)/Lzma2Dec.o $(LIB7Z_ROOT)/Bcj2.o \
	$(LIB7Z_ROOT)/7zCrcOpt.o $(LIB7Z_ROOT)/Delta.o \
	$(LIB7Z_ROOT)/Bra.o $(LIB7Z_ROOT)/Bra86.o $(LIB7Z_ROOT)/BraIA64.o \
	$(RGBSERVER_ROOT)/rgbserver.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(INCLUDE) $(DEFINE) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(INCLUDE) $(DEFINE) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf *.o $(EXE)
