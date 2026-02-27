##
## 2026, Akop Karapetyan
##

LIBRETRO_ROOT=libretro-common
RGBSERVER_ROOT=../rgbserver
INCLUDE=-I$(LIBRETRO_ROOT)/include -I$(RGBSERVER_ROOT)
DEFINE=
CFLAGS=-Wall -O2 `sdl2-config --cflags`
CXXFLAGS=$(CFLAGS) 
LDFLAGS=`sdl2-config --libs`
EXE=main

all: $(EXE)

$(EXE): main.o sound_queue.o $(RGBSERVER_ROOT)/rgbserver.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(INCLUDE) $(DEFINE) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(INCLUDE) $(DEFINE) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf *.o $(EXE)
