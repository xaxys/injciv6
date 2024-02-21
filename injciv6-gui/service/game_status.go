package service

import (
	"context"
	"injciv6-gui/utils"
	"sync"
	"time"
)

var Game *GameStatusService

func init() {
	Game = NewGameStatusService(500 * time.Millisecond)
	go func() {
		Game.UpdateStatus()
		Game.StartService()
	}()
}

type GameStatusService struct {
	status   utils.Civ6Status
	interval time.Duration
	lock     sync.RWMutex
	cancel   context.CancelFunc
	listener EventManager[utils.Civ6Status]
}

func NewGameStatusService(interval time.Duration) *GameStatusService {
	service := &GameStatusService{
		status:   utils.Civ6StatusUnknown,
		interval: interval,
	}
	service.Listener().FastLaunch(service.Status)
	return service
}

func (g *GameStatusService) Status() utils.Civ6Status {
	g.lock.RLock()
	defer g.lock.RUnlock()
	return g.status
}

func (g *GameStatusService) UpdateStatus() {
	status := g.Status()
	new_status := utils.IsCiv6Running()
	g.lock.Lock()
	g.status = new_status
	g.lock.Unlock()

	if status != g.status {
		g.listener.Notify(g.status)
	}
}

func (g *GameStatusService) StartService() {
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

func (g *GameStatusService) StopService() {
	g.lock.Lock()
	defer g.lock.Unlock()

	if g.cancel != nil {
		g.cancel()
		g.cancel = nil
	}
}

func (g *GameStatusService) Listener() *EventManager[utils.Civ6Status] {
	return &g.listener
}
