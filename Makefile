SRCS := main.c sockserver.c opserver.c net.c hexdump.c
OBJS := $(SRCS:%.c=%.o)
O:=0
CFLAGS := -g -O${O} -std=gnu99 -MD -MP
TARGETS := ssbench
TSCLOGREPO := git@github.com:jappavoo/tsclog.git

.PHONY: clean 

all: ${TARGETS}

ssbench: ${OBJS}
	${CC} ${CFLAGS} -o $@ $^ -lpthread


ext/tsclog/tsclogc.h:
	mkdir -p ext/tsclog && git clone ${TSCLOGREPO} ext/tsclog

clean:
	-rm -rf $(wildcard *.o *.d ${TARGETS} ext)


-include $(wildcard *.d)



