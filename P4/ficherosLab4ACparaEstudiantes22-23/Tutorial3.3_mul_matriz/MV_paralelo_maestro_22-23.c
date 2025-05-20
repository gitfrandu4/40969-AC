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

// Zona de memoria compartida para matrices A, B y C
volatile int * A	= (int *) 0x806000; 	// 8x8x4=256 B: 0x806000 - 0x8060FF
volatile int * B 	= (int *) 0x806400; 	// 8x8x4=256 B: 0x806400 - 0x8064FF
volatile int * C	= (int *) 0x806800; 	// 8x8x4=256 B: 0x806800 - 0x8068FF

#define n 8 					// tamaño de las matrices 8x8
#define sleepTime 100000

int rank = 0; 					// hilo maestro para nucleo= CPU

// Las siguientes constantes se encuentran definidas en fichero system.h
#define tipoNios2   ALT_CPU_CPU_IMPLEMENTATION 	// "fast" (Nios II/f), "tiny" (Nios II/e), "small" (Nios II/s)
#define nombreNios2 ALT_CPU_NAME 				// "CPU" (nucleo 1), "CPU2" (nucleo 2)
#define size_dCache ALT_CPU_DCACHE_SIZE			// tamanyo de la dCache

// inicializaMemoria : Inicializa zona memoria compartida
//
// ini_printf=0: inicializa valores de las matrices A, B, C
// ini_printf=1: printf de valores de A, B, C
// ini_printf=2: printf de direcciones de A, B, C
//
void inicializaMemoria(int ini_printf){
   int i,j;
   if (ini_printf == 0){
	   printf("\nInicializa Matrices\n");
   }
   else if (ini_printf == 1){
	   printf("\nPRINTF VALORES\n");
   }
   else if (ini_printf == 2){
	   printf("\nPRINTF DIRECCIONES\n");
   }
   for (i=0; i<n; i++){
	   if (ini_printf == 0){
		   for (j=0; j<n; j++) {
			   A[i*n+j] = (int)i+j;
			   B[i*n+j] = (int)i-j;
			   C[i*n+j] = 0;
		   }
	   }
	   else if (ini_printf == 1){
		   printf("C[%2i]= ", i);
		   for (j=0; j<n; j++) {
			   printf("%3i ", (int)C[i*n+j]);
		   }
		   printf("\n");
	   }
	   else if (ini_printf == 2){
		   /* Imprime las direcciones de memoria de las filas i-ésimas de las matrices:
		    * - &C[i*n]: Dirección base de la fila i de la matriz C 
		    * - &A[i*n]: Dirección base de la fila i de la matriz A
		    * - &B[i*n]: Dirección base de la fila i de la matriz B
		    * Las direcciones se imprimen en hexadecimal para verificar 
		    * que están en las zonas de memoria correctas:
		    * C: 0x806800-0x8068FF
		    * A: 0x806000-0x8060FF  
		    * B: 0x806400-0x8064FF
		    */
		   printf("C[%i]= 0x%x\tA[%i]= 0x%x\tB[%i]= 0x%x\n", 
		   	i, (unsigned int) &C[i*n], i, (unsigned int) &A[i*n], i, (unsigned int) &B[i*n]);
	   }
   }
}

// main : programa principal del hilo maestro
//
int main()
{
	 int thread_count	= 1    ; 	// opciones: 1,2; numero de hilos
	 int Niter		= 30000; 		// opciones: 1000,2000,10000,20000,...; veces repite matriz-vector
	 int dumy, start, iteraciones = 0, timeInterval = 0, timeInterval2 = 0;
	 alt_u32 freq		= 0    ;
	 unsigned int time[5];
	 char etiqueta_time[6][6]={"tStar","tInic","tFork","tComp","tJoin","tFina"};

	 int message_buffer_val      	= 0x0; // variable local, copia de variable global
	 int message_buffer_val_fork 	= 0x0; // variable local, copia de variable global
	 int message_buffer_val_join 	= 0x0; // variable local, copia de variable global

  	 // direccion del dispositivo mutex de exclusion mutua 
  	 alt_mutex_dev* mutex = altera_avalon_mutex_open("/dev/message_buffer_mutex");

  	 alt_putstr("\n\nMatriz x Matriz Paralelo - CPU Maestro - BEGIN\n");
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

	// Lee RAM de sincronizacion, se obliga a hacerlo con exclusión mutua
	//
	altera_avalon_mutex_lock(mutex,1); 				// bloquea mutex

	message_buffer_val 			= *(message_buffer_ptr); 		// lee valor de RAM sincronizacion
	message_buffer_val_fork 	= *(message_buffer_ptr_fork); 	// lee valor de RAM sincronizacion
	message_buffer_val_join 	= *(message_buffer_ptr_join); 	// lee valor de RAM sincronizacion

	altera_avalon_mutex_unlock(mutex); 				// libera mutex

	inicializaMemoria(0);			// se inicializan los valores de A,B,C
	inicializaMemoria(1);			// se visualizan los valores de la matriz C

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
	message_buffer_val 			= 15;  // ID=15(0xF) indica que Fork empieza
	message_buffer_val_fork 	= 1 ;  // indica que maestro esta preparado para computo


	// Escribe RAM de sincronizacion, se obliga a hacerlo con exclusión mutua

	altera_avalon_mutex_lock(mutex,1);				// bloquea mutex

	*(message_buffer_ptr) 		= message_buffer_val; 			// inicializa RAM FORK
	*(message_buffer_ptr_fork) 	= message_buffer_val_fork;  	// inicializa RAM FORK
	*(message_buffer_threads) 	= thread_count;  				// inicializa RAM FORK
	*(message_buffer_Niter) 	= Niter;  						// inicializa RAM FORK

	altera_avalon_mutex_unlock(mutex); 							// libera mutex


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
	// COMPUTO MAESTRO - Operacion Matriz x Matriz - repetido Niter veces
	// 1 hilo : el hilo calcula toda la matriz resultado : C
	// 2 hilos: cada hilo calcula la mitad de elementos de la matriz resultado: C
	//
	// time2: marca tiempo inicial computo y final de FORK
	time[2] = alt_timestamp();

	/* Variables for matrix multiplication:
	 * i: row index for matrices A and C
	 * j: column index for matrices B and C 
	 * k: index for dot product calculation
	 * k1: iteration counter for repeated matrix multiplications
	 * cij: temporary variable to store element C[i][j] during calculation
	 */
	int i, j, k, k1, cij;
	int local_n 	 = n / thread_count;		// numero de filas por hilo
	int my_first_row = rank * local_n;			// 1a fila asignada a este hilo
	int my_last_row  = (rank+1) * local_n - 1;	// ultima fila asignada a este hilo

	printf("\nEmpieza el computo Matriz-Matriz, numero iteraciones: %i\n", Niter);

	for (k1 = 0; k1 < Niter; k1++) {				// Repite el calculo Niter veces
	   	iteraciones++;
		for (i=my_first_row; i<=my_last_row; i++){	// Bucle sobre las filas de C y A
	       		for (j=0; j<n; j++){				// Bucle sobre las columnas de B y C
				cij = C[i*n+j];  					// cij ← C[i][j] - Carga el valor de C[i][j] en cij
				for(k=0; k<n; k++){					// Bucle sobre las columnas de A y B
					cij += A[i*n+k] * B[k*n+j]; 	// cij += A[i][k]*B[k][j] - Multiplica A[i][k] y B[k][j] y suma el resultado a cij
				}
				C[i*n+j] = cij; 					// C[i][j] ← cij - Almacena el resultado en C[i][j]
		   	}
	    }
	}
	// Fin del bucle de iteraciones

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
