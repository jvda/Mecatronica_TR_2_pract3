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
#include "simusil.h"

#define DELTATIME 10000 /* 10ns */
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
} Args_t;

/* GLOBALs: needed by SIGINT handlers                                 */
World_ptr_t w;   /* to be destroyed at exit                           */
Bomber_ptr_t b;  /* start/stop bombing                                */
List_ptr_t l;    /* list of living threads                            */
pthread_attr_t attr;
pthread_mutex_t mutex_canon;

struct timespec startMain; /* Init main */

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


/* thread code */
void *searchAndDestroy(void *arg)
{
  Args_t *x=arg;
  MissileState sm;
  Pos p0, p1;
  struct timespec startThread, deadline;
  const struct timespec deltaTime=(struct timespec){0, 10};/* 10ns*/
  const struct timespec stallTime=(struct timespec){0, 1000000};/* 1ms*/
  const struct timespec relaxTime=(struct timespec){0,10000000};/*10ms*/

  list_enqueue(x,x->id,l);

  clock_gettime(CLOCK_MONOTONIC, &startThread);
  sm=radarReadMissile(x->r,x->m,&p0);

  if (sm != MISSILE_ACTIVE)
  {
    printf("[%03d] Warning: missing missile!\n",x->id);
    printf("[%03d] \tBetween Wait & Read:\n",x->id);
    printf("[%03d] \t\tMissile impacted on ground, or\n",x->id);
    printf("[%03d] \t\tMissile intercepted by a spurious previous shoot\n",x->id);
  }
  else
  {
    timeAdd(startThread, deltaTime); /*10us later*/
    clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&startThread,NULL);

    sm=radarReadMissile(x->r,x->m,&p1);
    
  deadline.tv_nsec=(long)0;
  deadline.tv_sec=-(time_t)((double)p1.y/(NSECS_PER_SEC*(p0.y-p1.y/(DELTATIME))));

    //timeAdd(deadline, startMain);
    x->deadline = deadline;

    printf("[%03d] ---> Deadline = %lld.%.9ld\n",x->id,  (long long)x->deadline.tv_sec, x->deadline.tv_nsec);

	  pthread_mutex_lock(&mutex_canon); /*reserva de cañon*/
    printf("[%03d] ---> Moving cannon to position %d\n",x->id,p0.x);
    cannonMove(x->c,p0.x);
    clock_nanosleep(CLOCK_MONOTONIC,0,&stallTime,NULL); /*espera antes*/
    cannonFire(x->c);
    pthread_mutex_unlock(&mutex_canon); /*liberar cañon*/
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

  list_remove(x,l);
  free(x);
  pthread_exit(NULL);
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
  
  clock_gettime(CLOCK_MONOTONIC, &startMain);

  signal(SIGINT,handler);

  printf("Press ctrl+C to stop bombing\n");
  startBombing(b);
  while(1)
  {
    x=(Args_t*)malloc(sizeof(Args_t));
    x->id=workerCount++;
    x->r=r;
    x->c=c;
    x->m=radarWaitMissile(r);
    pthread_create(&x->thid,&attr,searchAndDestroy,(void*)x);
  }
  return 0; /* never reached!                                         */
}
