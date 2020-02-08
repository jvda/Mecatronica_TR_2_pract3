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

#define DELTATIME 10000 /*10ms*/

/* WORKER STUFF                                                       */
/* struct to pass all info to thread                                  */
typedef struct{
  int id;
  pthread_t thid;
  Radar_ptr_t   r;
  Cannon_ptr_t  c;
  Missile_ptr_t m;
} Args_t;

typedef struct{
  struct timespec impact_time;
  sem_t *action_command;
} list_item;

/* GLOBALs: needed by SIGINT handlers                                 */
World_ptr_t w;   /* to be destroyed at exit                           */
Bomber_ptr_t b;  /* start/stop bombing                                */
List_ptr_t l;    /* list of living threads                            */
pthread_attr_t attr;
sem_t mutex_canon;
sem_t mutex_lista;
unsigned n_lista=0;
List_ptr_t lista_misiles;

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

struct timespec addTime(struct timespec ts, s, ns)
{
  ts.tv_sec += s;
  ts.tv_nsec += ns;
  while (ts.tv_nsec >= 1000000000) {
    ts.tv_nsec -= 1000000000;
    ++ts.tv_sec;
  }
  return ts;
}

int cmp(void *a, void *b) {
  if (a->impact_time.tv_sec > b->impact_time.tv_sec)
    return 1;
  else if (a->impact_time.tv_sec < b->impact_time.tv_sec)
    return 0;
  else if (a->impact_time.tv_nsec > b->impact_time.tv_nsec)
    return 1;
  else
    return 0;
}


/* thread code */
void *searchAndDestroy(void *arg)
{
  Args_t *x=arg;
  MissileState sm;
  Pos p0, p1;
  const struct timespec stallTime=(struct timespec){0, 1000000};/* 1ms*/
  const struct timespec relaxTime=(struct timespec){0,10000000};/*10ms*/
  struct timespec start, dt;
  int timeRemaining;
  int first_missile=0;
  list_item misile_impact_data;
  sem_t action_command;
  sem_t *next_misile;

  list_enqueue(x,x->id,l);

  clock_gettime(CLOCK_MONOTONIC, &start);
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
    dt = addTime(&start, 0, DELTATIME) /*10us later*/
    clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&dt,NULL);
    sm=radarReadMissile(x->r,x->m,&p1)
    timeRemaining = p1.y/(p0.y-p1.y)*DELTATIME;
    misile_impact_data.impact_time = addTime(&dt, 0, timeRemaining); /*estimated impact time*/
    sem_init(&action_command, 0, 1);
    misile_impact_data.action_command = &action_command;
    
    sem_wait(&mutex_lista)
    if(n_lista++ == 0) first_missile=1;
    sem_post(&mutex_lista)
    
    if (!first_missile) {
      list_insert(&misile_impact_data, cmp, x->id, lista_misiles);
      sem_wait(&action_command);
    }
    
    /*sem_wait(&mutex_canon); /*reserva de cañon*/
    printf("[%03d] ---> Moving cannon to position %d\n",x->id,p.x);
    cannonMove(x->c,p.x);
    clock_nanosleep(CLOCK_MONOTONIC,0,&stallTime,NULL); /*espera antes*/
    cannonFire(x->c);
    /*sem_post(&mutex_canon); /*liberar cañon*/
    sem_wait(&mutex_lista);
    --n_lista;
    next_misile = list_dequeue(lista_misiles, 0);
    sem_post(next_misile);
    sem_post(&mutex_lista);
    
    /* bucle de monitorizacion hasta intercepcion */
    while ((sm=radarReadMissile(x->r,x->m,&p)) == MISSILE_ACTIVE)
    {
	  //printf("[%03d] ---> en seguimiento (%d,%d)\n",x->id,p.x,p.y);
      clock_nanosleep(CLOCK_MONOTONIC,0,&relaxTime,NULL);
    }
    switch (sm)
    {
      case MISSILE_INTERCEPTED:
           printf("[%03d] ---> Interceptado en (%d,%d)\n",x->id,p.x,p.y);
           break;
      case MISSILE_IMPACTED:
           printf("[%03d] ---> Impacta en suelo (%d)\n",x->id,p.x);
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

  w=createWorld("TRSM 2020",1,2); /* worldname,1 cannon,debug level 2 */
  b=getBomber(w);
  r=getRadar(w);
  c=getCannon(w,0); /* [0..n-1] cannon number 0 (first of one)        */
  l=createList("Threads","worker",2); /* listname,elemname,debuglevel */
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
  sem_init(&mutex_canon, 0, 1);
  sem_init(&mutex_lista, 0, 1);
  lista_misiles = createList("Misiles", "Misil", 0);

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
