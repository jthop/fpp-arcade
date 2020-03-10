include /opt/fpp/src/makefiles/common/setup.mk
include /opt/fpp/src/makefiles/platform/*.mk

all: libfpp-arcade.so
debug: all

CFLAGS+=-I.
OBJECTS_fpp_arcade_so += src/FPPArcade.o src/FPPTetris.o src/FPPPong.o src/FPPSnake.o
LIBS_fpp_arcade_so += -L/opt/fpp/src -lfpp
CXXFLAGS_src/FPPArcade.o += -I/opt/fpp/src


%.o: %.cpp Makefile
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

libfpp-arcade.so: $(OBJECTS_fpp_arcade_so) /opt/fpp/src/libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_arcade_so) $(LIBS_fpp_arcade_so) $(LDFLAGS) -o $@

clean:
	rm -f libfpp-arcade.so $(OBJECTS_fpp_arcade_so)

