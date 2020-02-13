/*
 * File: main.c
 *
 * This file is part of the SimuSil library
 *
 * Compile: $ gcc -Wall -I./include -pthread -L./lib 3_Parallel.c
 *                -lsimusil -lrt -o 3_Parallel
 *
 * Author: Sergio Romero Montiel <sromero@uma.es>
 *
 * Created on October 27th, 2016
 * Modified 2016-11-01: added list of thread to be canceled
 * Modified 2016-11-02: added debug levels
 */

#include <stdio.h>  /* printf(3)                                      */
#include <stdlib.h> /* exit(3), EXIT_SUCCESS                          */
#include <signal.h> /* signal(2), SIGINT, SIG_DFL                     */
#include <time.h>   /* clock_nanosleep(2)                             */
#include <pthread.h>/* pthread stuff (_create,_exit,_setdettachstate) */
#include <semaphore.h>
#include "simusil.h"

#define DELTATIME 10000000 /* 100ns */
#define NSECS_PER_SEC 1000000000UL
#define DIF_TS(a,b) ((a.tv_sec-b.tv_sec)*NSECS_PER_SEC+(a.tv_nsec-b.tv_nsec))
#define timeAdd(a,b)                  \
  do {                                \
    a.tv_sec+=b.tv_sec;               \
    a.tv_nsec+=b.tv_nsec;             \
    if (a.tv_nsec >= NSECS_PER_SEC) { \
      a.tv_sec++;                     \
      a.tv_nsec-=NSECS_PER_SEC;       \
    }                                 \
  } while(0)

/* WORKER STUFF                                                       */
/* struct to pass all info to thread                                  */
typedef struct{
  int id;
  pthread_t thid;
  Radar_ptr_t   r;
  Cannon_ptr_t  c;
  Missile_ptr_t m;
  struct timespec deadline; 
  sem_t semMissile;
} Args_t;

/* GLOBALs: needed by SIGINT handlers                                 */
World_ptr_t w;   /* to be destroyed at exit                           */
Bomber_ptr_t b;  /* start/stop bombing                                */
List_ptr_t l;    /* list of living threads                            */
pthread_attr_t attr;

void destroyWorker(void *arg)
{
  Args_t *x=arg;
  pthread_cancel(x->thid);
  free(x);
}

void destroyer(int signum)
{
  signal(SIGINT,SIG_DFL); /* restore default-TERM during destroyWorld */
  destroyList(l,destroyWorker);
  pthread_attr_destroy(&attr);
  destroyWorld(w);
  exit(EXIT_SUCCESS);
}

void handler(int signum)
{
  stopBombing(b);
  printf("Press ctrl+C to finish\n"); /* bad idea: printf in handler! */
  signal(SIGINT,destroyer);
}

/*  We need:        
*     The new Elem is inserted between two elements
*     elem_A and elem_B, where:
*       elem_A->next == elem_B (AND elem_B->prev == elem_A)
*         cmp(elem_B,new) => 1
*         cmp(elem_A,new) => 0
*       giving:
*         elem_A->next == new; new->next == elemB
*         elem_B->prev == new; new->prev == elemA
*/
int compareDeadline(void *arg1, void *arg2)
{ 
  
  Args_t *listMissile=arg1;
  Args_t *newMissile=arg2;
  
  if (newMissile->deadline.tv_sec < listMissile->deadline.tv_sec)  
  {
    return 1;
  }
  else if (newMissile->deadline.tv_sec  > listMissile->deadline.tv_sec)  
  {
    return 0;
  } 
  else if (newMissile->deadline.tv_nsec < listMissile->deadline.tv_nsec) 
  {
    return 1;
  }
  else
  { 
    return 0;
  }
}

/* thread code */
void *searchAndDestroy(void *arg)
{
  Args_t *x=arg;
  Args_t *nextMissile;
  MissileState sm;
  Pos p0, p1;
  struct timespec startThread, deadline;
  const struct timespec deltaTime=(struct timespec){0, DELTATIME};/* 100ns*/
  const struct timespec stallTime=(struct timespec){0, 1000000};/* 1ms*/
  const struct timespec relaxTime=(struct timespec){0,10000000};/*10ms*/
  unsigned long timeRemaining;
  
  clock_gettime(CLOCK_MONOTONIC, &startThread);
  sm=radarReadMissile(x->r,x->m,&p0);

  timeAdd(startThread, deltaTime); /*10ns later*/
  clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&startThread,NULL);

  sm=radarReadMissile(x->r,x->m,&p1);

  timeRemaining = (p1.y / (p0.y-p1.y)) * DELTATIME;
  printf("[%03d] Deadline = %li ns\n",x->id,timeRemaining);
  deadline=(struct timespec){0, timeRemaining};
  timeAdd(deadline, startThread);
  x->deadline = deadline;
  
  list_insert(x,compareDeadline,x->id,l);

  printf("+ [%03d] Waiting cannon!\n",x->id);
  sem_wait(&x->semMissile);
  printf("+ [%03d] I have cannon!\n",x->id);
  sm=radarReadMissile(x->r,x->m,&p1);
  if (sm != MISSILE_ACTIVE)
  {
    printf("[%03d] Warning: missing missile!\n",x->id);
    printf("[%03d] \tBetween Wait & Read:\n",x->id);
    printf("[%03d] \t\tMissile impacted on ground, or\n",x->id);
    printf("[%03d] \t\tMissile intercepted by a spurious previous shoot\n",x->id);
  }
  else
  {
    printf("[%03d] ---> Moving cannon to position %d\n",x->id,p0.x);
    cannonMove(x->c,p0.x);
    clock_nanosleep(CLOCK_MONOTONIC,0,&stallTime,NULL); /*espera antes*/
    cannonFire(x->c);
  }

  list_remove(x,l);

  printf("+ [%03d] Waiting next missile!\n",x->id);
  nextMissile=list_dequeue(l,1);  // if wait==1, waits until thereis an element
  printf("+ [%03d] Next missile: %03d\n",x->id,nextMissile->id);
  sem_post(&nextMissile->semMissile);

  if (sm == MISSILE_ACTIVE)
  {  
    /* bucle de monitorizacion hasta intercepcion */
    while ((sm=radarReadMissile(x->r,x->m,&p0)) == MISSILE_ACTIVE)
    {
	  //printf("[%03d] ---> en seguimiento (%d,%d)\n",x->id,p.x,p.y);
      clock_nanosleep(CLOCK_MONOTONIC,0,&relaxTime,NULL);
    }
    switch (sm)
    {
      case MISSILE_INTERCEPTED:
           printf("[%03d] ---> Interceptado en (%d,%d)\n",x->id,p0.x,p0.y);
           break;
      case MISSILE_IMPACTED:
           printf("[%03d] ---> Impacta en suelo (%d)\n",x->id,p0.x);
           break;
      case MISSILE_ERROR:
      default:
           printf("[%03d] ---> Error de seguimiento del misil\n",x->id);
    }
  }

  free(x);
  pthread_exit(NULL);
}

Args_t * newMissile(int id, Radar_ptr_t r, Cannon_ptr_t c)
{
  Args_t *x;

  x=(Args_t*)malloc(sizeof(Args_t));
  x->id=id;
  x->r=r;
  x->c=c;
  sem_init(&x->semMissile,0,0);
  x->m=radarWaitMissile(r);

  return x;
}

/*
 * Main code
 *
 * Master thread: wait missile and creates dettached worker for it
 */
int main(int argc, char *argv[])
{
  Radar_ptr_t r;
  Cannon_ptr_t c;
  Args_t *x;
  static int workerCount=0;

  debug_setlevel(1);

  w=createWorld("TRSM 2016",1,2); /* worldname,1 cannon,debug level 2 */
  b=getBomber(w);
  r=getRadar(w);
  c=getCannon(w,0); /* [0..n-1] cannon number 0 (first of one)        */
  l=createList("Threads","worker",2); /* listname,elemname,debuglevel */
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
  
  signal(SIGINT,handler);

  printf("Press ctrl+C to stop bombing\n");
  startBombing(b);

  x = newMissile(workerCount++, r, c);
  sem_post(&x->semMissile); // The first missile can continue
  pthread_create(&x->thid,&attr,searchAndDestroy,(void*)x);

  while(1)
  {
    x = newMissile(workerCount++, r, c);

    pthread_create(&x->thid,&attr,searchAndDestroy,(void*)x);
  }
  return 0; /* never reached!                                         */
}
