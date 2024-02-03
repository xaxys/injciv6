package service

import "sync"

var GlobalMap *sync.Map

func init() {
	GlobalMap = &sync.Map{}
}

func Set(key string, value interface{}) {
	GlobalMap.Store(key, value)
}

func Get[T any](key string) (T, bool) {
	var defT T
	value, ok := GlobalMap.Load(key)
	if !ok {
		return defT, false
	}
	return value.(T), true
}

func GetWithDef[T any](key string, defT T) T {
	value, ok := GlobalMap.Load(key)
	if !ok {
		return defT
	}
	return value.(T)
}
