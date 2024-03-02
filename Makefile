SRCS := main.c sockserver.c queue.c funcserver.c func.c net.c hexdump.c
OBJS := $(SRCS:%.c=%.o) ext/tsclog/tsclog.o
FUNCSRCS := stdout.c
FUNCSOS := $(FUNCSRCS:%.c=%.so) 
O:=0
CFLAGS += -g -O${O} -std=gnu99 -MD -MP
clinesize=$(shell cat /sys/devices/system/cpu/cpu0/cache/*/coherency_line_size | head -1)
CFLAGS += -D COHERENCY_LINE_SIZE=${clinesize} 
TARGETS := ssbench
TSCLOGREPO := git@github.com:jappavoo/tsclog.git
UTHASHREPO := git@github.com:troydhanson/uthash.git
UTHASHDIR = ext/uthash
UTHASHINCS = ${UTHASHDIR}/include
CFLAGS += -I${UTHASHINCS}
EXTFILES := ext/tsclog/tsclogc.h ${UTHASHINCS}/uthash.h

.PHONY: clean 

%.so: %.c
	${CC} ${CFLAGS} -fPIC -shared $< -o $@

all: ${EXTFILES}  ${TARGETS} ${FUNCSOS}

ssbench: ${OBJS}
	${CC} ${CFLAGS} -o $@ $^ -lpthread

ext/tsclog/tsclog.o: ext/tsclog/tsclogc.h ext/tsclog/tsclog.c
	make -C ext/tsclog tsclog.o

ext/tsclog/tsclogc.h:
	-mkdir -p ext/tsclog && git clone ${TSCLOGREPO} ext/tsclog

${UTHASHINCS}/uthash.h:
	-mkdir -p ext && git clone ${UTHASHREPO} ${UTHASHDIR}

clean:
	-rm -rf $(wildcard *.o *.d ${TARGETS} *.so ext)


-include $(wildcard *.d)



