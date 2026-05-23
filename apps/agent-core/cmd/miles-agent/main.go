package main

import (
	"context"
	"flag"
	"net/http"
	"os"
	"os/signal"
	"path/filepath"
	"strings"
	"syscall"
	"time"

	"milesedgeworth/agent-core/internal/api"
	"milesedgeworth/agent-core/internal/chat"
	"milesedgeworth/agent-core/internal/chat/config"
	"milesedgeworth/agent-core/internal/chat/observability"
	"milesedgeworth/agent-core/internal/chat/openai"
	chatservice "milesedgeworth/agent-core/internal/chat/service"
	"milesedgeworth/agent-core/internal/mileslog"
	"milesedgeworth/agent-core/internal/models"
	"milesedgeworth/agent-core/internal/store"
)

var logger = mileslog.New("MILES.SIDECAR")

func main() {
	addr := flag.String("addr", api.DefaultListenAddr, "listen address")
	parentPID := flag.Int("parent-pid", 0, "parent desktop process id")
	flag.Parse()

	cfg := config.FromEnv()

	var provider chat.Provider
	label := "unconfigured"
	if cfg.Enabled() {
		provider = openai.NewProvider(cfg.BaseURL, cfg.APIKey, cfg.Model, cfg.Temperature, cfg.MaxTokens)
		label = "openai-compatible"
		logger.Info("provider selected", "provider", "openai-compatible", "model", cfg.Model)
	} else {
		logger.Info("provider selected", "provider", "unconfigured")
	}

	if provider != nil && cfg.Langfuse.Enabled() {
		tracerProvider, err := observability.NewLangfuseTracerProvider(context.Background(), observability.LangfuseOTLPConfig{
			Host:      cfg.Langfuse.Host,
			PublicKey: cfg.Langfuse.PublicKey,
			SecretKey: cfg.Langfuse.SecretKey,
		})
		if err != nil {
			logger.Warn("langfuse tracing disabled", "error", err)
		} else {
			defer func() {
				ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
				defer cancel()
				if err := tracerProvider.Shutdown(ctx); err != nil {
					logger.Warn("langfuse shutdown failed", "error", err)
				}
			}()
			provider = observability.WrapProvider(provider, observability.Options{
				TracerProvider: tracerProvider,
				Model:          cfg.Model,
				Temperature:    cfg.Temperature,
				MaxTokens:      cfg.MaxTokens,
				CaptureContent: cfg.Langfuse.CaptureContent,
			})
			logger.Info("langfuse tracing enabled", "hostSet", true, "captureContent", cfg.Langfuse.CaptureContent)
		}
	}

	dataDir := os.Getenv("MILES_DATA_DIR")
	if strings.TrimSpace(dataDir) == "" {
		dataDir = filepath.Join(os.TempDir(), "MilesEdgeworth")
	}
	st, err := store.Open(dataDir)
	if err != nil {
		logger.Error("store open failed", "error", err)
		os.Exit(1)
	}
	defer st.Close()

	catalog := models.NewCatalog(dataDir)
	if err := catalog.Refresh(context.Background()); err != nil {
		logger.Warn("models catalog refresh failed", "error", err)
	}
	chatService := chatservice.New(st, provider, catalog, cfg.Model)

	server := &http.Server{
		Addr:    *addr,
		Handler: api.NewServer(st, chatService, label).Routes(),
	}

	errs := make(chan error, 1)
	go func() {
		logger.Info("server listening", "addr", *addr)
		errs <- server.ListenAndServe()
	}()

	signals := make(chan os.Signal, 1)
	signal.Notify(signals, os.Interrupt, syscall.SIGTERM)

	parentGone := watchParent(context.Background(), *parentPID, 2*time.Second, processAlive)

	shutdown := func(reason string) {
		logger.Info("shutdown requested", "reason", reason)
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		if err := server.Shutdown(ctx); err != nil {
			logger.Error("shutdown failed", "error", err)
			os.Exit(1)
		}
	}

	select {
	case sig := <-signals:
		shutdown(sig.String())
	case <-parentGone:
		shutdown("parent process disappeared")
	case err := <-errs:
		if err != nil && err != http.ErrServerClosed {
			logger.Error("server failed", "error", err)
			os.Exit(1)
		}
	}
}

type parentAliveFunc func(pid int) bool

func watchParent(ctx context.Context, parentPID int, interval time.Duration, alive parentAliveFunc) <-chan struct{} {
	gone := make(chan struct{})
	if parentPID <= 0 {
		return gone
	}
	if interval <= 0 {
		interval = time.Second
	}

	go func() {
		defer close(gone)
		ticker := time.NewTicker(interval)
		defer ticker.Stop()

		for {
			select {
			case <-ctx.Done():
				return
			case <-ticker.C:
				// 开发期主进程可能被调试器强杀，Qt 析构来不及清理 sidecar；
				// 这里让 sidecar 自己发现父进程消失并退出，避免孤儿进程长期占端口。
				if !alive(parentPID) {
					logger.Info("parent process disappeared", "parentPid", parentPID)
					return
				}
			}
		}
	}()

	return gone
}

func processAlive(pid int) bool {
	if pid <= 0 {
		return false
	}
	err := syscall.Kill(pid, 0)
	return err == nil || err == syscall.EPERM
}
