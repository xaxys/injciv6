package service

import "sync"

type Listener[T any] interface {
	OnEvent(event T)
}

type FuncListener[T any] func(event T)

func (fl FuncListener[T]) OnEvent(event T) {
	fl(event)
}

func NewFuncListener[T any](f func(event T)) Listener[T] {
	return FuncListener[T](f)
}

type EventManager[T any] struct {
	listeners  []Listener[T]
	mutex      sync.Mutex
	fastLaunch func() T
}

func (em *EventManager[T]) Register(listener Listener[T]) {
	em.mutex.Lock()
	em.listeners = append(em.listeners, listener)
	em.mutex.Unlock()

	if em.fastLaunch != nil {
		status := em.fastLaunch()
		go listener.OnEvent(status)
	}
}

func (em *EventManager[T]) Unregister(listener Listener[T]) {
	em.mutex.Lock()
	defer em.mutex.Unlock()
	for i, l := range em.listeners {
		if l == listener {
			em.listeners = append(em.listeners[:i], em.listeners[i+1:]...)
			break
		}
	}
}

func (em *EventManager[T]) UnregisterAll() {
	em.mutex.Lock()
	defer em.mutex.Unlock()
	em.listeners = []Listener[T]{}
}

func (em *EventManager[T]) Notify(event T) {
	em.mutex.Lock()
	listenersCopy := make([]Listener[T], len(em.listeners))
	copy(listenersCopy, em.listeners)
	em.mutex.Unlock()
	for _, listener := range listenersCopy {
		go listener.OnEvent(event)
	}
}

func (em *EventManager[T]) FastLaunch(fastLaunch func() T) {
	em.mutex.Lock()
	defer em.mutex.Unlock()
	em.fastLaunch = fastLaunch
}
