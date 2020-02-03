+------------------------------------------------------------------------------
| File: Readme.txt
|
| Directory: simusil
|
| A simulator of missile bombing, defese cannons and radar.
|
| Author: Sergio Romero Montiel
|
| Created on October 28th, 2016
+------------------------------------------------------------------------------


Simusil Library
================

Incluye varios codigos de ejemplo para consultar el radar y controlar la
artilleria de defensa.

Hay seis versiones:

a) El codigo 1_DoNothing.c inicia el simulador de bombardeo y espera a la
	pulsacion de ENTER para deterner el bombardeo (los similes activos
	siguen cayendo, pero no se generan nuevos). Después espera de nuevo
	la pulsacion de ENTER para liberar los recursos y salir.
	No realiza consultas del radar ni control de las defensas.
	-----------------------------------------------------------------------
	...
	createWorld();
	getchar();
	stopBombing(); // solo para debug, no debe usarse en los codigos
	getchar();
	destroyWorld();
	...
	-----------------------------------------------------------------------

b) El codigo 2_Serial.c inicia el simulador y entra en un bucle sin fin de:
	-----------------------------------------------------------------------
	1) esperar un misil en el radar [radarWaitMissile()]
	2) consultar la situacion [radarReadMissile()]
	3) mover el arma [cannonMove()]
	4) esperar a que el arma se estabilice [clock_nanosleep()]
	5) disparar (con las esperas temporizadas requeridas) [cannonFire()]
	6) bucle de seguimiento en el radar para localizar la intercepcion
		6.1) esperar 25ms
		6.2) consultar situacion del misil en el radar
		6.3) si aparece
			6.3.1) establecer como ultima posicion la actual
			6.3.2) volver al punto (7.1)
		6.4) si no aparece (interceptado)
			6.4.1) imprime la ultima posicion
			6.4.2) salir del bucle de seguimiento, ir a (8)
	7) Ir a (1)
	-----------------------------------------------------------------------
	Durante el seguimiento, los misiles van cayendo y la tasa de acierto es
	muy baja.

c) El codigo 3_Parallel.c inicia el simulador y entra en un bucle sin fin de:
	---------------------[Main]--------------------------------------------
	1) espera un misil en el radar
	2) crea un thread para el seguimiento e intercepcion [pthread_create()]
	3) Ir a (1)
	---------------------[Thread]------------------------------------------
	1) consultar la situacion
	2) mover y disparar el arma (con las esperas temporizadas requeridas)
	3) bucle de seguimiento, identico al punto (7) del codigo anterior
	4) FIN [pthread_exit()]
	-----------------------------------------------------------------------
	Puesto que la bateria debe usarse en exclusion mutua, el codigo falla
	siempre, peor que la version Serie.
	
d) El codigo 4_Mutex.c corrige el acceso al dispositivo que debe usarse en
	exclusividad por los diferentes threads.
	-----------------------------------------------------------------------
	mutex_lock()
	/* seccion critica de manejo del arma, punto (2) del codigo anterior */
	mutex_unlock()
	-----------------------------------------------------------------------
	La respuesta en bastante mejor, la bateria queda libre despues del
	diparo y se puede hacer en paralelo el seguimiento de varios misiles
	con el uso del arma.
	Los misiles son tratados a medida que aparecen y los threads de defensa
	se excluyen entre sí pero no se tiene en cuenta que los misiles tienen
	tiempos de llegada diferentes y ubicaciones aleatorias y que el arma se
	debe mover de una posicion a otra quizas pasando por posiciones donde
	hay misiles activos o ignorardo los más rapidos.

e) El codigo 5_EDF.c inicia el simulador y entra en un bucle sin fin de:
	---------------------[Main]--------------------------------------------
	1) espera un misil en el radar
	2) crea un thread para el seguimiento e intercepcion [worker]
	3) Ir a (1)
	---------------------[Thread]------------------------------------------
	1) consultar la situacion
	2) esperar dt
	3) consultar la situacion
	4) calcular la velocidad y estimar el tiempo de llegada (deadline)
	5) insertar en una lista ordenada por deadline un puntero a un semaforo
		en el que esperar [sem_wait()]
	{saliendo de la espera, los threads previos terminaron el uso del arma}
	6) mover y disparar (esperar a que se estabilice)
	8) despertar al siguiente de la lista [sem_post()]
	9) bucle de seguimiento del misil en el radar
	10) FIN
	-----------------------------------------------------------------------

f) El codigo 6_Scheduler.c inicia el simulador, crea un thread que hara de
	planificador de objetivos [master] y entra en un bucle sin fin de:
	---------------------[MAIN]--------------------------------------------
	1) espera un misil en el radar
	2) crea un thread para el seguimiento e intercepcion [worker]
	3) Ir a (1)
	---------------------[Master]------------------------------------------
	Recorre la lista de posicion siguiendo el algoritmo del ascensor y va
	despertando a los Workers para que usen el arma en exclusion mutua
	1) Establecer el siguiente Worker
	2) Despertarlo [sem_post()] del semaforo donde duerme
	3) Esperar a que termine el uso en exclusiva del arma [sem_wait]
	{saliendo de la espera despertado por el ultimo Worker activado}
	4) Ir a (1)
	---------------------[Workers]-----------------------------------------
	1) consultar la situacion
	2) insertar en una lista ordenada por posicion un puntero a un semaforo
		en el que esperar [sem_wait()]
	{saliendo de la espera despertado por el Master}
	6) mover y disparar el arma
	7) notificar al Master el fin de la seccion critica [sem_post()]
	8) bucle de seguimiento del misil en el radar
	9) FIN
	-----------------------------------------------------------------------


Desarrolla las tres ultimas versiones y comparar sus resultados.

