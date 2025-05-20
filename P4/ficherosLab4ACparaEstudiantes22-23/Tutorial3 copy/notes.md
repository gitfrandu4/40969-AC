---
# Tutorial 3 – Multiplicación Matriz × Vector (MV)  
**Práctica 4 – Arquitectura de Computadores**

> **Objetivo:** implementar y medir la versión secuencial y paralela (1 hilo y 2 hilos) del algoritmo `y = A × x` en los multiprocesadores **DualCore Nios II/e** y **DualCore Nios II/s**, calcular *speed‑up* y eficiencia, y analizar los resultados.

---

## 1 . Preparación del entorno

| Carpeta | Contenido | Propósito |
|---------|-----------|-----------|
| **DualCoreNios2e / DualCoreNios2s** | `*.sof`, `*.sopcinfo` | Configuración FPGA |
| **Tutorial3** | `MV_serie_22-23.c`, `MV_paralelo_maestro_22-23.c`, `MV_paralelo_esclavo_22-23.c` | Código fuente |

1. Inicia la máquina virtual **Altera**.  
2. Abre **SBT for Eclipse** y la **Nios II Command Shell**.  
3. Conecta la placa **DE0‑Nano** por USB.

---

## 2 . Versión secuencial (`MV_serie`)

1. **Crear proyecto**

```text
File ▸ New ▸ Nios II App & BSP from Template
SOPC Info : DE0_Nano_DualCoreNios2e.sopcinfo
CPU       : CPU
Template  : Hello World Small
Project   : MV_serie
```
Reemplaza `hello_world.c` por `MV_serie_22-23.c`.

2. **Configurar timer**

```text
[BSP] ▸ Nios II ▸ BSP Editor
timestamp_timer = interval_timer
Save ▸ Generate
```

3. **Compilar y ejecutar**

```bash
nios2-configure-sof DE0_Nano_DualCoreNios2e.sof
nios2-download  -r -g -i 0 MV_serie.elf
nios2-terminal
```

Repite con `DE0_Nano_DualCoreNios2s.sof`.

4. **Anota tiempos** para `Niter = 1000, 2000, 5000, 10000` en **Tabla 1**.

---

## 3 . Versión paralela (2 hilos)

### 3.1 Crear proyectos

| Proyecto | CPU | Fuente | Timer |
|----------|-----|--------|-------|
| `MV_paralelo_maestro` | CPU  | `MV_paralelo_maestro_22-23.c` | **Sí** |
| `MV_paralelo_esclavo` | CPU2 | `MV_paralelo_esclavo_22-23.c` | No |

> Configura el timer solo en el **BSP del maestro**.

### 3.2 Sincronización fork / join

| Fase  | Variable compartida | Maestro | Esclavo |
|-------|---------------------|---------|---------|
| **Fork**  | `message_buffer_ptr_fork` | Escribe `0x1` | OR `0x2` |
| **Cómputo** | `A`, `x`, `y`           | Filas 0‑7 | Filas 8‑15 |
| **Join**  | `message_buffer_ptr_join` | OR `0x1`, espera `0x3` → fija `6` | OR `0x2` |

> La exclusión mutua se realiza con `altera_avalon_mutex_lock()`.

### 3.3 Descarga y ejecución

```bash
# Configuración FPGA (e)
nios2-configure-sof DE0_Nano_DualCoreNios2e.sof

# 1 hilo
nios2-download -r -g -i 0 MV_paralelo_maestro.elf   # thread_count = 1

# 2 hilos
nios2-download -r -g -i 0 MV_paralelo_maestro.elf   # thread_count = 2
nios2-download -r -g -i 1 MV_paralelo_esclavo.elf
```

Registra los tiempos **Fork**, **Cómputo**, **Join** y **Total** para los mismos valores de `Niter`.  
Repite todo con la configuración **s**.

### 3.4 Tablas de resultados

Completa las tablas y calcula:

```text
SpeedUp    = T_1hilo / T_2hilos
Eficiencia = 100 × SpeedUp / 2
```

---

## 4 . Errores comunes

| Síntoma | Causa | Solución |
|---------|-------|----------|
| Tiempo = 0 ms | Timer mal configurado | Revisa `timestamp_timer`, vuelve a **Generate** y ejecuta `make`. |
| Bloqueo en Fork/Join | Orden de descarga incorrecto | Descarga primero el maestro (CPU) y luego el esclavo (CPU2). |

---

## 5 . Checklist de entrega

- Archivos C (`MV_serie`, `MV_paralelo_maestro`, `MV_paralelo_esclavo`)  
- **Tabla 1** y **Tabla 2** con tiempos, *speed‑up* y eficiencia  
- Respuestas justificadas a las preguntas de análisis  

---

## 6 . Preguntas guía

1. **Escalado de `Niter`**  
   - ¿Por qué `T ∝ Niter` en Nios II/e?

2. **Comparación e vs s**  
   - Explica la diferencia de prestaciones entre ambos núcleos.

3. **Paralelismo con 2 hilos**  
   - Justifica los valores de *speed‑up* y eficiencia obtenidos.

4. **Impacto de Fork/Join**  
   - Relaciona los tiempos de sincronización con el *overhead* total.

---

End of file
