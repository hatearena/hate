package main

import (
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

const (
	port          = 28780
	staleTimeout  = 180 * time.Second
	cleanInterval = 15 * time.Second
)

type ServerEntry struct {
	ip       string
	lastSeen time.Time
}

type MasterServer struct {
	mu      sync.RWMutex
	servers map[string]*ServerEntry
}

func NewMasterServer() *MasterServer {
	return &MasterServer{
		servers: make(map[string]*ServerEntry),
	}
}

func (ms *MasterServer) handleRegister(w http.ResponseWriter, r *http.Request) {
	host, _, err := net.SplitHostPort(r.RemoteAddr)
	if err != nil {
		host = r.RemoteAddr
	}

	ms.mu.Lock()
	if existing, ok := ms.servers[host]; ok {
		existing.lastSeen = time.Now()
	} else {
		ms.servers[host] = &ServerEntry{ip: host, lastSeen: time.Now()}
		log.Printf("registered server %s", host)
	}
	ms.mu.Unlock()

	w.WriteHeader(http.StatusOK)
}

func (ms *MasterServer) handleRetrieve(w http.ResponseWriter, r *http.Request) {
	ms.mu.RLock()
	servers := make([]string, 0, len(ms.servers))
	for _, s := range ms.servers {
		servers = append(servers, s.ip)
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
			log.Printf("removed stale server %s", s.ip)
			delete(ms.servers, key)
		}
	}
	ms.mu.Unlock()
}

func main() {
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
		Addr:    fmt.Sprintf(":%d", port),
		Handler: mux,
	}

	go func() {
		sigCh := make(chan os.Signal, 1)
		signal.Notify(sigCh, os.Interrupt)
		<-sigCh
		log.Println("shutting down...")
		server.Close()
	}()

	log.Printf("master server listening on port %d", port)
	if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		log.Fatalf("error: %v", err)
	}
}
