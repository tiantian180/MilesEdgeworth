package main

import (
	"context"
	"testing"
	"time"
)

func TestWatchParentClosesWhenParentDisappears(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	checks := 0
	gone := watchParent(ctx, 12345, time.Millisecond, func(pid int) bool {
		if pid != 12345 {
			t.Fatalf("pid = %d, want 12345", pid)
		}
		checks++
		return checks < 2
	})

	select {
	case <-gone:
	case <-time.After(250 * time.Millisecond):
		t.Fatal("watchParent did not close after parent disappeared")
	}
}

func TestWatchParentDisabledWithoutParentPID(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	gone := watchParent(ctx, 0, time.Millisecond, func(int) bool {
		t.Fatal("parent liveness should not be checked when parent pid is disabled")
		return false
	})

	select {
	case <-gone:
		t.Fatal("watchParent should stay open when parent pid is disabled")
	case <-time.After(10 * time.Millisecond):
	}
}
