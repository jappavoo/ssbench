SRCS := main.c sockserver.c msgserver.c net.c
OBJS := $(SRCS:%.c=%.o)
CFLAGS := -g -O3 -std=gnu99 -MD -MP
TARGETS := ssbench
TSCLOGREPO := git@github.com:jappavoo/tsclog.git

.PHONY: clean 

ext/tsclog/tsclogc.h:
	mkdir -p ext/tsclog && git clone ${TSCLOGREPO} ext/tsclog

all: ${TARGETS}

ssbench: ${OBJS}
	${CC} ${CFLAGS} -o $@ $^ -lpthread

clean:
	-rm -rf $(wildcard *.o *.d ${TARGETS} ext)


-include $(wildcard *.d)



