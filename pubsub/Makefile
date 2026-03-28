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

GEN_DIR=generated
PROTO_DIR=proto
TP_ROOT=3p
RGB_ROOT=$(TP_ROOT)/rpi-rgb-led-matrix
LIB7Z_ROOT=$(TP_ROOT)/lib7z
UN7Z_ROOT=$(TP_ROOT)/un7z
MINIZIP_ROOT=$(TP_ROOT)/minizip
RGB_LIBDIR=$(RGB_ROOT)/lib

INCLUDE=\
	-I$(RGB_ROOT)/include \
	-I$(GEN_DIR) \
	-I$(TP_ROOT)/arena \
	-I$(MINIZIP_ROOT) \
	-I$(LIB7Z_ROOT) \
	-I$(UN7Z_ROOT) \
	-I$(TP_ROOT)/libretro \
	`sdl2-config --cflags`
DEFINE=
CFLAGS=-Wall -O2
CXXFLAGS=$(CFLAGS)
LD_FLAGS=$(LIB) -lnats -lprotobuf-c
LD_SUB_FLAGS=$(LD_FLAGS) -L$(RGB_LIBDIR) -lrgbmatrix
LD_PUB_FLAGS=$(LD_FLAGS) `sdl2-config --libs` -lz
SUB_EXE=sub
PUB_EXE=pub

PROTO_FILES=geometry.proto frame.proto
PB_OBJECTS=$(patsubst %.proto, %.pb-c.o, $(PROTO_FILES))
Z7_OBJECTS=$(UN7Z_ROOT)/un7z.o \
	$(LIB7Z_ROOT)/7zStream.o $(LIB7Z_ROOT)/7zCrc.o $(LIB7Z_ROOT)/7zArcIn.o \
	$(LIB7Z_ROOT)/7zBuf.o $(LIB7Z_ROOT)/7zDec.o $(LIB7Z_ROOT)/CpuArch.o \
	$(LIB7Z_ROOT)/LzmaDec.o $(LIB7Z_ROOT)/Lzma2Dec.o $(LIB7Z_ROOT)/Bcj2.o \
	$(LIB7Z_ROOT)/7zCrcOpt.o $(LIB7Z_ROOT)/Delta.o \
	$(LIB7Z_ROOT)/Bra.o $(LIB7Z_ROOT)/Bra86.o $(LIB7Z_ROOT)/BraIA64.o
MINIZIP_OBJECTS=$(MINIZIP_ROOT)/unzip.o $(MINIZIP_ROOT)/ioapi.o

all: $(SUB_EXE) $(PUB_EXE)

$(RGB_LIBDIR)/librgbmatrix.a:
	@echo Building RGB matrix libraries...
	@cd $(RGB_LIBDIR); $(MAKE)

$(SUB_EXE): $(PB_OBJECTS) xm_sub.o log.o $(SUB_EXE).o $(RGB_LIBDIR)/librgbmatrix.a
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LD_SUB_FLAGS)

$(PUB_EXE): \
	$(PB_OBJECTS) $(MINIZIP_OBJECTS) $(Z7_OBJECTS) \
	kv_store.o args.o files.o video.o input.o audio.o log.o timing.o xm_pub.o $(PUB_EXE).o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LD_PUB_FLAGS)

%.pb-c.o: $(GEN_DIR)/%.pb-c.c
	$(CC) $(CFLAGS) -c $< -o $@

$(GEN_DIR)/%.pb-c.c: $(PROTO_DIR)/%.proto | $(GEN_DIR)
	protoc-c -I$(PROTO_DIR) --c_out=$| $(notdir $<)

$(GEN_DIR):
	mkdir -p $@

%.o: %.c
	$(CC) $(INCLUDE) $(DEFINE) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(INCLUDE) $(DEFINE) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf *.o $(SUB_EXE) $(PUB_EXE) $(GEN_DIR) `find $(TP_ROOT)/ -name *.o`
	@cd $(RGB_LIBDIR); $(MAKE) clean
