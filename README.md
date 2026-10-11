# 🛰️ WoL Sentinel (Wake-on-LAN Network Watchdog)

**WoL Sentinel** es un centinela de red ultraligero escrito en Go y compilado para arquitecturas embebidas **ARM Linux (ARMv7l / ARMv5te soft-float)**, diseñado para ejecutarse en cámaras IP (Xiongmai XM530, HiSilicon, Ingenic, etc.) y dispositivos IoT de bajo consumo.

Monitorea la red local las 24 horas del día escuchando paquetes mágicos **Wake-on-LAN (UDP 9 y 7)** y envía alertas en tiempo real a **Telegram** con la dirección IP de origen, la MAC objetivo y la hora exacta del evento.

---

## ⚡ Características

- **Independiente:** Binario estático compilado sin dependencias externas ni `glibc` pesadas.
- **Soporte TLS Nativo:** Envía peticiones HTTPS a la API de Telegram directamente usando la pila criptográfica de Go.
- **Huella de memoria mínima:** Binario optimizado y comprimido con UPX LZMA (~1.4 MB), ideal para dispositivos con 16-64 MB de RAM.
- **Detección precisa:** Inspecciona cabeceras UDP para validar la estructura del Magic Packet (sincronismo `0xFF * 6` + repeticiones de la MAC).
- **100% Seguro:** Sin credenciales grabadas en código. Toda la configuración se suministra en tiempo de ejecución por argumentos o variables de entorno.

---

## 🚀 Uso

### Opciones de línea de comandos

```bash
./wol_sentinel \
  -mac "AA:BB:CC:DD:EE:FF" \
  -token "123456789:ABCDEF_tu_bot_token" \
  -chat "-100123456789" \
  -log "/tmp/wol.log"
```

### Parámetros

| Parámetro | Variable de Entorno | Descripción | Valor por defecto |
| :--- | :--- | :--- | :--- |
| `-mac` | `TARGET_MAC` | MAC de la PC a vigilar. Si se omite, alerta sobre cualquier paquete WoL en la red. | *(Todas las MACs)* |
| `-token` | `BOT_TOKEN` | Token de la API de Telegram Bot. | *(Opcional)* |
| `-chat` | `CHAT_ID` | ID del Chat o Grupo de Telegram destinatario. | *(Opcional)* |
| `-log` | `LOG_PATH` | Ruta del archivo de registro local. | `/mnt/mtd/Log/wol.log` |

---

## 📲 Ejemplo de Notificación en Telegram

```text
🚨 ALERTA WAKE-ON-LAN DETECTADO 🚨

📅 Fecha/Hora: 2026-10-11 04:12:35
🌐 IP Origen: 192.168.1.105:54321
🎯 MAC Objetivo: AA:BB:CC:DD:EE:FF
📡 Puerto Receptor: UDP 9
📦 Tamaño: 102 bytes

Detectado en vivo por la cámara IP
```

---

## 🛠️ Compilación desde el código fuente

Para compilar para dispositivos ARM embebidos (ej. SoC Xiongmai XM530):

```bash
env GOOS=linux GOARCH=arm GOARM=5 go build -ldflags="-s -w" -o wol_sentinel main.go
upx --best --lzma wol_sentinel
```

---

## 📄 Licencia

Distribuido bajo la licencia [MIT](LICENSE). Código libre y de uso abierto.