SRCS := main.c sockserver.c msgserver.c net.c
OBJS := $(SRCS:%.c=%.o)
CFLAGS := -g -O3  -MD -MP
TARGETS := ssbench 
.PHONY: clean 

all: ${TARGETS}

ssbench: ${OBJS}
	${CC} ${CFLAGS} -o $@ $^ -lpthread

clean:
	-rm -rf $(wildcard *.o *.d)


-include $(wildcard *.d)



