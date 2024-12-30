CC				= gcc
CFLAGS			= -I $(INC_DIR)

INC_DIR 		= include
SRC_DIR			= src
OBJ_DIR 		= obj

EXAMPLES_DIR	= examples

PREFIX			= /usr

_DEPS			= qolqoi.h
DEPS			= $(patsubst %,$(INC_DIR)/%,$(_DEPS))

all: libqolqoi.so

obj/qolqoi.o: src/qolqoi.c ${DEPS}
	mkdir -p $(OBJ_DIR)
	$(CC) -c -o $@ $< $(CFLAGS)

libqolqoi.so: obj/qolqoi.o
	$(CC) -fPIC -shared -o $@ $^ $(CFLAGS) $(LIBS) $(DEFINES)

examples:
	$(CC) $(CFLAGS) $(EXAMPLES_DIR)/conv.c -o $(EXAMPLES_DIR)/conv -L. -lqolqoi -lpng

install: libqolqoi.so 
	mkdir -p $(PREFIX)/lib
	cp libqolqoi.so $(PREFIX)/lib/libqolqoi.so
	chmod +x $(PREFIX)/lib/libqolqoi.so
	cp $(INC_DIR)/qolqoi.h $(PREFIX)/include/qolqoi.h

uninstall:
	rm -f $(PREFIX)/lib/libqolqoi.so
	rm -f $(PREFIX)/include/qolqoi.h

clean:
	rm -f $(OBJ_DIR)/*.o 

.PHONY: clean examples install uninstall