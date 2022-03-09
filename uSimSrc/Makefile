HDRS=	asm.h		\
	bdev.h		\
	cdev.h		\
	clock.h		\
	console.h	\
	cpu.h		\
	dasm.h		\
	ddt.h		\
	digits.h	\
	dir.h		\
	file.h		\
	hostfile.h	\
	memory.h	\
	monitor.h	\
	op.h		\
	opcb.h		\
	opdd.h		\
	opddcb.h	\
	oped.h		\
	opfd.h		\
	opfdcb.h	\
	pstring.h	\
	ring.h		\
	search.h	\
	sort.h		\
	ustdio.h		\
	string.h	\
	system.h

OBJS=	asm.o		\
	bdev.o		\
	cdev.o		\
	clock.o		\
	console.o	\
	cpu.o		\
	dasm.o		\
	ddt.o		\
	digits.o	\
	dir.o		\
	file.o		\
  gets.o \
	hostfile.o	\
	main.o		\
	memory.o	\
	monitor.o	\
  printf.o \
	pstring.o	\
	ring.o		\
	search.o	\
	sort.o		\
	stack.o		\
	stdio.o		\
	string.o	\
	system.o

CFLAGS=	-g -U COLOR -DZ80 -DBIG_ENDIAN -DTERMIOS -DADM31 -DBSD
CRLIB=	-ltermcap

uSim:	$(HDRS) $(OBJS)
	$(CC) $(OBJS) $(CRLIB) -o uSim

clean:
	rm -f $(OBJS)
