package main

import (
	"flag"
	"fmt"
	"log"
	"net"
	"net/http"
	"os"
	"os/signal"
	"strings"
	"sync"
	"time"
)

var devMode bool

const (
	port               = 28780
	servinfoPort       = 28766
	staleTimeout       = 180 * time.Second
	cleanInterval      = 15 * time.Second
	registerRateLimit  = 10 * time.Second
	heartbeatRateLimit = 5 * time.Second
	verifyTimeout      = 3 * time.Second
	protocolVersion    = 122
	maxServerNameLen   = 64
	maxMapNameLen      = 64
	maxDescLen         = 256
	maxServersPerIP    = 5
	maxTotalServers    = 10000
)

type ServerEntry struct {
	ip         string
	lastSeen   time.Time
	verified   bool
	verifyFail int
}

type rateLimitEntry struct {
	lastReg   time.Time
	lastHeart time.Time
	count     int
}

type MasterServer struct {
	mu        sync.RWMutex
	servers   map[string]*ServerEntry
	rateLimit map[string]*rateLimitEntry
}

func NewMasterServer() *MasterServer {
	return &MasterServer{
		servers:   make(map[string]*ServerEntry),
		rateLimit: make(map[string]*rateLimitEntry),
	}
}

func sanitize(s string, maxLen int) string {
	var b strings.Builder
	b.Grow(len(s))
	for _, r := range s {
		if r >= 32 && r <= 126 {
			b.WriteRune(r)
		}
	}
	cleaned := b.String()
	if len(cleaned) > maxLen {
		cleaned = cleaned[:maxLen]
	}
	return cleaned
}

func isPrivateIP(ip string) bool {
	parsed := net.ParseIP(ip)
	if parsed == nil {
		return true
	}
	if parsed.IsLoopback() ||
		parsed.IsLinkLocalUnicast() ||
		parsed.IsLinkLocalMulticast() ||
		parsed.IsMulticast() ||
		parsed.IsUnspecified() {
		return true
	}
	if parsed4 := parsed.To4(); parsed4 != nil {
		switch {
		case parsed4[0] == 10:
			return true
		case parsed4[0] == 172 && parsed4[1] >= 16 && parsed4[1] <= 31:
			return true
		case parsed4[0] == 192 && parsed4[1] == 168:
			return true
		case parsed4[0] == 100 && parsed4[1] >= 64 && parsed4[1] <= 127:
			return true
		case parsed4[0] == 169 && parsed4[1] == 254:
			return true
		case parsed4[0] == 198 && parsed4[1] >= 18 && parsed4[1] <= 19:
			return true
		}
	}
	return false
}

func (ms *MasterServer) verifyServer(ip string) bool {
	addr := net.UDPAddr{
		IP:   net.ParseIP(ip),
		Port: servinfoPort,
	}
	conn, err := net.DialUDP("udp", nil, &addr)
	if err != nil {
		return false
	}
	defer conn.Close()

	conn.SetDeadline(time.Now().Add(verifyTimeout))

	if _, err := conn.Write([]byte{0}); err != nil {
		return false
	}

	buf := make([]byte, 1500)
	n, err := conn.Read(buf)
	if err != nil {
		return false
	}

	if n < 1 {
		return false
	}

	return buf[0] == protocolVersion
}

func (ms *MasterServer) handleRegister(w http.ResponseWriter, r *http.Request) {
	host, _, err := net.SplitHostPort(r.RemoteAddr)
	if err != nil {
		http.Error(w, "bad request", http.StatusBadRequest)
		return
	}

	parsed := net.ParseIP(host)
	if parsed == nil || (!devMode && isPrivateIP(host)) {
		log.Printf("rejected registration from invalid/private IP: %s", host)
		http.Error(w, "invalid IP", http.StatusBadRequest)
		return
	}

	action := sanitize(r.URL.Query().Get("action"), 16)
	mapName := sanitize(r.URL.Query().Get("map"), maxMapNameLen)
	serverName := sanitize(r.URL.Query().Get("name"), maxServerNameLen)
	desc := sanitize(r.URL.Query().Get("desc"), maxDescLen)
	if desc == "" {
		desc = sanitize(r.UserAgent(), maxDescLen)
	}
	if serverName == "" {
		serverName = sanitize(r.Referer(), maxServerNameLen)
	}

	ms.mu.Lock()

	rl, hasRL := ms.rateLimit[host]
	if !hasRL {
		rl = &rateLimitEntry{}
		ms.rateLimit[host] = rl
	}
	now := time.Now()

	if action == "add" || action == "" {
		if hasRL && now.Sub(rl.lastReg) < registerRateLimit {
			ms.mu.Unlock()
			http.Error(w, "rate limited", http.StatusTooManyRequests)
			return
		}
		rl.lastReg = now
		rl.count++

		if len(ms.servers) >= maxTotalServers {
			ms.mu.Unlock()
			log.Printf("rejected registration from %s: server list full", host)
			http.Error(w, "server full", http.StatusServiceUnavailable)
			return
		}

		serverCount := 0
		for _, s := range ms.servers {
			if s.ip == host {
				serverCount++
			}
		}
		if serverCount >= maxServersPerIP {
			ms.mu.Unlock()
			log.Printf("rejected registration from %s: too many servers per IP", host)
			http.Error(w, "too many servers", http.StatusTooManyRequests)
			return
		}
	} else {
		if hasRL && now.Sub(rl.lastHeart) < heartbeatRateLimit {
			ms.mu.Unlock()
			http.Error(w, "rate limited", http.StatusTooManyRequests)
			return
		}
		rl.lastHeart = now
	}

	existing, exists := ms.servers[host]
	if exists {
		existing.lastSeen = now
		if !existing.verified && existing.verifyFail < 3 {
			go func(ip string) {
				if ms.verifyServer(ip) {
					ms.mu.Lock()
					if e, ok := ms.servers[ip]; ok {
						e.verified = true
						log.Printf("verified server %s (re-check)", ip)
					}
					ms.mu.Unlock()
				} else {
					ms.mu.Lock()
					if e, ok := ms.servers[ip]; ok {
						e.verifyFail++
					}
					ms.mu.Unlock()
				}
			}(host)
		}
		ms.mu.Unlock()
		w.WriteHeader(http.StatusOK)
		return
	}

	entry := &ServerEntry{
		ip:       host,
		lastSeen: now,
	}

	go func(ip string, e *ServerEntry) {
		if ms.verifyServer(ip) {
			ms.mu.Lock()
			if existing, ok := ms.servers[ip]; ok && existing == e {
				e.verified = true
				log.Printf("verified server %s", ip)
			}
			ms.mu.Unlock()
		} else {
			ms.mu.Lock()
			if existing, ok := ms.servers[ip]; ok && existing == e {
				e.verifyFail++
			}
			ms.mu.Unlock()
		}
	}(host, entry)

	ms.servers[host] = entry
	log.Printf("registered server %s (map=%s, name=%s)", host, mapName, serverName)
	ms.mu.Unlock()

	if mapName != "" || serverName != "" || desc != "" {
		log.Printf("registered server metadata: map=%q name=%q desc=%q", mapName, serverName, desc)
	}
	w.WriteHeader(http.StatusOK)
}

func (ms *MasterServer) handleRetrieve(w http.ResponseWriter, r *http.Request) {
	ms.mu.RLock()
	servers := make([]string, 0, len(ms.servers))
	for _, s := range ms.servers {
		if s.verified {
			servers = append(servers, s.ip)
		}
	}
	ms.mu.RUnlock()

	var sb strings.Builder
	for _, ip := range servers {
		fmt.Fprintf(&sb, "addserver %s\n", ip)
	}

	w.Header().Set("Content-Type", "text/plain")
	w.WriteHeader(http.StatusOK)
	w.Write([]byte(sb.String()))
}

func (ms *MasterServer) cleanupStale() {
	now := time.Now()
	ms.mu.Lock()
	for key, s := range ms.servers {
		if now.Sub(s.lastSeen) > staleTimeout {
			log.Printf("removed stale/unverified server %s (verified=%v, fails=%d)",
				s.ip, s.verified, s.verifyFail)
			delete(ms.servers, key)
		}
	}
	for key, rl := range ms.rateLimit {
		if now.Sub(rl.lastReg) > staleTimeout && now.Sub(rl.lastHeart) > staleTimeout {
			delete(ms.rateLimit, key)
		}
	}
	ms.mu.Unlock()
}

func main() {
	flag.BoolVar(&devMode, "dev", false, "allow private/test IPs for development")
	flag.Parse()

	ms := NewMasterServer()

	go func() {
		ticker := time.NewTicker(cleanInterval)
		defer ticker.Stop()
		for range ticker.C {
			ms.cleanupStale()
		}
	}()

	mux := http.NewServeMux()
	mux.HandleFunc("/register.do", ms.handleRegister)
	mux.HandleFunc("/retrieve.do", ms.handleRetrieve)

	server := &http.Server{
		Addr:         fmt.Sprintf(":%d", port),
		Handler:      mux,
		ReadTimeout:  10 * time.Second,
		WriteTimeout: 10 * time.Second,
		IdleTimeout:  30 * time.Second,
	}

	go func() {
		sigCh := make(chan os.Signal, 1)
		signal.Notify(sigCh, os.Interrupt)
		<-sigCh
		log.Println("shutting down...")
		server.Close()
	}()

	if devMode {
		log.Printf("running in dev mode (private IPs allowed)")
	}
	log.Printf("master server listening on port %d", port)
	if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		log.Fatalf("error: %v", err)
	}
}
