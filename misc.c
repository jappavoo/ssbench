#include <sys/ipc.h>
#include <sys/sem.h>
#include <fcntl.h>


// counting semaphore operations wait and signal
struct sembuf semwait = { 0, -1, 0};
struct sembuf semsignal = { 0, 1, 0};
