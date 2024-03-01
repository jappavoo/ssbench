SRCS := main.c sockserver.c queue.c funcserver.c net.c hexdump.c
OBJS := $(SRCS:%.c=%.o) ext/tsclog/tsclog.o
O:=0
CFLAGS += -g -O${O} -std=gnu99 -MD -MP
clinesize=$(shell cat /sys/devices/system/cpu/cpu0/cache/*/coherency_line_size | head -1)
CFLAGS += -D COHERENCY_LINE_SIZE=${clinesize} 
TARGETS := ssbench
TSCLOGREPO := git@github.com:jappavoo/tsclog.git
UTHASHDIR = ext/uthash
UTHASHINCS = ${UTHASHDIR}/include
CFLAGS += -I${UTHASHINCS}
EXTFILES := ext/tsclog/tsclogc.h ${UTHASHINCS}/uthash.h

.PHONY: clean 

all: ${EXTFILES}  ${TARGETS} 

ssbench: ${OBJS}
	${CC} ${CFLAGS} -o $@ $^ -lpthread

ext/tsclog/tsclog.o: ext/tsclog/tsclogc.h ext/tsclog/tsclog.c
	make -C ext/tsclog tsclog.o

ext/tsclog/tsclogc.h:
	-mkdir -p ext/tsclog && git clone ${TSCLOGREPO} ext/tsclog

${UTHASHINCS}/uthash.h:
	-mkdir -p ext && git clone https://github.com/troydhanson/uthash.git ${UTHASHDIR}

clean:
	-rm -rf $(wildcard *.o *.d ${TARGETS} ext)


-include $(wildcard *.d)



