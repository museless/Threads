# flags #
CC		= gcc
CFLAGS	= -g -Wall -std=c99 -pthread -D_DEFAULT_SOURCE

# elf #
EXTHS	= threads 

# obj #
OTHS	= source/threads.c source/example.c 

# phony #
.phony:	build

build	: $(EXTHS)

$(EXTHS) : $(OTHS)
	$(CC) -o $(EXTHS) $(OTHS) $(CFLAGS)
