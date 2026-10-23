# Guía de Conexiones de Hardware - Brazo Robótico Clasificador

Este documento detalla el esquema de cableado para el proyecto del brazo robótico con detección de metales. El sistema está basado en un microcontrolador ESP32-S3, utilizando aislamiento óptico para los sensores industriales y un bus I2C para el control de servos.

---

## 1. Distribución de Energía (Potencia y Lógica)

La alimentación principal proviene de una fuente de 12V / 20A. **Por seguridad, el polo positivo de 12V pasa primero por un fusible.** Después del fusible, la señal se considera "12V Seguros" y se divide en dos módulos Step-Down XL4016.

### Módulo XL4016 #1 (Potencia para Servomotores)
*   **Entrada (IN+):** Conectado a los "12V Seguros".
*   **Entrada (IN-):** Conectado al COM/GND de la fuente de 12V.
*   **Salida (OUT+):** Ajustada exactamente a **5.5V**.
*   **Salida (OUT-):** Tierra (GND).
*   **Filtro (Capacitor 1):** Un capacitor electrolítico de 2200 µF conectado directamente en los tornillos de salida. Pata larga a OUT+, pata corta (franja) a OUT-.

### Módulo XL4016 #2 (Lógica para ESP32 y PCA9685)
*   **Entrada (IN+):** Conectado a los "12V Seguros".
*   **Entrada (IN-):** Conectado al COM/GND de la fuente de 12V.
*   **Salida (OUT+):** Ajustada exactamente a **5.0V**.
*   **Salida (OUT-):** Tierra (GND).

---

## 2. Lógica y Control (ESP32-S3 y PCA9685)

El ESP32-S3 actúa como cerebro principal, alimentado de forma limpia por el módulo XL4016 #2.

### Alimentación del ESP32-S3
*   **Pin VIN (o 5V):** Conectado al OUT+ (5.0V) del XL4016 #2.
*   **Pin GND:** Conectado al OUT- (GND) del XL4016 #2.

### Driver de Servos PCA9685
**Potencia (Bloque de tornillos verde):**
*   **V+:** Conectado al OUT+ (5.5V) del XL4016 #1.
*   **GND:** Conectado al OUT- del XL4016 #1.
*   **Filtro (Capacitor 2):** Un capacitor de 2200 µF conectado en el bloque verde. Pata larga a V+, pata corta a GND (en paralelo al capacitor 1 para inyección rápida de corriente).

**Lógica (Pines laterales macho):**
*   **VCC:** Conectado al pin 3.3V del ESP32-S3.
*   **GND:** Conectado a un pin GND del ESP32-S3.
*   **SDA:** Conectado al **GPIO 8** del ESP32-S3.
*   **SCL:** Conectado al **GPIO 9** del ESP32-S3.

**Conexión de Servos (MG90S):**
*   Canales 0 al 3 (Base, Extensión, Elevación, Pinza). 
*   Cable Marrón -> Pin inferior (GND).
*   Cable Rojo -> Pin central (V+).
*   Cable Naranja -> Pin superior (PWM).

---

## 3. Sensores y Entradas Periféricas

### Sensores Inductivos (LJ12A3-4-Z/BX) y Optoacopladores (PC817)
Dado que los sensores son NPN y trabajan a 12V, se aíslan del ESP32 (3.3V) mediante el módulo optoacoplador PC817 de 4 canales.

*   **Cable Marrón (Sensor 1 y 2):** A los "12V Seguros".
*   **Cable Azul (Sensor 1 y 2):** A GND (COM de la fuente).
*   **Lado de Entrada Optoacoplador (`IN1, G` / `IN2, G`):**
    *   `IN1` (y `IN2`): Conectados a los "12V Seguros" (`12V+`) junto con el cable **Marrón** del sensor.
    *   `G` (enseguida de `IN1`): Cable **Negro** del Sensor Inductivo 1.
    *   `G` (enseguida de `IN2`): Cable **Negro** del Sensor Inductivo 2.
    *   **¡Importante!** **Retirar todos los jumpers centrales** del módulo PC817 (si se dejan puestos, unen los terminales `G` a tierra y mantienen los optoacopladores encendidos permanentemente).
*   **Lado de Salida Optoacoplador (`V1, G` / `V2, G`):**
    *   `G` (Canal 1 y 2): Conectados a **GND** del ESP32-S3.
    *   `V1` (Salida Sensor 1): Conectado al **GPIO 7** (o **GPIO 4**) del ESP32-S3.
    *   `V2` (Salida Sensor 2): Conectado al **GPIO 4** (o **GPIO 7**) del ESP32-S3.

### Finales de Carrera (Límites de Rotación de Base - Izquierda / Derecha)
Utilizan interruptores mecánicos conectados en configuración Pull-Up para limitar el giro del eje Base (Canal 0).

*   **Final de Carrera 1 (Límite Izquierda):** Terminal `COM` a GND. Terminal `NO` (Normalmente Abierto) al **GPIO 5** del ESP32-S3.
*   **Final de Carrera 2 (Límite Derecha):** Terminal `COM` a GND. Terminal `NO` (Normalmente Abierto) al **GPIO 6** del ESP32-S3.

### Sensor Ultrasónico de Distancia (HC-SR04)
El sensor HC-SR04 estándar opera a **5.0V**, por lo que se alimenta desde el **Módulo XL4016 #2 (5.0V)**.

*   **Pin VCC:** Conectado al **OUT+ (5.0V)** del Módulo XL4016 #2 (el mismo riel de 5V que alimenta el pin VIN del ESP32-S3).
*   **Pin GND:** Conectado a **GND** del ESP32-S3 / OUT- del XL4016 #2.
*   **Pin TRIG (Disparo):** Conectado directamente al **GPIO 10** del ESP32-S3 (el pulso de 3.3V del ESP32 es reconocido perfectamente como `HIGH` por el HC-SR04).
*   **Pin ECHO (Respuesta - ¡Importante Protección 3.3V!):** Conectado al **GPIO 11** del ESP32-S3.
    *   *Nota eléctrica:* Si tu módulo es el **HC-SR04 clásico (5V)**, el pin `ECHO` entrega un pulso de **5.0V** que puede dañar el GPIO del ESP32-S3 (tolerante solo a 3.3V). Se debe colocar un **divisor de tensión** simple (una resistencia de **1 kΩ** entre `ECHO` y **GPIO 11**, y una de **2 kΩ** entre **GPIO 11** y **GND**), o bien usar un canal libre (Canal 3) de tu módulo optoacoplador PC817. *(Si tu módulo es la versión **HC-SR04P / HC-SR04+**, puede alimentarse directamente a 3.3V sin resistencias).*

---

## 4. Tabla Resumen de Mapeo GPIO (ESP32-S3)

Esta tabla es la referencia principal para programar el microcontrolador.

| Componente | Pin ESP32-S3 | Configuración en Código (C/C++) | Función |
| :--- | :--- | :--- | :--- |
| **PCA9685 (I2C)** | `GPIO 8` | `Wire.begin(8, 9);` | Línea SDA (Datos I2C) |
| **PCA9685 (I2C)** | `GPIO 9` | `Wire.begin(8, 9);` | Línea SCL (Reloj I2C) |
| **Sensor Inductivo 1** | `GPIO 7` | `pinMode(7, INPUT_PULLUP);` | Detecta metal (Lee LOW) - Dispara rutina |
| **Sensor Inductivo 2** | `GPIO 4` | `pinMode(4, INPUT_PULLUP);` | Detecta metal (Lee LOW) - Dispara rutina |
| **Final de Carrera 1** | `GPIO 5` | `pinMode(5, INPUT_PULLUP);` | Límite Rotación Base Izquierda (Lee LOW) |
| **Final de Carrera 2** | `GPIO 6` | `pinMode(6, INPUT_PULLUP);` | Límite Rotación Base Derecha (Lee LOW) |
| **HC-SR04 (TRIG)** | `GPIO 10` | `pinMode(10, OUTPUT);` | Pulso de disparo ultrasónico (10 µs HIGH) |
| **HC-SR04 (ECHO)** | `GPIO 11` | `pinMode(11, INPUT);` | Medición de ancho de pulso de eco (3.3V máx.) |

> **Nota para programación:** Todos los pines de entrada (sensores inductivos y finales de carrera) se conectan a GND al activarse. Por lo tanto, tu lógica de código debe interpretar un estado `LOW` (0) como "Activado" o "Metal Detectado", y un estado `HIGH` (1) como "Reposo".

---

## 5. Especificación Funcional y Secuencia de Operación

### 5.1 Secuencia Automática de Operación
1. **Inicialización y Seguridad:** El sistema debe inicializarse, verificar las condiciones de seguridad (finales de carrera libres, PCA9685 en línea, sin fallas activas) y establecer una **posición inicial segura (`HOME`)** antes de operar.
2. **Detección y Verificación de Material:**
   * La secuencia espera una pieza en la zona de detección, **confirmando su presencia física con el sensor ultrasónico HC-SR04** (distancia menor o igual al umbral calibrado) durante un tiempo de estabilización.
   * Una vez confirmada la presencia del objeto, **verifica su material con los sensores inductivos (`GPIO 7` / `GPIO 4`)**.
3. **Acción según Material (Rutas Configurables):**
   * **Pieza Metálica:** El brazo ejecuta la **Ruta de Metal** (baja a la pieza, cierra la pinza, eleva la pieza, la traslada a la zona de descarga de metal y la libera).
   * **Pieza No Metálica:** Si la pieza no es metálica, el sistema puede:
     * **Modo Bloqueo / Retiro Manual (por defecto según norma):** No iniciar el ciclo de metal y **solicitar el retiro de la pieza**, o
     * **Modo Ruta No Metálica (si se habilita la ruta grabada en el portal):** Ejecutar la **Ruta de No Metal** grabada manualmente desde el portal cautivo hacia un segundo destino.
4. **Finalización de Ciclo:** Al terminar, el sistema registra el resultado en las estadísticas, vuelve a la posición inicial (`HOME`) y **espera a que la zona quede despejada (retiro de la pieza)** antes de habilitar un nuevo ciclo.

### 5.2 Control Estricto por Máquina de Estados (FSM)
Toda la lógica de operación se organiza estrictamente mediante una **Máquina de Estados Finitos (`INIT_CHECK`, `IDLE_WAIT_PART`, `VERIFY_MATERIAL`, `RUN_METAL_ROUTE`, `RUN_NON_METAL_ROUTE`, `WAIT_PART_REMOVAL`, `MANUAL`, `HOMING`, `FAULT_LOCKED`)**. Esta estructura evita que lecturas aisladas o intermitentes de los sensores activen movimientos inesperados en el brazo.

### 5.3 Manejo de Errores y Paros de Seguridad
* **Detección de Servos Atascados (Timeout de Movimiento):** Se limita el tiempo máximo permitido para completar cada movimiento. Si se supera dicho tiempo o se detecta un bloqueo, el sistema entra en estado de falla y solicita revisión.
* **Protección por Finales de Carrera (`GPIO 5` y `GPIO 6`):** El código detiene inmediatamente cualquier movimiento que amenace con sobrepasar el límite al activarse un final de carrera.
* **Enclavamiento de Seguridad:** El sistema **nunca reanudará automáticamente** movimientos potencialmente peligrosos tras presentarse una falla o condición de seguridad; requiere reconocimiento manual del operador desde el dashboard tras despejar la condición.

### 5.4 Límites de Movimiento y Calibración desde el Portal
* **Intervalo Seguro Inicial:** El software restringe inicialmente los ángulos de todas las articulaciones a un intervalo seguro de **15° a 165°**.
* **Ajuste Dinámico:** Desde el portal cautivo se pueden personalizar y guardar en memoria no volátil (NVS) los límites mínimos y máximos (`minAngle` / `maxAngle`) de cada uno de los 4 servos, el umbral de distancia del HC-SR04 y los tiempos de espera.
* **Grabación Manual de Rutas (*Teach-In*):** Una vez definidos los límites, el usuario puede mover manualmente el brazo y guardar paso a paso los puntos de la **Ruta para Metal** y la **Ruta para No Metal** antes de iniciar el modo automático.

### 5.5 Interfaz de Usuario y Punto de Acceso Wi-Fi (`bot-detector`)
* El ESP32-S3 levanta un punto de acceso Wi-Fi denominado **`bot-detector`** con Portal Cautivo en `http://192.168.4.1`.
* Permite alternar entre **Modo Manual / Calibración** y **Modo Automático**.
* **Bloqueo de Seguridad en Interfaz:** El sistema bloquea cualquier comando de movimiento manual proveniente de la interfaz web si existe una condición de seguridad activa (paro de emergencia, falla por atasco o intento de avanzar hacia un final de carrera presionado).