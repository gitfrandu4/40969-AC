//
// AC - Practica 4 - version: junio 2022
//
// Multiplicacion Matriz x Vector, y = A . x
// Version paralela - Hilo Esclavo
// Tipo multiprocesador: 2 x Nios II/{e,s}, DualCoreNios2{e,s}.sof
// Tipo procesador: Nios II/{e,s}, nombre: CPU2
//
//

#include <stdio.h>
#include <altera_avalon_mutex.h>
#include <system.h>

// RAM para sincronizacion entre hilos, tamano total RAM= 20 bytes
// Todas las variables se encuentran en una linea de cache (32 bytes)
//
volatile unsigned int * message_buffer_ptr 		= (unsigned int *) MESSAGE_BUFFER_RAM_BASE;
volatile unsigned int * message_buffer_ptr_join = (unsigned int *) (MESSAGE_BUFFER_RAM_BASE+4);
volatile unsigned int * message_buffer_ptr_fork = (unsigned int *) (MESSAGE_BUFFER_RAM_BASE+8);
volatile unsigned int * message_buffer_threads  = (unsigned int *) (MESSAGE_BUFFER_RAM_BASE+12);
volatile unsigned int * message_buffer_Niter    = (unsigned int *) (MESSAGE_BUFFER_RAM_BASE+16);

// Zona de memoria compartida para matrices A, B y C
volatile int * A	= (int *) 0x806000; 	// 8x8x4=256 B: 0x806000 - 0x8060FF
volatile int * B 	= (int *) 0x806400; 	// 8x8x4=256 B: 0x806400 - 0x8064FF
volatile int * C	= (int *) 0x806800; 	// 8x8x4=256 B: 0x806800 - 0x8068FF

#define n 8 					// tamaño de las matrices 8x8

int rank = 1; 					// hilo esclavo para nucleo= CPU2

#define size_dCache ALT_CPU_DCACHE_SIZE 	// tamano de la dCache, en system.h


// main : programa principal del hilo esclavo
//
int main()
{
	int message_buffer_val 			= 0x0;  // variable local, copia de variable global
	int message_buffer_val_fork 	= 0x0;  // variable local, copia de variable global
	int message_buffer_val_join 	= 0x0;  // variable local, copia de variable global
	int thread_count 				= 0;	// variable local, copia de variable global
	int Niter		 				= 0;	// variable local, copia de variable global
	*(message_buffer_ptr_fork)	|= 0;	// no hace nada, pero es necesario inicializar puntero porque si no se cuelga cuando se usa Nios II/s

  	 // direccion del dispositivo mutex de exclusion mutua 
	alt_mutex_dev* mutex = altera_avalon_mutex_open("/dev/message_buffer_mutex");

	//
	// FORK - Sincronizacion de Distribucion
	// Esclavo inicializa variables compartidas: *(message_buffer_ptr_fork) |= 2
	// cuando maestro inicializa : *(message_buffer_ptr) = 0x15
	// Si el maestro observa que ambos hilos estan sincronizados, maestro inicializa *(message_buffer_ptr) = 5
	// Ademas, el siguiente while se usa para leer:
	//	  thread_count 	: numero de hilos activados
	//	  Niter		: numero de iteraciones del computo
	//
	while(message_buffer_val != 5) {
		altera_avalon_mutex_lock(mutex,2);					// bloquea mutex 

		message_buffer_val 	= *(message_buffer_ptr); 			// lee valor almacenado en RAM 
		thread_count		= *(message_buffer_threads);		// lee valor almacenado en RAM
		Niter				= *(message_buffer_Niter);			// lee valor almacenado en RAM

		if(message_buffer_val == 15 && thread_count == 2) {
			message_buffer_val_fork 	= *(message_buffer_ptr_fork); 	// lee valor almacenado en RAM 
			message_buffer_val_fork 	|= 2;				// inicializa variable compartida
			*(message_buffer_ptr_fork) 	= message_buffer_val_fork;	// escribe valor en RAM
		}

		altera_avalon_mutex_unlock(mutex); 					// libera mutex 
	}

	//
	// COMPUTO ESCLAVO - Operacion Matriz x Matriz - repetido Niter veces
	// 2 hilos: cada hilo calcula la mitad de elementos de la matriz resultado: C
	// Hilo esclavo: la mitad inferior de la matriz C
	//
	/* Variables for matrix multiplication:
	 * i: row index for matrices A and C
	 * j: column index for matrices B and C 
	 * k: index for dot product calculation
	 * k1: iteration counter for repeated matrix multiplications
	 * cij: temporary variable to store element C[i][j] during calculation
	 */
	int i, j, k, k1, iteraciones = 0, cij;
	int local_n 	 = n / thread_count;		// numero de filas por hilo
	int my_first_row = rank * local_n;			// 1a fila asignada a este hilo
	int my_last_row  = (rank+1) * local_n - 1;	// ultima fila asignada a este hilo

	for (k1 = 0; k1 < Niter; k1++) {				// Repite el calculo Niter veces	
	   	iteraciones++;
		for (i=my_first_row; i<=my_last_row; i++){	// Bucle sobre las filas de C y A
	       		for (j=0; j<n; j++){				// Bucle sobre las columnas de B y C
				cij = C[i*n+j];  					// cij ← C[i][j] - Carga el valor de C[i][j] en cij
				for(k=0; k<n; k++){					// Bucle sobre las columnas de A y B
					cij += A[i*n+k] * B[k*n+j]; 	// cij += A[i][k]*B[k][j] - Multiplica A[i][k] y B[k][j] y suma el resultado a cij
				}
				C[i*n+j] = cij;  					// C[i][j] ← cij - Almacena el resultado en C[i][j]
		   	}
	    }
	}

	//
	// JOIN - Sincronizacion de union
	// Esclavo inicializa variable compartidas:
	//    *(message_buffer_ptr_join) |= 2
	//
	while(message_buffer_val != 6 && thread_count == 2) {
		altera_avalon_mutex_lock(mutex,2);				// bloquea mutex 

		message_buffer_val 		= *(message_buffer_ptr); 	// lee valor de variable compartida 

		message_buffer_val_join 	= *(message_buffer_ptr_join); 	// lee valor de variable compartida 
		message_buffer_val_join 	|= 2;				// indica que hilo esclavo ha llegado a JOIN
		*(message_buffer_ptr_join) 	= message_buffer_val_join;	// escribe valor en RAM

		altera_avalon_mutex_unlock(mutex); 				// libera mutex 
	}

  	return 0;
}
