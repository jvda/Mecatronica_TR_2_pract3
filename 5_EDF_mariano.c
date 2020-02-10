/*
 * File: main.c
 *
 * This file is part of the SimuSil library
 *
 * Compile: $ gcc -Wall -I./include -pthread -L./lib 5_EDF.c
 *                -lsimusil -lrt -o 5_EDF
 *
 * Author: Sergio Romero Montiel <sromero@uma.es>
 *
 * Created on November 2nd, 2016
 */

#include <stdio.h>  /* printf(3)                                      */
#include <stdlib.h> /* exit(3), EXIT_SUCCESS                          */
#include <signal.h> /* signal(2), SIGINT, SIG_DFL                     */
#include <time.h>   /* clock_nanosleep(2)                             */
#include <pthread.h>/* pthread stuff (_create,_exit,_setdettachstate) */
#include <semaphore.h>  /* sem_t, sem_init(), sem_post(), sem_wait()  */
#include "simusil.h"

#define NSECS_PER_SEC 1000000000UL
#define TS_1us   ((struct timespec){0,     1000})
#define TS_10us  ((struct timespec){0,    10000})
#define TS_100us ((struct timespec){0,   100000})
#define TS_1ms   ((struct timespec){0,  1000000})
#define TS_10ms  ((struct timespec){0, 10000000})
#define TS_25ms  ((struct timespec){0, 25000000})
#define TS_100ms ((struct timespec){0,100000000})


/* WORKER STUFF                                                       */
/* struct to pass all info to thread                                  */
typedef struct{
  int id;
  pthread_t thid;
  Radar_ptr_t   r;
  Cannon_ptr_t  c;
  Missile_ptr_t m;
  struct timespec deadline;
  List_ptr_t edf;    /* EDF list                                      */
  sem_t private; /* wait worker                                       */
  sem_t *common; /* wait dispatcher                                   */
} Args_t;

/* GLOBALs: needed by SIGINT handlers                                 */
World_ptr_t w;   /* to be destroyed at exit                           */
Bomber_ptr_t b;  /* start/stop bombing                                */
List_ptr_t l;    /* list of living threads                            */
pthread_attr_t attr;

long diff_ts(struct timespec a, struct timespec b)
{
  return ((a.tv_sec-b.tv_sec)*NSECS_PER_SEC+(a.tv_nsec-b.tv_nsec));
}

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

int cmp_deadline(void *arg1, void *arg2)
{
  Args_t *list_elem=arg1;
  Args_t *new_elem=arg2;
  
  if      (new_elem->deadline.tv_sec  < list_elem->deadline.tv_sec)  return 1;
  else if (new_elem->deadline.tv_sec  > list_elem->deadline.tv_sec)  return 0;
  else if (new_elem->deadline.tv_nsec < list_elem->deadline.tv_nsec) return 1;
  else return 0;
}

/* thread code */
void *searchAndDestroy(void *arg)
{
  Args_t *x=arg;
  MissileState sm,sm1;
  Pos p1,p2;
  struct timespec t1;
  struct timespec t2;
  struct timespec deadline;
  long tdiff,v;
  int rtn1,rtn2;

  const struct timespec stallTime2=(struct timespec){0, 10};/* 1ms*/
  const struct timespec stallTime=(struct timespec){0, 1000000};/* 1ms*/
  const struct timespec relaxTime=(struct timespec){0,10000000};/*10ms*/
  const struct timespec relaxTime2=(struct timespec){0,30000000};/*10ms*/

  sm1=radarReadMissile(x->r,x->m,&p1);
  rtn1=clock_gettime(CLOCK_MONOTONIC, &t1); 

  clock_nanosleep(CLOCK_MONOTONIC,0,&stallTime2,NULL);
  
  sm=radarReadMissile(x->r,x->m,&p2);
  rtn2=clock_gettime(CLOCK_MONOTONIC,&t2);
  tdiff=diff_ts(t2,t1);
  
  v=NSECS_PER_SEC*(p1.y-p2.y/(tdiff));

  deadline.tv_nsec=(long)0;
  deadline.tv_sec=-(time_t)((double)p2.y/v);

  x->deadline=deadline;//tiempo que va a tadar en llegar al suelo
  list_insert(x,cmp_deadline,x->id,x->edf);

  sem_wait(&x->private);
  sm=radarReadMissile(x->r,x->m,&p2);
  if (sm != MISSILE_ACTIVE)
  {
    sem_post(x->common);
    printf("[%03d] Warning: missing missile!\n",x->id);
    printf("[%03d] \tBetween Wait & Read:\n",x->id);
    printf("[%03d] \t\tMissile impacted on ground, or\n",x->id);
    printf("[%03d] \t\tMissile intercepted by a spurious previous shoot\n",x->id);
  }
  else
  {
    printf("[%03d] ---> Moving cannon to position %d\n",x->id,p2.x);
    cannonMove(x->c,p2.x);
    clock_nanosleep(CLOCK_MONOTONIC,0,&stallTime,NULL); /*espera antes*/
    cannonFire(x->c);
    sem_post(x->common);
    /* bucle de monitorizacion hasta intercepcion */
    while ((sm=radarReadMissile(x->r,x->m,&p2)) == MISSILE_ACTIVE)
    {
      clock_nanosleep(CLOCK_MONOTONIC,0,&relaxTime,NULL);
    }
    switch (sm)
    {
      case MISSILE_INTERCEPTED:
           printf("[%03d] ---> Interceptado en (%d,%d)\n",x->id,p2.x,p2.y);
           break;
      case MISSILE_IMPACTED:
           printf("[%03d] ---> Impacta en suelo (%d)\n",x->id,p2.x);
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
 * Function name: dispatcher
 *
 * Description:   code for the dispatcher thread (synchronizes workers)
 *
 * Return value:  (none)
 */
void* dispatcher(void *arg)
{
  Args_t *x=arg, *t;

  while(1)
  {
    sem_wait(x->common);
    t=list_dequeue(x->edf,1);  /* wait! */
    sem_post(&t->private);
  }
  return NULL; /* never reached                                       */
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
  List_ptr_t edf;    /* EDF list                                      */
  sem_t common;

  debug_setlevel(1);

  w=createWorld("TRSM 2016",1,2); /* worldname,1 cannon,debug level 2 */
  b=getBomber(w);
  r=getRadar(w);
  c=getCannon(w,0); /* [0..n-1] cannon number 0 (first of one)        */
  l=createList("Threads","worker",2); /* listname,elemname,debuglevel */
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);

  signal(SIGINT,handler);

  edf=createList("EDF","worker",2); /* listname,elemname,debuglevel */
  sem_init(&common,0,1); /* opened */
  x=(Args_t*)malloc(sizeof(Args_t));
  x->id=0;  /* dispatcher in thread list is 'worker 0'                */
  x->edf=edf;
  x->common=&common;
  pthread_create(&x->thid,&attr,dispatcher,(void*)x);

  printf("Press ctrl+C to stop bombing\n");
  startBombing(b);
  while(1)
  {
    x=(Args_t*)malloc(sizeof(Args_t));
    x->id=++workerCount;
    x->r=r;
    x->c=c;
    x->edf=edf;
    x->common=&common;
    sem_init(&x->private,0,0); /* closed */
    x->m=radarWaitMissile(r);
    pthread_create(&x->thid,&attr,searchAndDestroy,(void*)x);
  }
  return 0; /* never reached!                                         */
}
