package service

import (
	"context"
	"injciv6-gui/utils"
	"sync"
	"time"
)

var Inject *InjectStatusService

func init() {
	Inject = NewInjectStatusService(500 * time.Millisecond)
	Inject.UpdateStatus()
	Inject.StartService()
}

type InjectStatusService struct {
	injected bool
	interval time.Duration
	lock     sync.RWMutex
	cancel   context.CancelFunc
	listener EventManager[bool]
}

func NewInjectStatusService(interval time.Duration) *InjectStatusService {
	service := &InjectStatusService{
		injected: false,
		interval: interval,
	}
	service.Listener().FastLaunch(service.IsInjected)
	return service
}

func (g *InjectStatusService) IsInjected() bool {
	g.lock.RLock()
	defer g.lock.RUnlock()
	return g.injected
}

func (g *InjectStatusService) UpdateStatus() {
	injected := g.IsInjected()
	new_injected := utils.IsCiv6Injected()

	g.lock.Lock()
	g.injected = new_injected
	g.lock.Unlock()

	if injected != g.injected {
		g.listener.Notify(g.injected)
	}
}

func (g *InjectStatusService) StartService() {
	g.lock.Lock()
	defer g.lock.Unlock()
	if g.cancel != nil {
		return
	}

	ticker := time.NewTicker(g.interval)
	ctx, cancel := context.WithCancel(context.Background())
	g.cancel = cancel
	go func() {
		for {
			select {
			case <-ticker.C:
				g.UpdateStatus()
			case <-ctx.Done():
				return
			}
		}
	}()
}

func (g *InjectStatusService) StopService() {
	g.lock.Lock()
	defer g.lock.Unlock()
	if g.cancel == nil {
		return
	}
	g.cancel()
	g.cancel = nil
}

func (g *InjectStatusService) Listener() *EventManager[bool] {
	return &g.listener
}
