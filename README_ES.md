# Firmware de cámara ESP32-CAM para rover

![Plataforma](https://img.shields.io/badge/platform-ESP32-blue)
![Placa](https://img.shields.io/badge/board-AI%20Thinker%20ESP32--CAM-00979D)
![Framework](https://img.shields.io/badge/framework-Arduino-00979D)
![Red](https://img.shields.io/badge/networking-STA%20%2B%20AP%20de%20rescate-success)
![Estado](https://img.shields.io/badge/status-Espec%C3%ADfico%20del%20proyecto-orange)

Firmware para una cámara montada en un rover, basado en la **AI Thinker ESP32-CAM**.

Este firmware está pensado para una **topología de red fija del rover**:

- la cámara se conecta normalmente al rover en **modo STA Wi-Fi**,
- la cámara usa siempre una **IP estática** dentro de la red del rover,
- solo se arranca un **AP de rescate** cuando el rover no es accesible,
- el firmware reintenta periódicamente la conexión al rover y sale del modo rescate de forma automática.

El objetivo del diseño es tener un comportamiento determinista, menor complejidad al arrancar y una recuperación más sencilla en campo.

---

## Puntos destacados

- Orientado a **AI Thinker ESP32-CAM**
- Inicialización de cámara en **JPEG** con tamaño por defecto **VGA**
- **IP STA fija** en la red del rover: `192.168.4.2`
- **AP de rescate** en una subred distinta: `192.168.8.1`
- Reconexión automática cada **10 segundos** mientras está en modo rescate
- Configuración adaptada a la presencia de PSRAM
- Soporte opcional de flash LED mediante `setupLedFlash()`
- Evita `WIFI_AP_STA` durante el arranque normal para reducir complejidad y picos de consumo

---

## Topología de red

### Modo normal

La cámara intenta unirse al punto de acceso del rover como estación.

| Parámetro | Valor |
|---|---|
| SSID del rover | `ROVER_DIEGO` |
| Contraseña del rover | `12341234` |
| IP STA de la cámara | `192.168.4.2` |
| Gateway / AP del rover | `192.168.4.1` |
| Máscara | `255.255.255.0` |

### Modo rescate

Si el rover no está disponible durante el arranque, la cámara crea su propio punto de acceso.

| Parámetro | Valor |
|---|---|
| SSID de rescate | `CAMARA_RESCATE` |
| Contraseña de rescate | `12341234` |
| IP del AP de rescate | `192.168.8.1` |
| Máscara | `255.255.255.0` |

### Diagrama de red en ASCII

```text
Funcionamiento normal
---------------------
Móvil / PC / Control del rover
               |
               v
   [ AP / Gateway del rover ]   SSID: ROVER_DIEGO
            192.168.4.1
               |
               v
      [ Cámara ESP32-CAM ]
            192.168.4.2

Funcionamiento de rescate
-------------------------
Móvil / PC
    |
    v
[ AP de rescate ESP32-CAM ]   SSID: CAMARA_RESCATE
           192.168.8.1
```

Se usa una subred de rescate distinta para evitar ambigüedades de encaminamiento con la red del rover.

---

## Arranque y comportamiento en ejecución

### Secuencia de arranque

1. Arranca la depuración serie a `115200` baudios.
2. Configura los pines de la cámara AI Thinker mediante `camera_pins.h`.
3. Inicializa el sensor de cámara y los buffers.
4. Aplica ajustes específicos de sensor/placa cuando corresponde.
5. Inicializa el flash LED si existe `LED_GPIO_NUM`.
6. Intenta conectarse al rover en **modo STA**.
7. Si STA falla, arranca el **AP de rescate**.
8. Arranca el servidor web de la cámara.

### Lógica de funcionamiento

- Si STA está conectado, la cámara permanece en la red del rover.
- Si STA no está disponible, el AP de rescate mantiene la cámara accesible.
- Cada `10000 ms`, el firmware reintenta la conexión con el rover.
- Si STA vuelve a estar disponible, el AP de rescate se desactiva automáticamente.
- Si se pierde una conexión STA ya establecida, el firmware vuelve al ciclo de reconexión.

---

## Configuración de cámara

El sketch configura la cámara de esta forma:

- **Modelo:** `AI Thinker ESP32-CAM`
- **Formato de píxel:** `JPEG`
- **Tamaño por defecto:** `VGA`
- **XCLK:** `10 MHz`
- **Ubicación por defecto del framebuffer:** PSRAM cuando está disponible
- **Calidad JPEG por defecto:** `12`
- **Modo con PSRAM:** mejor calidad y doble buffer cuando existe

Cuando se detecta PSRAM, el firmware pasa a una configuración más capaz:

- calidad JPEG mejorada a `10`
- número de frame buffers aumentado a `2`
- modo de captura cambiado a `CAMERA_GRAB_LATEST`

Si no hay PSRAM, el sketch usa una configuración más conservadora.

---

## Acceso a la interfaz web

Abre la cámara desde un navegador usando una de estas direcciones:

- **Modo normal:** `http://192.168.4.2`
- **Modo rescate:** `http://192.168.8.1`

La interfaz exacta y el conjunto de endpoints dependen de los ficheros del servidor de cámara que estén enlazados en la compilación. En una integración estándar del servidor web de cámara ESP32, la página raíz suele exponer previsualización, streaming, captura y controles de cámara.

---

## Estructura del repositorio

Una estructura típica del repositorio para este firmware podría ser:

```text
.
├── CameraWebServer.ino
├── camera_pins.h
├── app_httpd.cpp          # si usas la UI/servidor estándar de cámara ESP32
├── camera_index.h         # si la implementación elegida lo necesita
├── README.md
└── LICENSE
```

Como mínimo, la compilación debe proporcionar:

- `CameraWebServer.ino`
- `camera_pins.h`
- una implementación de:
  - `startCameraServer()`
  - `setupLedFlash(int pin)`

---

## Requisitos de compilación

Entorno recomendado:

- **Arduino IDE** o flujo compatible con ESP32 Arduino
- **Paquete de placas ESP32 de Espressif**
- **Placa seleccionada:** `AI Thinker ESP32-CAM`
- PSRAM habilitada cuando corresponda
- un esquema de particiones con espacio suficiente para firmware de cámara

### Cabeceras requeridas

- `esp_camera.h`
- `WiFi.h`
- `soc/soc.h`
- `soc/rtc_cntl_reg.h`

---

## Valores de configuración en el sketch

La versión actual es intencionadamente **específica del proyecto** y usa valores de red embebidos:

```cpp
char wifi_ssid[33] = "ROVER_DIEGO";
char wifi_pass[65] = "12341234";

static const char* RESCUE_AP_SSID = "CAMARA_RESCATE";
static const char* RESCUE_AP_PASS = "12341234";

IPAddress sta_local_IP(192, 168, 4, 2);
IPAddress sta_gateway(192, 168, 4, 1);
IPAddress ap_local_IP(192, 168, 8, 1);
```

### Reconfiguración remota de Wi-Fi

Los cambios remotos de Wi-Fi están deshabilitados intencionadamente en este sketch.

La función:

```cpp
void saveWiFiConfig(const String& s, const String& p)
```

registra la petición, pero no la aplica.

Esto protege la topología fija rover/cámara frente a cambios remotos accidentales.

---

## Diagnóstico por puerto serie

El puerto serie muestra información útil, incluyendo:

- transiciones de estado Wi-Fi
- SSID y BSSID asociados
- IP STA y gateway
- valores RSSI
- activación del AP de rescate
- reintentos de conexión
- errores de inicialización de cámara

Velocidad recomendada del monitor serie:

```text
115200
```

---

## Troubleshooting

### 1. La cámara no conecta con el rover

Comprueba:

- que el AP del rover realmente está activo en `192.168.4.1`
- que SSID y contraseña coinciden con el sketch
- que la subred del rover es `192.168.4.0/24`
- que la IP estática `192.168.4.2` no la usa ya otro equipo

Si la conexión falla, la cámara debería exponer el AP `CAMARA_RESCATE` en `192.168.8.1`.

### 2. La página web no carga aunque el ping responde

Esto suele apuntar a los **ficheros del servidor web de cámara** más que al enlace Wi-Fi.

Comprueba que la compilación incluye los ficheros correctos que implementan:

- `startCameraServer()`
- los handlers HTTP necesarios para la UI/servidor elegido
- `camera_index.h` o recursos equivalentes si tu servidor los necesita

### 3. Error de compilación como `index_handler was not declared`

Suele significar que `app_httpd.cpp` o los ficheros asociados de la UI están incompletos, mezclados o desincronizados.

Asegúrate de que todos los ficheros del servidor web pertenecen al mismo conjunto de implementación.

### 4. La placa arranca pero falla la inicialización de cámara

Comprueba:

- que la placa seleccionada es **AI Thinker ESP32-CAM**
- que el mapeo de pines de cámara corresponde a esa placa
- que la alimentación es estable
- que la PSRAM está correctamente configurada

### 5. El AP de rescate aparece demasiado a menudo

Eso suele indicar que la asociación STA es inestable o que el rover aún no está listo cuando arranca la cámara.

Causas típicas:

- el AP del rover arranca demasiado tarde
- señal débil
- credenciales incorrectas
- inestabilidad de alimentación durante el arranque Wi-Fi

### 6. El navegador funciona en modo rescate pero no en la red del rover

Eso suele indicar un problema de red en el lado del rover:

- gateway o máscara incorrectos en el rover
- otro dispositivo usando ya `192.168.4.2`
- firewall, portal cautivo o lógica de routing en el lado del controlador

---

## Criterio de diseño

El sketch evita intencionadamente ejecutar AP y STA a la vez en la ruta normal de arranque.

Ventajas:

- comportamiento de red más determinista
- menor complejidad al arrancar
- menos riesgo de picos transitorios de consumo
- diagnóstico de campo más sencillo
- separación más clara entre **funcionamiento normal** y **modo de recuperación**

No es un sketch genérico de provisión Wi-Fi. Es un nodo de cámara adaptado a un sistema rover concreto.

---

## Limitaciones

- credenciales Wi-Fi embebidas
- esquema de IP estática embebido
- cambios remotos de Wi-Fi deshabilitados
- el comportamiento depende de la implementación del servidor web de cámara enlazada
- diseñado para una topología de red concreta del rover, no para despliegue genérico de consumo

---

## Descripción sugerida para el repositorio

**Firmware ESP32-CAM para cámara de rover con red STA fija, AP de rescate y reconexión automática.**

---

## Ideas de evolución

Mejoras posibles:

- mover credenciales a almacenamiento no volátil
- añadir una página de estado con modo actual y RSSI
- exponer endpoints JSON de salud/diagnóstico
- añadir lógica de recuperación respaldada por watchdog
- añadir un modo opcional de configuración autenticada

---

## Licencia

Añade la licencia que encaje con tu repositorio, por ejemplo:

- MIT
- GPL-3.0
- Apache-2.0

Si todavía no tienes una, añade un fichero `LICENSE` en la raíz del repositorio.
