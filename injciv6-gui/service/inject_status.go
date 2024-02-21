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
	go func() {
		Inject.UpdateStatus()
		Inject.StartService()
	}()
}

type InjectStatusService struct {
	status   utils.InjectStatus
	interval time.Duration
	lock     sync.RWMutex
	cancel   context.CancelFunc
	listener EventManager[utils.InjectStatus]
}

func NewInjectStatusService(interval time.Duration) *InjectStatusService {
	service := &InjectStatusService{
		status:   utils.InjectStatusUnknown,
		interval: interval,
	}
	service.Listener().FastLaunch(service.IsInjected)
	return service
}

func (g *InjectStatusService) IsInjected() utils.InjectStatus {
	g.lock.RLock()
	defer g.lock.RUnlock()
	return g.status
}

func (g *InjectStatusService) UpdateStatus() {
	status := g.IsInjected()
	new_status := utils.IsCiv6Injected()

	g.lock.Lock()
	g.status = new_status
	g.lock.Unlock()

	if status != g.status {
		g.listener.Notify(g.status)
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

func (g *InjectStatusService) Listener() *EventManager[utils.InjectStatus] {
	return &g.listener
}
