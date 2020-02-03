/*
 * File: 1_DoNothing.c
 *
 * This file is part of the SimuSil library
 *
 * Compile: $ gcc -Wall -I./include -pthread -L./lib 1_DoNothing.c
 *                -lsimusil -lrt -o 1_DoNothing
 *
 * Author: Sergio Romero Montiel <sromero@uma.es>
 *
 * Created on October 27th, 2016
 * Modified 2016-11-02: added debug levels
 */


#include <stdio.h>  /* printf(3)                                      */
#include <stdlib.h> /* exit(3), EXIT_SUCCESS                          */
#include <signal.h> /* signal(2), SIGINT, SIG_IGN                     */
#include <unistd.h> /* pause(2)                                       */
#include "simusil.h"

/* global, use by SIGINT handler                                      */
Bomber_ptr_t b;
void handler(int signum)
{
  stopBombing(b);
}

/*
 * Main code
 *
 * Program that scan missiles and fire cannons against them
 */
int main(int argc, char *argv[])
{
  World_ptr_t w;

  debug_setlevel(1);
  w=createWorld("TRSM 2016",1,2); /* missiles have debug level = 1    */
  b=getBomber(w);
  printf("Press Control+C to stop bombing\n");
  signal(SIGINT,handler);
  startBombing(b);
  pause();
  printf("Press Control+C to finish\n");
  pause();
  destroyWorld(w);

  exit(EXIT_SUCCESS);
}
