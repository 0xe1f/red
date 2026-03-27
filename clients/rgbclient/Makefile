##
## RGBClient
## 2024, Akop Karapetyan
##

GEN_DIR=generated
PROTO_DIR=proto
RGB_ROOT=rpi-rgb-led-matrix
RGB_LIBDIR=$(RGB_ROOT)/lib

INCLUDE=\
	-I$(RGB_ROOT)/include \
	-Iinclude \
	-I/usr/include/nats/ \
	-I$(GEN_DIR)
LIB=-L$(RGB_LIBDIR)
DEFINE=
CFLAGS=-Wall -O2
CXXFLAGS=$(CFLAGS)
LDFLAGS=$(LIB) -lrgbmatrix -lnats -lprotobuf-c
EXE=rgbclient

PROTO_FILES = $(wildcard $(PROTO_DIR)/*.proto)
PB_O_FILES = $(patsubst $(PROTO_DIR)/%.proto, $(GEN_DIR)/%.pb-c.o, $(PROTO_FILES))

all: $(EXE)

$(RGB_LIBDIR)/librgbmatrix.a:
	@echo Building RGB matrix libraries...
	@cd $(RGB_LIBDIR); $(MAKE)

$(EXE): $(PB_O_FILES) xm.o log.o $(EXE).o $(RGB_LIBDIR)/librgbmatrix.a
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(GEN_DIR)/%.pb-c.o: $(PROTO_DIR)/%.pb-c.c
	$(CC) $(CFLAGS) -c $< -o $@

$(GEN_DIR)/%.pb-c.c: $(PROTO_DIR)/%.proto | $(GEN_DIR)
	protoc-c -I$(PROTO_DIR) --c_out=$(GEN_DIR) $(notdir $<)

$(GEN_DIR):
	mkdir -p $@

%.o: %.c
	$(CC) $(INCLUDE) $(DEFINE) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(INCLUDE) $(DEFINE) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf *.o $(EXE) $(GEN_DIR)
	@cd $(RGB_LIBDIR); $(MAKE) clean
