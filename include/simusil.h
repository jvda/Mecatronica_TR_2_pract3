/*
 * File: simusil.h
 *
 * Author: Sergio Romero Montiel <sromero@uma.es>
 *
 * Created on October 27th, 2016
 * Modified: 2016-11-01 enum MissileState style changed
 */

#ifndef _SIMUSIL_H_
#define _SIMUSIL_H_

/* tipos */
typedef struct{
  int x;
  int y;
}Pos;

typedef struct List* List_ptr_t;

typedef struct Radar* Radar_ptr_t;
typedef struct Bomber* Bomber_ptr_t;
typedef struct World* World_ptr_t;
typedef struct Cannon* Cannon_ptr_t;
typedef struct Missile* Missile_ptr_t;
typedef enum{MISSILE_ERROR,MISSILE_ACTIVE=0,MISSILE_INTERCEPTED,MISSILE_IMPACTED}MissileState;


/* Prototipos */

// DEBUG //
/* funciones para el control de depuracion */
void debug_setlevel(int);
int debug_getlevel();
// END DEBUG //


// LISTS //
/* funciones para el manejo de listas */
/*
 * Fucntion name: createList
 * Description:   allocates and initializes an empty List
 *                name are only used in debug to refer to the list or the objects
 *                Debug messages are printed if DEBUG is defined and the debug level
 *                (debug_setlevel()) is set a value greater or equal third argument
 * Return value:  a pointer to the allocated List object
 */
List_ptr_t createList(char *,char *,int); // list name, elem name, debug level

/*
 * Function name: destroyList
 * Description:   removes and frees every Elem in List, and free the List object
 *                The List to be destroyed should be created with createList
 *                If given a function (destructor) on arg 2, it is invoked with
                  every object in the list.
 * Return value:  (none)
 */
void destroyList(List_ptr_t, void(*)(void*));

/*
 * Function name: list_enqueue
 * Description:   enqueue a reference to an object at the tail of the list
 * Return value:  A pointer to the enqueued Elem (which points to object)
 */
void list_enqueue(void *, int, List_ptr_t);

/*
 * Function name: list_dequeue
 * Description: list_dequeues first Elem at the head of the queue
 *              if second arg is 0 and the list is empty return inmediately
 *              if second arg is not 0 and the list is empty then it
 *              waits until there is an element available
 * Return value: pointer to first object in List or NULL if empty list
 */
void* list_dequeue(List_ptr_t,int wait);  // if wait==1, waits until thereis an element

/*
 * Function name: list_remove
 * Description:   Search and remove a given refence (ptr) from a given List
 * Return value:  1 if found and extracted; 0 is not found in List
 */
int  list_remove(void *,List_ptr_t);

/*
 * Function name: list_insert
 * Description:   Insert an element in an ordered List
 *                The order is determined by the function provided
 *                The new Elem is inserted between two elements
 *                elem_A and elem_B, where:
 *                  elem_A->next == elem_B (AND elem_B->prev == elem_A)
 *                  cmp(elem_B,new) => 1
 *                  cmp(elem_A,new) => 0
 *                giving:
 *                elem_A->next == new; new->next == elemB
 *                elem_B->prev == new; new->prev == elemA
 * Return value:  (none)
 */
void list_insert(void *,int(*)(void*,void*),int,List_ptr_t);
// END LIST //


// WORLD //
/* funciones para la gestion del mundo del simulador*/
World_ptr_t createWorld(char *, int, int); // nombre, numero de piezas de artilleria, debug level
void destroyWorld(World_ptr_t);
// END WORLD //


// RADAR //
/* funciones para consultar el radar */
Radar_ptr_t getRadar(World_ptr_t);
Missile_ptr_t radarWaitMissile(Radar_ptr_t);
MissileState radarReadMissile(Radar_ptr_t,Missile_ptr_t,Pos*);
// END RADAR //


/* funciones para manejar la artilleria */
int getNumCannons(World_ptr_t);
Cannon_ptr_t getCannon(World_ptr_t,int);
void cannonMove(Cannon_ptr_t,int);
void cannonFire(Cannon_ptr_t);

/* funciones para controlar el bombardeo (solo en desarrollo) */
Bomber_ptr_t getBomber(World_ptr_t);
void startBombing(Bomber_ptr_t);
void stopBombing(Bomber_ptr_t);

#endif /*_SIMUSIL_H_*/

