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
LIBRETRO_ROOT=libretro-common
RGBSERVER_ROOT=../rgbserver
INCLUDE=-I$(LIBRETRO_ROOT)/include -I$(MINIZIP_ROOT) -I$(RGBSERVER_ROOT)
DEFINE=
CFLAGS=-Wall -O2 `sdl2-config --cflags`
CXXFLAGS=$(CFLAGS) 
LDFLAGS=`sdl2-config --libs` -lz
EXE=rh

all: $(EXE)

$(EXE): kv_store.o args.o files.o sound_queue.o video.o main.o \
	$(MINIZIP_ROOT)/unzip.o $(MINIZIP_ROOT)/ioapi.o \
	$(RGBSERVER_ROOT)/rgbserver.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(INCLUDE) $(DEFINE) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(INCLUDE) $(DEFINE) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf *.o $(EXE)
