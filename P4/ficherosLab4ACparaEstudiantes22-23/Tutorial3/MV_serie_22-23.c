//
// AC - Practica 4 - version: junio 2022
//
// Multiplicacion Matriz x Vector, y = A . x
// Version Secuencial
// Tipo multiprocesador: 2 x Nios II/e, DualCoreNios2e.sof
// Tipo procesador: Nios II/e, nombre: CPU
//
//

#include <stdio.h>
#include <altera_avalon_mutex.h>		// para controlador exclusion mutua
#include <system.h>
#include "sys/alt_stdio.h" 			// para alt_putstr
#include <unistd.h> 				// para usleep
// Timer, incluir el timestamp en BSP: boton dcho en BSP folder, Nios2 > BSP editor > cambiar system timer y timestamp timer
#include <sys/alt_timestamp.h>

// Zona de memoria compartida para matriz A y vectores x, y
volatile int * A	= (int *) 0x806000; 	// 16x16x4=1KiB: 0x806000 - 0x8063FF
volatile int * x 	= (int *) 0x806400; 	// 16x1 x4=64 B: 0x806400 - 0x80643F
volatile int * y	= (int *) 0x806800; 	// 16x1 x4=64 B: 0x806800 - 0x80683F

#define m 16 					// numero de columnas de las matrices
#define n 16 					// numero de filas de las matrices

int rank = 0; 					// hilo maestro para nucleo= CPU

// Las siguientes constantes se encuentran definidas en fichero system.h
#define tipoNios2   ALT_CPU_CPU_IMPLEMENTATION 	// "fast" (Nios II/f), "tiny" (Nios II/e), "small" (Nios II/s)
#define nombreNios2 ALT_CPU_NAME 		// "CPU" (nucleo 1), "CPU2" (nucleo 2)
#define size_dCache ALT_CPU_DCACHE_SIZE		// tamanyo de la dCache

// inicializaMemoria : Inicializa zona memoria compartida
//
// ini_printf=0: inicializa valores de las matrices A, x, y
// ini_printf=1: printf de valores A, x, y
// ini_printf=2: printf de direcciones A, x, y
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

// main : programa principal
//
int main()
{
	 int thread_count	= 1    ; 	// opciones: 1,2; numero de hilos
	 int Niter		= 30000; 	// opciones: 1000,2000,10000,20000,...; veces repite matriz-vector
	 int dumy, start, iteraciones = 0, timeInterval = 0, timeInterval2 = 0;
	 alt_u32 freq=0;
	 unsigned int time[5];
	 char etiqueta_time[6][6]={"tStar","tInic","tFork","tComp","tJoin","tFina"};

  	 alt_putstr("\n\nMatriz x Vector Secuencial - CPU - BEGIN\n");
  	 printf("\tNombre procesador Nios II\t: %s\n", nombreNios2);
  	 printf("\tTipo procesador Nios II\t\t: %s\n", tipoNios2);
  	 printf("\tTamano dCache Nios II\t\t: %i bytes\n", size_dCache);
  	 printf("\tHilos\t\t\t\t: Secuencial \n\tIteraciones\t\t\t: %i\n", Niter);

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

	inicializaMemoria(0);			// se inicializan los valores de A,x,y
	inicializaMemoria(1);			// se visualizan los valores de la matriz A

	// time1: marca tiempo inicial y final de Inicializacion
	time[1] = alt_timestamp();

	//
	// COMPUTO MAESTRO - Operacion Matriz x Vector - repetido Niter veces
	// 2 hilos: cada hilo calcula la mitad de filas de la matriz C
	//
	// time2: marca tiempo inicial computo y final de FORK

	time[2] = alt_timestamp();

	int i, j, k, k1;
	int local_n 	 = n ;
	int my_first_row = 0;		// 1a fila asignada a este nucleo
	int my_last_row  = local_n - 1; // ultima fila asignada a este nucleo

	printf("\nEmpieza el computo matriz-vector, Numero iteraciones: %i\n", Niter);
	for (k1 = 0; k1 < Niter; k1++) {
	   	iteraciones++;
		for (i=my_first_row; i<=my_last_row; i++){
	       		dumy = y[i];
		   	for(j=0; j<m; j++){
			   dumy += A[i*m+j] * x[j];
		   	}
		   	y[i] = dumy;
	    	}
	}
	time[3] = alt_timestamp();

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

	inicializaMemoria(1);			// se visualiza los valores de la matriz C resultante

	alt_putstr("\nFin del programa y reseteadas las variables de sincronizacion\n");

  	return 0;
}
