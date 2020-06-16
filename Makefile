CC=gcc
CFLAGS=-O3
BIN = bsf
SRC = bsf.c
OBJ = $(SRC:.c=.o) 

all: $(BIN)

debug: CCFLAGS += -DDEBUG
debug: $(BIN)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(BIN) *.o   
