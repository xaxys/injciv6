package service

import (
	"context"
	"injciv6-gui/utils"
	"sync"
	"time"
)

var IPv6 *IPv6StatusService

func init() {
	IPv6 = NewIPv6StatusService(2000 * time.Millisecond)
	go func() {
		IPv6.UpdateStatus()
		IPv6.StartService()
	}()
}

type IPv6WithStatus struct {
	IPv6   string
	Status utils.IPv6Status
}

type IPv6StatusService struct {
	ipv6          string
	status        utils.IPv6Status
	lastError     error
	interval      time.Duration
	lock          sync.RWMutex
	cancel        context.CancelFunc
	listener      EventManager[IPv6WithStatus]
	errorListener EventManager[error]
}

func NewIPv6StatusService(interval time.Duration) *IPv6StatusService {
	service := &IPv6StatusService{
		ipv6:          "",
		status:        utils.IPv6StatusUnknown,
		interval:      interval,
		errorListener: EventManager[error]{},
	}
	service.Listener().FastLaunch(service.IPv6WithStatus)
	return service
}

func (g *IPv6StatusService) IPv6() string {
	g.lock.RLock()
	defer g.lock.RUnlock()
	return g.ipv6
}

func (g *IPv6StatusService) Status() utils.IPv6Status {
	g.lock.RLock()
	defer g.lock.RUnlock()
	return g.status
}

func (g *IPv6StatusService) IPv6AndStatus() (string, utils.IPv6Status) {
	g.lock.RLock()
	defer g.lock.RUnlock()
	return g.ipv6, g.status
}

func (g *IPv6StatusService) IPv6WithStatus() IPv6WithStatus {
	g.lock.RLock()
	defer g.lock.RUnlock()
	return IPv6WithStatus{
		IPv6:   g.ipv6,
		Status: g.status,
	}
}

func (g *IPv6StatusService) UpdateStatus() {
	g.lock.RLock()
	ipv6, status := g.ipv6, g.status
	g.lock.RUnlock()

	new_ipv6, new_status, err := utils.GetMyIPv6()
	g.lock.Lock()
	g.ipv6, g.status, g.lastError = new_ipv6, new_status, err
	g.lock.Unlock()

	if ipv6 != g.ipv6 || status != g.status {
		ipv6WithStatus := IPv6WithStatus{
			IPv6:   g.ipv6,
			Status: g.status,
		}
		g.listener.Notify(ipv6WithStatus)
	}
	if g.lastError != nil {
		g.errorListener.Notify(g.lastError)
	}
}

func (g *IPv6StatusService) StartService() {
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

func (g *IPv6StatusService) StopService() {
	g.lock.Lock()
	defer g.lock.Unlock()
	if g.cancel == nil {
		return
	}

	g.cancel()
	g.cancel = nil
}

func (g *IPv6StatusService) Listener() *EventManager[IPv6WithStatus] {
	return &g.listener
}

func (g *IPv6StatusService) ErrorListener() *EventManager[error] {
	return &g.errorListener
}
