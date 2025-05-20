//
// AC - Practica 4 - version: junio 2022
//
// Multiplicacion Matriz x Vector, y = A . x
// Version paralela - Hilo Maestro
// Tipo multiprocesador: 2 x Nios II/{e,s}, DualCoreNios2{e,s}.sof
// Tipo procesador: Nios II/{e,s}, nombre: CPU
//
//

#include <stdio.h>
#include <altera_avalon_mutex.h>		// para controlador exclusion mutua
#include <system.h>
#include "sys/alt_stdio.h" 			// para alt_putstr
#include <unistd.h> 				// para usleep
// Timer, incluir el timestamp en BSP: boton dcho en BSP folder, Nios2 > BSP editor > cambiar system timer y timestamp timer
#include <sys/alt_timestamp.h>

// RAM para sincronizacion entre hilos, tamano total RAM= 20 bytes
// Todas las variables se encuentran en una linea de cache (32 bytes)
//
volatile unsigned int * message_buffer_ptr 	= (unsigned int *) MESSAGE_BUFFER_RAM_BASE;
volatile unsigned int * message_buffer_ptr_join = (unsigned int *) (MESSAGE_BUFFER_RAM_BASE+4);
volatile unsigned int * message_buffer_ptr_fork = (unsigned int *) (MESSAGE_BUFFER_RAM_BASE+8);
volatile unsigned int * message_buffer_threads  = (unsigned int *) (MESSAGE_BUFFER_RAM_BASE+12);
volatile unsigned int * message_buffer_Niter    = (unsigned int *) (MESSAGE_BUFFER_RAM_BASE+16);

// Zona de memoria compartida para matriz A y vectores x, y
volatile int * A	= (int *) 0x806000; 	// 16x16x4=1KiB: 0x806000 - 0x8063FF
volatile int * x 	= (int *) 0x806400; 	// 16x1 x4=64 B: 0x806400 - 0x80643F
volatile int * y	= (int *) 0x806800; 	// 16x1 x4=64 B: 0x806800 - 0x80683F

#define m 16 					// numero de columnas de las matrices
#define n 16 					// numero de filas de las matrices

#define sleepTime 100000

int rank = 0; 					// hilo maestro para nucleo= CPU

// Las siguientes constantes se encuentran definidas en fichero system.h
#define tipoNios2   ALT_CPU_CPU_IMPLEMENTATION 	// "fast" (Nios II/f), "tiny" (Nios II/e), "small" (Nios II/s)
#define nombreNios2 ALT_CPU_NAME 		// "CPU" (nucleo 1), "CPU2" (nucleo 2)
#define size_dCache ALT_CPU_DCACHE_SIZE		// tamanyo de la dCache

// inicializaMemoria : Inicializa zona memoria compartida
//
// ini_printf=0: inicializa valores de las matrices A, x, y
// ini_printf=1: printf de valores de A, x, y
// ini_printf=2: printf de direcciones de A, x, y
//
void inicializaMemoria(int ini_printf){
   int i,j;
   if (ini_printf == 0){
	   printf("\nInicializa Matriz y Vector\n");
   }
   else if (ini_printf == 1){
	   printf("\nPRINTF VALORES\n");
   }
   else if (ini_printf == 2){
	   printf("\nPRINTF DIRECCIONES\n");
   }
   for (i=0; i<n; i++){
	   if (ini_printf == 0){
		   x[i] = (int)i;
		   y[i] = 0.0;
	   }
	   else if (ini_printf == 1){
		   printf("y[%2i]= %i"   , i, (int)y[i]);
		   printf("\tx[%2i]= %2i", i, (int)x[i]);
		   printf("\tA[%2i]= "   , i);
	   }
	   else if (ini_printf == 2){
		   printf("y[%i]= 0x%x"  , i, (unsigned int) &y[i]);
		   printf("\tx[%i]= 0x%x", i, (unsigned int) &x[i]);
		   printf("\tA[%i][]= "  , i);
	   }
	   for(j=0; j<m; j++){
		   if (ini_printf == 0){
			   A[i*m+j]=(int)j;
		   }
		   else if (ini_printf == 1){
			   printf("%i ", (int)A[i*m+j]);
		   }
		   else if (ini_printf == 2){
			   printf(" 0x%x ", (unsigned int) &A[i*m+j]);
		   }
	   }
	   if (ini_printf == 1 || ini_printf == 2){
		   printf("\n");
	   }
   }
}

// main : programa principal del hilo maestro
//
int main()
{
	 int thread_count	= 1    ; 	// opciones: 1,2; numero de hilos
	 int Niter		= 30000; 	// opciones: 1000,2000,10000,20000,...; veces repite matriz-vector
	 int dumy, start, iteraciones = 0, timeInterval = 0, timeInterval2 = 0;
	 alt_u32 freq		= 0    ;
	 unsigned int time[5];
	 char etiqueta_time[6][6]={"tStar","tInic","tFork","tComp","tJoin","tFina"};

	 int message_buffer_val      	= 0x0; // variable local, copia de variable global
	 int message_buffer_val_fork 	= 0x0; // variable local, copia de variable global
	 int message_buffer_val_join 	= 0x0; // variable local, copia de variable global

  	 // direccion del dispositivo mutex de exclusion mutua 
  	 alt_mutex_dev* mutex = altera_avalon_mutex_open("/dev/message_buffer_mutex");

  	 alt_putstr("\n\nMatriz x Vector Paralelo - CPU Mestro - BEGIN\n");
  	 printf("\tNombre procesador Nios II\t: %s\n", nombreNios2);
  	 printf("\tTipo procesador Nios II\t\t: %s\n", tipoNios2);
  	 printf("\tTamano dCache Nios II\t\t: %i bytes\n", size_dCache);
  	 printf("\tHilos\t\t\t\t: %i \n\tIteraciones\t\t\t: %i\n", thread_count, Niter);

 	// Inicializa el timestamp para medir tiempo en ciclos de reloj
 	start = alt_timestamp_start();
 	if(start < 0) {
     		printf("\nTimestamp start -> FALLO!, %i\n", start);
     	}
     	else{
     		freq = alt_timestamp_freq() / 1e6;
     		printf("\nTimestamp start -> OK!, frecuencia de reloj= %u MHz\n", (unsigned int) freq);
     	}

	// time0: marca tiempo inicial
	time[0] = alt_timestamp();

	//
	// INCIALIZACION DE VARIABLES
	//

	// Lee RAM de sincronizacion, se obliga a hacerlo con exclusi—n mutua
	//
	altera_avalon_mutex_lock(mutex,1); 				// bloquea mutex

	message_buffer_val 		= *(message_buffer_ptr); 	// lee valor de RAM sincronizacion
	message_buffer_val_fork 	= *(message_buffer_ptr_fork); 	// lee valor de RAM sincronizacion
	message_buffer_val_join 	= *(message_buffer_ptr_join); 	// lee valor de RAM sincronizacion

	altera_avalon_mutex_unlock(mutex); 				// libera mutex

	inicializaMemoria(0);			// se inicializan los valores de A,x,y
	inicializaMemoria(1);			// se visualizan los valores de la matriz A

	alt_putstr("\nCPU - antes FORK\n");
	printf("\tmessage_buffer_val:\t\t%08X\n\tmessage_buffer_val_fork:\t%08X\n\tmessage_buffer_val_join:\t%08X\n",
			message_buffer_val, message_buffer_val_fork, message_buffer_val_join);    

	// time1: marca tiempo inicial FORK y final de Inicializacion
	time[1] = alt_timestamp();

	//
	// FORK - Sincronizacion de Distribucion
	// Maestro inicializa variables compartidas:
	//    *(message_buffer_ptr) 	 = 15
	//    *(message_buffer_ptr_fork) = 1
	//
	message_buffer_val 		= 15;  // ID=15(0xF) indica que Fork empieza
	message_buffer_val_fork 	= 1 ;  // indica que maestro esta preparado para computo


	// Escribe RAM de sincronizacion, se obliga a hacerlo con exclusi—n mutua

	altera_avalon_mutex_lock(mutex,1);				// bloquea mutex

	*(message_buffer_ptr) 		= message_buffer_val; 		// inicializa RAM FORK
	*(message_buffer_ptr_fork) 	= message_buffer_val_fork;  	// inicializa RAM FORK
	*(message_buffer_threads) 	= thread_count;  		// inicializa RAM FORK
	*(message_buffer_Niter) 	= Niter;  			// inicializa RAM FORK

	altera_avalon_mutex_unlock(mutex); 				// libera mutex


	// bucle while de espera de la sincronizacion de los hilos en FORK
	// message_buffer_val = 5 : indica que los dos hilos estan sincronizados para empezar computo
	//
	int vista = 0;
	while( (message_buffer_val != 5) ){
		if (vista > 0) usleep(sleepTime); 			// espera "sleepTime/1.000.000" segundos

		altera_avalon_mutex_lock(mutex,1);			// bloquea mutex

		message_buffer_val_fork = *(message_buffer_ptr_fork); 	// lee valor en RAM para ver si el otro procesador esta listo

		altera_avalon_mutex_unlock(mutex); 					  // libera mutex

		// message_buffer_val_fork == 0x3 : indica que los dos hilos (thread_count = 2) estan sincronizados en FORK
		// message_buffer_val_fork == 0x1 : indica que el hilo maestro unico (thread_count = 1) esta sincronizado en FORK
		if ( (message_buffer_val_fork == 0x3 && thread_count == 2 ) ||
			 (message_buffer_val_fork == 0x1 && thread_count == 1) ){
			dumy = 5;

			altera_avalon_mutex_lock(mutex,1);	// bloquea mutex
			*(message_buffer_ptr) 		= dumy; // escribe valor en buffer
			*(message_buffer_ptr_join) 	= 0;  	// escribe valor en buffer
			altera_avalon_mutex_unlock(mutex); 	// libera mutex

			message_buffer_val 		= dumy;

			alt_putstr("\nCPU - SINCRONIZACION FORK REALIZADA!!\n");
			printf("\tmessage_buffer_val:\t\t%08X\n\tmessage_buffer_val_fork:\t%08X\n",
					message_buffer_val, message_buffer_val_fork);      
		}
		vista++;
	}


	//
	// COMPUTO MAESTRO - Operacion Matriz x Vector - repetido Niter veces
	// 1 hilo : el hilo calcula todo el vector resultado : y
	// 2 hilos: cada hilo calcula la mitad de elementos del vector resultado: y
	//
	// time2: marca tiempo inicial computo y final de FORK
	time[2] = alt_timestamp();

	int i, j, k1, k;
	int local_n 	 = n / thread_count;
	int my_first_row = rank * local_n;			// 1a fila asignada a este hilo
	int my_last_row  = (rank+1) * local_n - 1;  		// ultima fila asignada a este hilo

	printf("\nEmpieza el computo Matriz-Vector, numero iteraciones: %i\n", Niter);

	for (k1 = 0; k1 < Niter; k1++) {
	   	iteraciones++;
		for (i=my_first_row; i<=my_last_row; i++){
	       		dumy = y[i];
		   	for(j=0; j<m; j++){
			   dumy = dumy + A[i*m+j] * x[j];
		   	}
		   	y[i] = dumy;
	    	}
	}

	//
	// JOIN - Sincronizacion de union
	// Maestro inicializa variable compartida:
	//    *(message_buffer_ptr_join) |= 1
	//
	// time3: marca tiempo del inicio sincronizacion JOIN y final de computo
	time[3] = alt_timestamp();

	message_buffer_val_join = 1; 					// indica que maestro ha llegado a JOIN

	altera_avalon_mutex_lock(mutex,1);				// bloquea mutex

	*(message_buffer_ptr_join) 	|= message_buffer_val_join; 	// inicializa RAM JOIN por parte del maestro

	altera_avalon_mutex_unlock(mutex); 				// libera mutex

	// bucle while de espera de la sincronizacion de los hilos en JOIN
	// message_buffer_val = 6 : los dos hilos estan sincronizados para terminar JOIN

	vista = 0;
	while( (message_buffer_val != 6) ){
		if (vista > 0) usleep(sleepTime); 			// espera "sleepTime/1.000.000" segundos

		altera_avalon_mutex_lock(mutex,1);			// bloquea mutex

		message_buffer_val 	= *(message_buffer_ptr); 	// lee valor almacenado en RAM
		message_buffer_val_join = *(message_buffer_ptr_join); 	// lee valor almacenado en RAM

		altera_avalon_mutex_unlock(mutex); 			// libera mutex

		if ( (message_buffer_val_join == 0x3 && thread_count == 2 ) ||
			 (message_buffer_val_join == 0x1 && thread_count == 1) ){

			dumy = 6;

			altera_avalon_mutex_lock(mutex,1);		// bloquea mutex
			*(message_buffer_ptr) 			= dumy; // escribe valor en RAM indicando que los hilos estan sincronizados JOIN
			altera_avalon_mutex_unlock(mutex); 		// libera mutex

			message_buffer_val 			= dumy; // actualiza variable local

			alt_putstr("\nCPU - SINCRONIZACION JOIN REALIZADA!!\n");
			printf("\tmessage_buffer_val:\t\t%08X\n\tmessage_buffer_val_join:\t%08X\n\n",
					message_buffer_val, message_buffer_val_join);
		}
		vista++;
	}

	//
	// FINAL
	//
	// time4: Nueva marca de tiempo (parte final del programa)
	time[4] = alt_timestamp();

	// printf de los tiempos medidos
	for (k = 1; k < 5; k++){
		timeInterval = (time[k] - time[0])   * 1e-3 / freq;
		timeInterval2= (time[k] - time[k-1]) * 1e-3 / freq;
		alt_putstr("CPU - ");
		printf("%6s : time[%i]= %10u clk\t (%6u ms) intervalo= %6u ms\n",
				&etiqueta_time[k][0], k, time[k], timeInterval, timeInterval2);
	}

	timeInterval = (time[4] - time[0]) * 1e-3 / freq;
	printf("\nCPU - %6s : time[%i]= %10u clk\t TiempoTotal= %6u ms\n",
			&etiqueta_time[5][0], 5, time[4], timeInterval);

	// Reset de las variables de sincronizacion a 0
	altera_avalon_mutex_lock(mutex,1);				// bloquea mutex
	*(message_buffer_ptr) 		= 0;  				// inicializa RAM
	*(message_buffer_ptr_fork) 	= 0;  				// inicializa RAM FORK
	*(message_buffer_ptr_join) 	= 0;  				// inicializa RAM JOIN
	altera_avalon_mutex_unlock(mutex);				// libera mutex

	inicializaMemoria(1);						// se visualiza los valores de la matriz C resultante

	alt_putstr("\nFin del programa y reseteadas las variables de sincronizacion\n");

  	return 0;
}
