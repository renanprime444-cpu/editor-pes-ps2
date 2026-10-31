EE_BIN = we_editor.elf
EE_OBJS = main.o core.o
EE_LIBS = -lpad -lmc -ldebug -lc -lkernel
EE_CFLAGS += -O2 -Wall

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
