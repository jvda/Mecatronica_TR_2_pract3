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

#define RIGHT 1
#define LEFT  0

/* WORKER STUFF                                                       */
/* struct to pass all info to thread                                  */
typedef struct{
  int id;
  pthread_t thid;
  Radar_ptr_t   r;
  Cannon_ptr_t  c;
  Missile_ptr_t m;
  long posX; 
  int cannonMovingTo;
  long cannonPosX;
  sem_t semMissile;
} Args_t;

/* GLOBALs: needed by SIGINT handlers                                 */
World_ptr_t w;   /* to be destroyed at exit                           */
Bomber_ptr_t b;  /* start/stop bombing                                */
List_ptr_t l;    /* list of living threads                            */
pthread_attr_t attr;
sem_t semList;
long cannonLastPosX = -1;
long cannonMovingDir = RIGHT;

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
int comparePosition(void *arg1, void *arg2)
{ 
  
  Args_t *elem=arg1;
  Args_t *new=arg2;
  
  if (new->cannonMovingTo == RIGHT) 
  {
    if (new->posX > new->cannonPosX)
    {
      if (elem->posX >= new->cannonPosX)
      {
        if (elem->posX <= new->posX) return 0;
        else return 1;
      }
      else
      {
        return 1;
      }
    }
    else
    {
      if (elem->posX >= new->cannonPosX)
      {
        return 0;
      }
      else
      {
        if (elem->posX < new->posX) return 1;
        else return 0;
      }
    }    
  } 
  else
  {
    if (new->posX < new->cannonPosX)
    {
      if (elem->posX <= new->cannonPosX)
      {
        if (elem->posX >= new->posX) return 0;
        else return 1;
      }
      else
      {
        return 1;  
      }
    }
    else
    {
      if (elem->posX <= new->cannonPosX)
      {
        return 0;
      }
      else
      {
        if (elem->posX > new->posX) return 1;
        else return 0;
      }      
    }
  }
}

void* planner(void *arg)
{
  Args_t *nextMissile;

  while(1)
  { 
    sem_wait(&semList);
    nextMissile=list_dequeue(l,1);  // if wait==1, waits until thereis an element
    printf("[Planner] Next missile: %03d\n",nextMissile->id);
    sem_post(&nextMissile->semMissile);
  }
  return NULL;
}

/* thread code */
void *searchAndDestroy(void *arg)
{
  Args_t *x=arg;
  MissileState sm;
  Pos p;
  const struct timespec stallTime=(struct timespec){0, 1000000};/* 1ms*/
  const struct timespec relaxTime=(struct timespec){0,10000000};/*10ms*/
  
  sm=radarReadMissile(x->r,x->m,&p);

  x->posX = p.x;
  x->cannonMovingTo = cannonMovingDir;
  x->cannonPosX = cannonLastPosX;
  list_insert(x,comparePosition,x->id,l);

  printf("[%03d] Waiting cannon!\n",x->id);
  sem_wait(&x->semMissile);
  sm=radarReadMissile(x->r,x->m,&p);
  if (sm != MISSILE_ACTIVE)
  {
    sem_post(&semList); 
    printf("[%03d] Warning: missing missile!\n",x->id);
    printf("[%03d] \tBetween Wait & Read:\n",x->id);
    printf("[%03d] \t\tMissile impacted on ground, or\n",x->id);
    printf("[%03d] \t\tMissile intercepted by a spurious previous shoot\n",x->id);
  }
  else
  {
    printf("[%03d] ---> Moving cannon to position %d\n",x->id,p.x);
    cannonMove(x->c,p.x);
    clock_nanosleep(CLOCK_MONOTONIC,0,&stallTime,NULL); /*espera antes*/
    cannonFire(x->c);

    if (p.x > cannonLastPosX) cannonMovingDir = RIGHT;
    else cannonMovingDir = LEFT;
    cannonLastPosX = p.x;

    sem_post(&semList);
    
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
  Args_t *x, *argsPlanner;
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

  sem_init(&semList,0,1);
  argsPlanner=(Args_t*)malloc(sizeof(Args_t));
  argsPlanner->id = 0;
  pthread_create(&argsPlanner->thid,&attr,planner,(void*)argsPlanner);

  printf("Press ctrl+C to stop bombing\n");
  startBombing(b);
  while(1)
  {
    x=(Args_t*)malloc(sizeof(Args_t));
    x->id=workerCount++;
    x->r=r;
    x->c=c;
    sem_init(&x->semMissile,0,0);
    x->m=radarWaitMissile(r);
    pthread_create(&x->thid,&attr,searchAndDestroy,(void*)x);
  }
  return 0; /* never reached!                                         */
}
