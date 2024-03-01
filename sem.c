/*
Filename: ATEST17.QCSRC
The output of this example is as follows:
 Enter Testcase - LIBRARY/ATEST17
 Wait on semaphore to prevent access to shared data
 Create/start threads
 Wait a bit until we are 'done' with the shared data
 Thread 00000000 00000020: Entered
 Thread 00000000 0000001f: Entered
 Thread 00000000 0000001e: Entered
 Unlock shared data
 Wait for the threads to complete, and release their resources
 Thread 00000000 0000001f: Start critical section, holding semaphore
 Thread 00000000 0000001f: End critical section, release semaphore
 Thread 00000000 00000020: Start critical section, holding semaphore
 Thread 00000000 00000020: End critical section, release semaphore
 Thread 00000000 0000001e: Start critical section, holding semaphore
 Thread 00000000 0000001e: End critical section, release semaphore
 Clean up
 Main completed
*/
#define _MULTI_THREADED
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>

#define pthread_getthreadid_np pthread_self

#define checkResults(string, val) {             \
 if (val) {                                     \
   printf("Failed with %d at %s", val, string); \
   exit(1);                                     \
 }                                              \
}
 
#define                 NUMTHREADS   3
int                     sharedData=0;
int                     sharedData2=0;
 
/* Simple semaphore used here is actually a set of 1                  */
int                     semaphoreId=-1;
/* Simple lock operation. 0=which-semaphore, -1=decrement, 0=noflags  */
struct sembuf lockOperation = { 0, -1, 0};
/* Simple unlock operation. 0=which-semaphore, 1=increment, 0=noflags */
struct sembuf unlockOperation = { 0, 1, 0};
 
void *theThread(void *parm)
{
   int   rc;
   printf("Thread %.8lx: Entered\n", pthread_getthreadid_np());
   rc = semop(semaphoreId, &lockOperation, 1);
   checkResults("semop(lock)\n",rc);
   /********** Critical Section *******************/
   printf("Thread %.8lx: Start critical section, holding semaphore\n",
          pthread_getthreadid_np());
   /* Access to shared data goes here */
   ++sharedData; --sharedData2;
   printf("Thread %.8lx: End critical section, release semaphore\n",
          pthread_getthreadid_np());
   /********** Critical Section *******************/
   rc = semop(semaphoreId, &unlockOperation, 1);
   checkResults("semop(unlock)\n",rc);
   return NULL;
}
 
int main(int argc, char **argv)
{
  pthread_t             thread[NUMTHREADS];
  int                   rc=0;
  int                   i;
 
 
  printf("Enter Testcase - %s\n", argv[0]);
  /* Create a private semaphore set with 1 semaphore that only I can use */
  semaphoreId = semget(IPC_PRIVATE, 1, S_IRUSR|S_IWUSR);
  if (semaphoreId < 0) { printf("semget failed, err=%d\n",errno); exit(1); }
  /* Set the semaphore (#0 in the set) count to 1. Simulate a mutex */
  rc = semctl(semaphoreId, 0, SETVAL, (int)1);
  checkResults("semctl(SETALL)\n", rc);
 
  printf("Wait on semaphore to prevent access to shared data\n");
  rc = semop(semaphoreId, &lockOperation, 1);
  checkResults("semop(lock)\n",rc);
 
  printf("Create/start threads\n");
  for (i=0; i <NUMTHREADS; ++i) {
  rc = pthread_create(&thread[i], NULL, theThread, NULL);
  checkResults("pthread_create()\n", rc);
  }
 
  printf("Wait a bit until we are 'done' with the shared data\n");
  sleep(3);
  printf("Unlock shared data\n");
  rc = semop(semaphoreId, &unlockOperation, 1);
  checkResults("semop(unlock)\n",rc);
 
 
  printf("Wait for the threads to complete, and release their resources\n");
  for (i=0; i <NUMTHREADS; ++i) {
 rc = pthread_join(thread[i], NULL);
     checkResults("pthread_join()\n", rc);
  }
 
  printf("Clean up\n");
  rc = semctl(semaphoreId, 0, IPC_RMID);
  checkResults("semctl(removeID)\n", rc);
  printf("Main completed\n");
  return 0;
}
 
