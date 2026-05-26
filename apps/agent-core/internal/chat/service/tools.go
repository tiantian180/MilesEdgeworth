package service

import (
	"context"
	"encoding/json"
	"sync"
	"time"

	"milesedgeworth/agent-core/internal/chat"
)

const (
	MaxToolCallsPerRun = 3
	ToolResultTimeout  = 120 * time.Second
	PetMotionToolName  = "pet_motion"
	ToolCallEventType  = "TOOL_CALL"
)

type ToolResult struct {
	RunID      string          `json:"runId"`
	ToolCallID string          `json:"toolCallId"`
	Result     json.RawMessage `json:"result"`
}

type toolWaiter struct {
	toolCallID string
	ch         chan ToolResult
	mu         sync.Mutex
	closed     bool
}

type toolRegistry struct {
	values sync.Map
}

func (r *toolRegistry) register(runID, toolCallID string) *toolWaiter {
	waiter := &toolWaiter{
		toolCallID: toolCallID,
		ch:         make(chan ToolResult, 1),
	}
	r.values.Store(runID, waiter)
	return waiter
}

func (r *toolRegistry) unregister(runID string) {
	if value, ok := r.values.Load(runID); ok {
		waiter := value.(*toolWaiter)
		waiter.close()
	}
	r.values.Delete(runID)
}

func (r *toolRegistry) submit(result ToolResult) bool {
	value, ok := r.values.Load(result.RunID)
	if !ok {
		return false
	}
	waiter := value.(*toolWaiter)
	waiter.mu.Lock()
	defer waiter.mu.Unlock()
	if waiter.closed || waiter.toolCallID != result.ToolCallID {
		return false
	}
	select {
	case waiter.ch <- result:
		return true
	default:
		return false
	}
}

func waitForToolResult(ctx context.Context, waiter *toolWaiter) (ToolResult, bool) {
	timer := time.NewTimer(ToolResultTimeout)
	defer timer.Stop()
	select {
	case <-ctx.Done():
		waiter.close()
		return ToolResult{}, false
	case result := <-waiter.ch:
		return result, true
	case <-timer.C:
		waiter.close()
		return ToolResult{
			Result: json.RawMessage(`{"success":false,"reason":"timeout"}`),
		}, true
	}
}

func (w *toolWaiter) close() {
	w.mu.Lock()
	w.closed = true
	w.mu.Unlock()
}

var PetMotionTool = chat.ToolDefinition{
	Name:        PetMotionToolName,
	Description: "Move the desktop pet. moveTo uses reachable screen percentage coordinates; moveBy moves relative to the current position.",
	Parameters:  json.RawMessage(`{"type":"object","properties":{"action":{"type":"string","enum":["moveTo","moveBy"],"description":"moveTo: move to reachable screen percentage coordinates; moveBy: move relative to the current position"},"x":{"type":"number","description":"moveTo: reachable x percentage; moveBy: relative x percentage"},"y":{"type":"number","description":"moveTo: reachable y percentage; moveBy: relative y percentage"},"mode":{"type":"string","enum":["walk","run"],"description":"Movement speed mode; defaults to walk"}},"required":["action","x","y"]}`),
}

func availableTools() []chat.ToolDefinition {
	return []chat.ToolDefinition{PetMotionTool}
}
