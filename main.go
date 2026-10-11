package main

import (
	"bytes"
	"crypto/tls"
	"flag"
	"fmt"
	"net"
	"net/http"
	"net/url"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"
)

var (
	TargetMAC string
	BotToken  string
	ChatID    string
	LogPath   string
)

var httpClient = &http.Client{
	Timeout: 10 * time.Second,
	Transport: &http.Transport{
		TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
	},
}

func sendTelegram(text string) error {
	if BotToken == "" || ChatID == "" {
		return nil
	}
	apiURL := fmt.Sprintf("https://api.telegram.org/bot%s/sendMessage", BotToken)
	params := url.Values{}
	params.Set("chat_id", ChatID)
	params.Set("text", text)
	params.Set("parse_mode", "Markdown")

	resp, err := httpClient.PostForm(apiURL, params)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	return nil
}

func logMessage(msg string) {
	fmt.Println(msg)
	if LogPath != "" {
		f, err := os.OpenFile(LogPath, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0644)
		if err == nil {
			f.WriteString(msg + "\n")
			f.Sync()
			f.Close()
		}
	}
}

func parseWoL(data []byte) (bool, string) {
	// Magic Packet: 6 bytes 0xFF followed by 16 repetitions of 6-byte MAC
	if len(data) < 102 {
		return false, ""
	}

	idx := bytes.Index(data, []byte{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
	if idx < 0 || idx+6+96 > len(data) {
		return false, ""
	}

	macBytes := data[idx+6 : idx+12]
	for i := 1; i < 16; i++ {
		start := idx + 6 + (i * 6)
		if !bytes.Equal(data[start:start+6], macBytes) {
			return false, ""
		}
	}

	macStr := fmt.Sprintf("%02X:%02X:%02X:%02X:%02X:%02X",
		macBytes[0], macBytes[1], macBytes[2], macBytes[3], macBytes[4], macBytes[5])
	return true, macStr
}

func listenPort(port int) {
	addr := net.UDPAddr{
		Port: port,
		IP:   net.IPv4zero,
	}
	conn, err := net.ListenUDP("udp", &addr)
	if err != nil {
		logMessage(fmt.Sprintf("[%s] Error escuchando en UDP :%d: %v", time.Now().Format("2006-01-02 15:04:05"), port, err))
		return
	}
	defer conn.Close()

	logMessage(fmt.Sprintf("[%s] Centinela escuchando en UDP :%d", time.Now().Format("2006-01-02 15:04:05"), port))

	buf := make([]byte, 2048)
	for {
		n, remoteAddr, err := conn.ReadFrom(buf)
		if err != nil {
			continue
		}

		now := time.Now().Format("2006-01-02 15:04:05")
		isWoL, mac := parseWoL(buf[:n])

		logMessage(fmt.Sprintf("[%s] [PAQUETE UDP :%d] %d bytes desde %s (isWoL=%v, MAC=%s)",
			now, port, n, remoteAddr.String(), isWoL, mac))

		if isWoL {
			// If target MAC is specified, check filter (case insensitive)
			if TargetMAC != "" && !strings.EqualFold(TargetMAC, mac) {
				logMessage(fmt.Sprintf("[%s] WOL para otra MAC ignorada: %s (buscando %s)", now, mac, TargetMAC))
				continue
			}

			logLine := fmt.Sprintf("[%s] 🚨 WOL PACKET DETECTADO: Origen=%s -> MAC=%s (puerto %d)",
				now, remoteAddr.String(), mac, port)
			logMessage(logLine)

			alertMsg := fmt.Sprintf(
				"🚨 *ALERTA WAKE-ON-LAN DETECTADO* 🚨\n\n"+
					"📅 *Fecha/Hora:* %s\n"+
					"🌐 *IP Origen:* %s\n"+
					"🎯 *MAC Objetivo:* %s\n"+
					"📡 *Puerto Receptor:* UDP %d\n"+
					"📦 *Tamaño:* %d bytes\n\n"+
					"_Detectado en vivo por la cámara IP_",
				now, remoteAddr.String(), mac, port, n,
			)

			go func(m string) {
				if err := sendTelegram(m); err != nil {
					logMessage(fmt.Sprintf("[ERROR TELEGRAM] %v", err))
				} else {
					logMessage("[TELEGRAM] Alerta enviada con exito")
				}
			}(alertMsg)
		}
	}
}

func main() {
	flag.StringVar(&TargetMAC, "mac", os.Getenv("TARGET_MAC"), "Target PC MAC address to monitor (e.g. AA:BB:CC:DD:EE:FF). Empty = all.")
	flag.StringVar(&BotToken, "token", os.Getenv("BOT_TOKEN"), "Telegram Bot Token")
	flag.StringVar(&ChatID, "chat", os.Getenv("CHAT_ID"), "Telegram Chat ID")
	flag.StringVar(&LogPath, "log", "/mnt/mtd/Log/wol.log", "Log file destination")
	flag.Parse()

	now := time.Now().Format("2006-01-02 15:04:05")
	startupMsg := fmt.Sprintf("[%s] === CENTINELA WOL INICIADO ===", now)
	logMessage(startupMsg)

	if BotToken != "" && ChatID != "" {
		targetDesc := TargetMAC
		if targetDesc == "" {
			targetDesc = "Todas las MACs"
		}
		tgStartup := fmt.Sprintf(
			"🟢 *Centinela WoL ACTIVO*\n\n"+
				"📹 *Dispositivo:* Cámara IP\n"+
				"🎯 *Monitoreando MAC:* %s\n"+
				"📡 *Puertos:* UDP 9 y UDP 7\n"+
				"📝 *Log:* %s\n"+
				"⏰ *Hora inicio:* %s\n\n"+
				"_El centinela está vigilando la red local para registrar cualquier paquete que despierte tu PC._",
			targetDesc, LogPath, now,
		)

		go func() {
			if err := sendTelegram(tgStartup); err != nil {
				logMessage(fmt.Sprintf("[ERROR TELEGRAM STARTUP] %v", err))
			} else {
				logMessage("[TELEGRAM] Notificacion de inicio enviada")
			}
		}()
	}

	go listenPort(9)
	go listenPort(7)

	sig := make(chan os.Signal, 1)
	signal.Notify(sig, syscall.SIGINT, syscall.SIGTERM)
	<-sig

	logMessage(fmt.Sprintf("[%s] Centinela detenido", time.Now().Format("2006-01-02 15:04:05")))
}
