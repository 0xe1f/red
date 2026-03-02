##
## 2026, Akop Karapetyan
##

MINIZIP_ROOT=minizip
LIBRETRO_ROOT=libretro-common
RGBSERVER_ROOT=../rgbserver
INCLUDE=-I$(LIBRETRO_ROOT)/include -I$(MINIZIP_ROOT) -I$(RGBSERVER_ROOT)
DEFINE=
CFLAGS=-Wall -O2 `sdl2-config --cflags`
CXXFLAGS=$(CFLAGS) 
LDFLAGS=`sdl2-config --libs` -lz
EXE=main

all: $(EXE)

$(EXE): args.o files.o main.o sound_queue.o \
	$(MINIZIP_ROOT)/unzip.o $(MINIZIP_ROOT)/ioapi.o \
	$(RGBSERVER_ROOT)/rgbserver.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(INCLUDE) $(DEFINE) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(INCLUDE) $(DEFINE) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf *.o $(EXE)
