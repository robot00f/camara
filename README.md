# 🌐 Micro-Fetcher HTTPS (for IP Cameras & Embedded Linux)

> [!NOTE]
> Documentación disponible también en [Español](README_es.md).

**Micro-Fetcher** is an ultra-lightweight (~112 KB) static HTTPS/TLS client written in C and statically linked with [BearSSL](https://bearssl.org/). It was specifically engineered for IoT devices and IP cameras with extremely constrained flash storage (Xiongmai XM530 SoC, HiSilicon, Ingenic, Allwinner, etc.) where:

* The firmware lacks `curl` and `wget` with SSL/TLS support in Busybox.
* Writable configuration flash partitions (`/mnt/mtd` using JFFS2/CramFS) have less than 120 KB of available space.
* The Linux kernel enforces strict memory protection (W^X / non-executable heaps), preventing dynamic decompressors like UPX from running.
* You need to autonomously download binaries, scripts, or updates directly over HTTPS (e.g. from GitHub / TLS 1.2) into volatile RAM (`/tmp`).

---

## ⚡ Features

- **Ultra-Compact:** Only **112 KB** uncompressed (fits comfortably in 64 KB JFFS2 flash eraseblocks).
- **Modern Cryptography:** Implements **TLS 1.2** with **ECDHE-RSA** key exchange and authenticated encryption via **AES-128-GCM** (SHA-256).
- **Zero Dependencies:** Fully static binary compiled against musl libc. No external CA bundles or shared libraries required (`libssl`, `libcrypto`, `glibc`).
- **GitHub Ready:** Fully compatible with Fastly and `raw.githubusercontent.com` TLS 1.2 negotiation.

---

## 🚀 Usage

### Syntax
```bash
./fetcher <host> <path_with_query> <out_file>
```

### Example: Downloading directly from GitHub on an IP Camera
```sh
# Download remote binary directly into RAM (/tmp)
/mnt/mtd/fetcher raw.githubusercontent.com /robot00f/camara/main/wol_sentinel /tmp/wol_sentinel

# Grant execute permissions
chmod +x /tmp/wol_sentinel
```

---

## 🛠️ Building `fetcher` from Source

The client is optimized for compilation with **Zig CC** by statically linking BearSSL and stripping unused functions, data sections, and unwind tables:

```bash
zig cc -target arm-linux-musleabi -Os -s -flto \
  -fno-asynchronous-unwind-tables -fno-unwind-tables \
  -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -I./bearssl/inc fetcher.c libbearssl.a -o fetcher
```

---

## 🛰️ Secondary Module: WoL Sentinel (Wake-on-LAN Watchdog)

As a practical real-world use case, this repository includes **WoL Sentinel**, a network watchdog written in Go designed to be deployed and launched by `fetcher` on the camera.

### What does it do?
It runs silently in the background listening on **UDP ports 9 and 7** for Wake-on-LAN Magic Packets. When a packet targeting a designated PC MAC address is detected, it immediately sends a rich Markdown alert to a **Telegram** bot/group containing the source IP address, port, and exact timestamp.

### Running WoL Sentinel
```sh
/tmp/wol_sentinel \
  -mac "AA:BB:CC:DD:EE:FF" \
  -token "123456789:ABCDEF_your_bot_token" \
  -chat "-100123456789" \
  -log "/tmp/wol.log" > /tmp/sentinel.stdout 2>&1 &
```

### Command-Line Arguments

| Flag | Environment Variable | Description | Default |
| :--- | :--- | :--- | :--- |
| `-mac` | `TARGET_MAC` | MAC address of the PC to monitor. If empty, alerts on all WoL packets. | *(All MACs)* |
| `-token` | `BOT_TOKEN` | Telegram Bot API token. | *(Optional)* |
| `-chat` | `CHAT_ID` | Telegram Chat ID / Group ID. | *(Optional)* |
| `-log` | `LOG_PATH` | Local file path for logging events. | `/tmp/wol.log` |

### Sample Telegram Alert
```text
🚨 WAKE-ON-LAN DETECTED 🚨

📅 Date/Time: 2026-10-11 17:55:23
🌐 Source IP: 192.168.1.50:54321
🎯 Target MAC: AA:BB:CC:DD:EE:FF
📡 Inbound Port: UDP 9
📦 Packet Size: 102 bytes

Captured live by the IP Camera Sentinel
```

### Compiling WoL Sentinel from Source
> [!NOTE]
> On older kernels with strict W^X memory protection (e.g., Linux 3.0.8 / XM530), do **not** pack with UPX. The native static Go ELF runs cleanly without segmentation faults.

```bash
env GOOS=linux GOARCH=arm GOARM=5 CGO_ENABLED=0 go build -ldflags="-s -w" -trimpath -o wol_sentinel main.go
```

---

## 📁 Repository Structure

| File | Type | Description |
| :--- | :--- | :--- |
| **`fetcher`** | ARM Binary (112 KB) | Static HTTPS client for persistent flash storage (`/mnt/mtd`). |
| **`fetcher.c`** | C Source Code | Minimal BearSSL HTTPS fetcher implementation. |
| **`wol_sentinel`** | ARM Binary (5.1 MB) | Wake-on-LAN sentinel daemon to execute from RAM (`/tmp`). |
| **`main.go`** | Go Source Code | WoL sentinel source code with UDP raw listener and Telegram client. |
| **`README.md`** | Markdown | English documentation. |
| **`README_es.md`** | Markdown | Documentación en español. |

---

## 📄 License

Distributed under the [MIT](LICENSE) License. Open-source and free to use.