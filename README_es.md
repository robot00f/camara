# 🌐 Micro-Fetcher HTTPS (para Cámaras IP y Linux Embebido)

> [!NOTE]
> Documentación disponible también en [English](README.md).

**Micro-Fetcher** es un cliente HTTPS/TLS ultraligero (~112 KB) escrito en C y enlazado estáticamente con [BearSSL](https://bearssl.org/). Fue diseñado específicamente para dispositivos IoT y cámaras IP con almacenamiento flash extremadamente reducido (SoC Xiongmai XM530, HiSilicon, Ingenic, Allwinner, etc.) donde:

* No existe `curl` ni `wget` con soporte SSL/TLS en el firmware.
* Las particiones flash de configuración (`/mnt/mtd` en JFFS2/CramFS) tienen menos de 120 KB de espacio disponible.
* El kernel impone protección de memoria estricta (W^X), lo que impide utilizar descompresores dinámicos como UPX.
* Se requiere descargar binarios, scripts o actualizaciones directamente desde la web (ej. GitHub por HTTPS / TLS 1.2) hacia la memoria RAM (`/tmp`).

---

## ⚡ Características del Fetcher

- **Ultracompacto:** Pesa únicamente **112 KB** sin comprimir (entra cómodamente en bloques flash JFFS2 de 64 KB).
- **Criptografía Moderna:** Implementa **TLS 1.2** con intercambio de claves **ECDHE-RSA** y cifrado autenticado **AES-128-GCM** (SHA-256).
- **Zero Dependencies:** Binario estático compilado contra musl libc. No requiere certificados CA externos ni librerías dinámicas (`libssl`, `libcrypto`, `glibc`).
- **Compatible con GitHub:** Diseñado para negociar TLS 1.2 contra servidores CDN como Fastly y `raw.githubusercontent.com`.

---

## 🚀 Uso de `fetcher`

### Sintaxis
```bash
./fetcher <host> <ruta_con_query> <archivo_salida>
```

### Ejemplo: Descargar desde GitHub directamente en la cámara
```sh
# Descarga un archivo directamente a /tmp
/mnt/mtd/fetcher raw.githubusercontent.com /robot00f/camara/main/wol_sentinel /tmp/wol_sentinel

# Dar permisos de ejecución
chmod +x /tmp/wol_sentinel
```

---

## 🛠️ Compilación de `fetcher`

El cliente está optimizado para compilarse con **Zig CC** enlazando BearSSL de forma estática y eliminando secciones y tablas de desenrollado (*unwind tables*) para minimizar el tamaño del ejecutable:

```bash
zig cc -target arm-linux-musleabi -Os -s -flto \
  -fno-asynchronous-unwind-tables -fno-unwind-tables \
  -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -I./bearssl/inc fetcher.c libbearssl.a -o fetcher
```

---

## 🛰️ Módulo Secundario: WoL Sentinel (Wake-on-LAN Watchdog)

Como caso de uso práctico, este repositorio incluye **WoL Sentinel**, un centinela de red escrito en Go diseñado para desplegarse mediante `fetcher` en la cámara IP.

### ¿Qué hace?
Monitorea la red local en segundo plano escuchando paquetes mágicos **Wake-on-LAN (UDP 9 y 7)**. Cuando detecta un paquete dirigido a una PC vigilada, envía una alerta instantánea a un grupo de **Telegram** con la IP de origen, el puerto y la hora exacta.

### Ejecución
```sh
/tmp/wol_sentinel \
  -mac "AA:BB:CC:DD:EE:FF" \
  -token "123456789:ABCDEF_tu_bot_token" \
  -chat "-100123456789" \
  -log "/tmp/wol.log" > /tmp/sentinel.stdout 2>&1 &
```

### Parámetros de WoL Sentinel

| Parámetro | Variable de Entorno | Descripción | Valor por defecto |
| :--- | :--- | :--- | :--- |
| `-mac` | `TARGET_MAC` | MAC de la PC a vigilar. Si se omite, alerta sobre cualquier paquete WoL en la red. | *(Todas las MACs)* |
| `-token` | `BOT_TOKEN` | Token de la API de Telegram Bot. | *(Opcional)* |
| `-chat` | `CHAT_ID` | ID del Chat o Grupo de Telegram destinatario. | *(Opcional)* |
| `-log` | `LOG_PATH` | Ruta del archivo de registro local. | `/tmp/wol.log` |

### Ejemplo de Alerta en Telegram
```text
🚨 ALERTA WAKE-ON-LAN DETECTADO 🚨

📅 Fecha/Hora: 2026-10-11 17:55:23
🌐 IP Origen: 192.168.1.50:54321
🎯 MAC Objetivo: AA:BB:CC:DD:EE:FF
📡 Puerto Receptor: UDP 9
📦 Tamaño: 102 bytes

Detectado en vivo por la cámara IP
```

### Compilar WoL Sentinel desde código fuente
```bash
env GOOS=linux GOARCH=arm GOARM=5 CGO_ENABLED=0 go build -ldflags="-s -w" -trimpath -o wol_sentinel main.go
```

---

## 📁 Estructura del Repositorio

| Archivo | Tipo | Descripción |
| :--- | :--- | :--- |
| **`fetcher`** | Binario ARM (112 KB) | Cliente HTTPS estático para flash persistente (`/mnt/mtd`). |
| **`fetcher.c`** | Código Fuente C | Implementación minimalista del cliente con BearSSL. |
| **`wol_sentinel`** | Binario ARM (5.1 MB) | Centinela Wake-on-LAN para ejecutar en RAM (`/tmp`). |
| **`main.go`** | Código Fuente Go | Código del centinela con cliente UDP y API Telegram. |

---

## 👥 Autores y Agradecimientos

- **[@robot00f](https://github.com/robot00f)** — Creador y mantenedor.
- **Antigravity ([Google DeepMind](https://deepmind.google/))** — Ingeniería inversa, arquitectura del cliente HTTPS y desarrollo en pareja de WoL Sentinel.

---

## 📄 Licencia

Distribuido bajo la licencia [MIT](LICENSE). Código libre y de uso abierto.
