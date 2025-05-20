# Implementación Paralela de Multiplicación de Matrices

## 1. Introducción y Contexto

La multiplicación de matrices es una operación fundamental en computación científica y análisis numérico. La implementación paralela de este algoritmo es especialmente relevante en arquitecturas multiprocesador, donde podemos distribuir el cálculo entre diferentes unidades de procesamiento para mejorar el rendimiento.

En este trabajo, hemos modificado un programa de multiplicación matriz-vector para implementar una multiplicación matriz-matriz de tamaño 8×8, utilizando arquitectura dual-core Nios II. El código se ejecuta en dos núcleos que trabajan de manera cooperativa utilizando el paradigma maestro-esclavo.

## 2. Algoritmo de Multiplicación de Matrices

La multiplicación de matrices A×B=C para matrices cuadradas de tamaño n×n sigue este pseudocódigo:

```
void Matriz-Matriz (int n, int* A, int* B, int* C){
  int i,j,k,cij;
  for (i = 0; i < n; ++i)         /* i: fila de la matriz */
    for (j = 0; j < n; ++j)  {    /* j: columna de la matriz */
      cij = C[i*n+j];             /* cij ← C[i][j] */
      for (k = 0; k < n; k++)     /* cij: fila-A(i) x columna-B(j) */
        cij += A[i*n+k] * B[k*n+j]; /* cij += A[i][k]*B[k][j] */
      C[i*n+j] = cij;             /* C[i][j] ← cij */
    }
}
```

Este algoritmo tiene complejidad O(n³), lo que lo hace computacionalmente intensivo incluso para matrices de tamaño moderado. Por esta razón, es un excelente candidato para la paralelización.

## 3. Estrategia de Paralelización

La estrategia de paralelización implementada es una **distribución por filas**, donde:

- El conjunto de filas de la matriz resultado C se divide entre los núcleos disponibles
- Cada núcleo calcula un subconjunto de filas completas de la matriz C
- Para n=8 con 2 núcleos, cada núcleo calcula 4 filas de la matriz resultado

Específicamente:
- El núcleo maestro (CPU) calcula las filas 0-3 de C
- El núcleo esclavo (CPU2) calcula las filas 4-7 de C

## 4. Organización de Memoria

La memoria se organiza de la siguiente manera:

```
A: 0x806000 - 0x8060FF (8×8×4=256 bytes)
B: 0x806400 - 0x8064FF (8×8×4=256 bytes)
C: 0x806800 - 0x8068FF (8×8×4=256 bytes)
```

Las matrices se almacenan en memoria compartida accesible por ambos núcleos, permitiendo:
- Acceso de escritura a regiones específicas (cada núcleo escribe solo sus filas asignadas)
- Acceso de lectura a todas las matrices (cada núcleo necesita leer las matrices A y B completas)

## 5. Mecanismo de Sincronización Fork/Join

La ejecución paralela utiliza un patrón **fork/join** implementado mediante variables compartidas:

### Fase Fork (Inicialización y Distribución)
1. El maestro inicializa las matrices A, B y C
2. El maestro configura parámetros compartidos: número de hilos, iteraciones
3. El maestro establece `message_buffer_ptr_fork = 0x1` para indicar que está listo
4. El esclavo establece `message_buffer_ptr_fork |= 0x2` para indicar que está listo
5. El maestro detecta que ambos están listos (`0x3`) y establece `message_buffer_ptr = 5` para iniciar cálculo

### Fase Cómputo
1. Cada núcleo ejecuta el algoritmo para sus filas asignadas, durante `Niter` iteraciones
2. Se protege la lectura/escritura mediante exclusión mutua con `altera_avalon_mutex_lock/unlock`

### Fase Join (Reunión)
1. El maestro establece `message_buffer_ptr_join |= 0x1` para indicar fin de su cálculo
2. El esclavo establece `message_buffer_ptr_join |= 0x2` para indicar fin de su cálculo
3. El maestro detecta finalización (`0x3`) y establece `message_buffer_ptr = 6`
4. El maestro muestra los resultados y tiempos de ejecución

## 6. Cambios Principales Realizados

Comparado con la versión original matriz-vector, los cambios principales fueron:

1. **Definición de matrices**:
   - Reemplazado vectores `x` e `y` por matrices `B` y `C`
   - Cambiado tamaño de 16×16 a 8×8

2. **Inicialización de datos**:
   - Modificado `inicializaMemoria()` para inicializar tres matrices
   - Valores iniciales: `A[i*n+j] = i+j`, `B[i*n+j] = i-j`, `C[i*n+j] = 0`

3. **Algoritmo de cálculo**:
   - Implementado bucle triple para multiplicación matriz-matriz
   - Uso de acumulador `cij` para cada elemento de resultado

4. **Visualización de resultados**:
   - Adaptado el formato de salida para mostrar matrices completas

## 7. Análisis de Rendimiento

La implementación paralela puede evaluarse considerando:

1. **Speed-up**: Ratio entre tiempo secuencial y tiempo paralelo
   - Speed-up teórico máximo = número de procesadores (2 en este caso)
   - Speed-up práctico limitado por:
     - Overhead de sincronización (fork/join)
     - Contención de memoria compartida
     - Desbalance de carga (actualmente equilibrado)

2. **Eficiencia**: Speed-up / número de procesadores
   - 100% eficiencia equivale a speed-up ideal
   - Variables que afectan: número de iteraciones, tamaño de matrices

3. **Escalabilidad**:
   - La implementación actual es escalable hasta cierto punto
   - Para matrices más grandes, podría requerirse una estrategia de división diferente

## 8. Consideraciones Adicionales

1. **Localidad de datos**:
   - Cada núcleo procesa filas completas, mejorando la localidad espacial
   - El acceso a columnas de B es menos eficiente (posible mejora: transponer B)

2. **Balanceo de carga**:
   - División actual (4 filas por núcleo) asume núcleos homogéneos
   - En sistemas heterogéneos, una distribución dinámica sería mejor

3. **Sincronización**:
   - La sincronización actual es eficiente para 2 núcleos
   - Para más núcleos, sería preferible una barrera o semáforo más avanzado

## 9. Conclusiones

La paralelización de la multiplicación de matrices en arquitectura dual-core Nios II demuestra:

1. **Ventajas del paralelismo**:
   - División del trabajo entre núcleos
   - Potencial de mejora de rendimiento (speed-up)
   - Utilización eficiente de recursos multi-núcleo

2. **Desafíos**:
   - Sincronización entre núcleos
   - Gestión de memoria compartida
   - Balance entre paralelización y overhead

Esta implementación muestra cómo estructuras clásicas como fork/join pueden aplicarse eficientemente para paralelizar algoritmos con alta intensidad computacional, aprovechando la arquitectura multi-núcleo para mejorar el rendimiento en operaciones fundamentales del álgebra lineal.

## 10. Comparativa Detallada del Código Original vs Modificado

### 10.1. Declaración de Matrices y Memoria

**Código Original (Matriz-Vector):**
```c
// Original en ambos archivos (maestro y esclavo)
volatile int * A = (int *) 0x806000;   // 16x16x4=1KiB: 0x806000 - 0x8063FF
volatile int * x = (int *) 0x806400;   // 16x1 x4=64 B: 0x806400 - 0x80643F
volatile int * y = (int *) 0x806800;   // 16x1 x4=64 B: 0x806800 - 0x80683F

#define m 16                          // numero de columnas de las matrices
#define n 16                          // numero de filas de las matrices
```

**Código Modificado (Matriz-Matriz):**
```c
// Modificado en ambos archivos (maestro y esclavo)
volatile int * A = (int *) 0x806000;   // 8x8x4=256 B: 0x806000 - 0x8060FF
volatile int * B = (int *) 0x806400;   // 8x8x4=256 B: 0x806400 - 0x8064FF
volatile int * C = (int *) 0x806800;   // 8x8x4=256 B: 0x806800 - 0x8068FF

#define n 8                           // tamaño de las matrices 8x8
```

### 10.2. Algoritmo de Cálculo (Núcleo Maestro)

**Código Original (Matriz-Vector):**
```c
// Bucle de cálculo en MV_paralelo_maestro_22-23.c original
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
```

**Código Modificado (Matriz-Matriz):**
```c
// Bucle de cálculo en MV_paralelo_maestro_22-23.c modificado
for (k1 = 0; k1 < Niter; k1++) {
    iteraciones++;
    for (i=my_first_row; i<=my_last_row; i++){
        for (j=0; j<n; j++){
            cij = C[i*n+j];  // cij ← C[i][j]
            for(k=0; k<n; k++){
                cij += A[i*n+k] * B[k*n+j]; // cij += A[i][k]*B[k][j]
            }
            C[i*n+j] = cij;  // C[i][j] ← cij
        }
    }
}
```

### 10.3. Algoritmo de Cálculo (Núcleo Esclavo)

**Código Original (Matriz-Vector):**
```c
// Bucle de cálculo en MV_paralelo_esclavo_22-23.c original
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
```

**Código Modificado (Matriz-Matriz):**
```c
// Bucle de cálculo en MV_paralelo_esclavo_22-23.c modificado
for (k1 = 0; k1 < Niter; k1++) {
    iteraciones++;
    for (i=my_first_row; i<=my_last_row; i++){
        for (j=0; j<n; j++){
            cij = C[i*n+j];  // cij ← C[i][j]
            for(k=0; k<n; k++){
                cij += A[i*n+k] * B[k*n+j]; // cij += A[i][k]*B[k][j]
            }
            C[i*n+j] = cij;  // C[i][j] ← cij
        }
    }
}
```

### 10.4. Inicialización de Datos

**Código Original (Matriz-Vector):**
```c
// Inicialización en MV_paralelo_maestro_22-23.c original
void inicializaMemoria(int ini_printf){
   // ...
   for (i=0; i<n; i++){
      if (ini_printf == 0){
         x[i] = (int)i;
         y[i] = 0.0;
      }
      // ...
      for(j=0; j<m; j++){
         if (ini_printf == 0){
            A[i*m+j]=(int)j;
         }
         // ...
      }
   }
}
```

**Código Modificado (Matriz-Matriz):**
```c
// Inicialización en MV_paralelo_maestro_22-23.c modificado
void inicializaMemoria(int ini_printf){
   // ...
   for (i=0; i<n; i++){
      if (ini_printf == 0){
         for (j=0; j<n; j++) {
            A[i*n+j] = (int)i+j;
            B[i*n+j] = (int)i-j;
            C[i*n+j] = 0;
         }
      }
      // ...
   }
}
```

### 10.5. Impresión de Resultados

**Código Original (Matriz-Vector):**
```c
// Visualización en MV_paralelo_maestro_22-23.c original
else if (ini_printf == 1){
   printf("y[%2i]= %i", i, (int)y[i]);
   printf("\tx[%2i]= %2i", i, (int)x[i]);
   printf("\tA[%2i]= ", i);
   for(j=0; j<m; j++){
      printf("%i ", (int)A[i*m+j]);
   }
   printf("\n");
}
```

**Código Modificado (Matriz-Matriz):**
```c
// Visualización en MV_paralelo_maestro_22-23.c modificado
else if (ini_printf == 1){
   printf("C[%2i]= ", i);
   for (j=0; j<n; j++) {
      printf("%3i ", (int)C[i*n+j]);
   }
   printf("\n");
}
```

### 10.6. Diferencias en Paralelización

**Paralelización Original:**
- División de 16 filas entre 2 núcleos (8 filas cada uno)
- Maestro: filas 0-7, Esclavo: filas 8-15
- Operación matriz-vector (complejidad O(n²))

**Paralelización Modificada:**  
- División de 8 filas entre 2 núcleos (4 filas cada uno)
- Maestro: filas 0-3, Esclavo: filas 4-7
- Operación matriz-matriz (complejidad O(n³))

Esta comparativa detallada demuestra cómo se ha adaptado el código para implementar la multiplicación matriz-matriz manteniendo el mismo patrón de paralelización, pero aumentando la complejidad algorítmica y modificando la estructura de datos para usar matrices cuadradas.
