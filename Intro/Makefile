CC	=	gcc
CFLAGS	=	-Wall
DEPS	=	defines.h
TARGETS = writenoncanonical noncanonical

#make -B all

#correr make all para compilar
all:	writenoncanonical	noncanonical

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

writenoncanonical:	$(OBJ)
	$(CC)	$(CFLAGS)	-o	$@	$@.c	$(OBJ)	-lm

noncanonical:	$(OBJ)
	$(CC)	$(CFLAGS)	-o	$@	$@.c	$(OBJ)	-lm

#correr make clean para apagar
clean:
	@rm $(TARGETS)
