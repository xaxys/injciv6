package components

import (
	"injciv6-gui/service"
	"injciv6-gui/utils"
)

type AppMainWindow struct {
	AppName string
	*MultiPageMainWindow
}

func NewAppMainWindow(appName string, cfg *MultiPageMainWindowConfig) (*AppMainWindow, error) {
	amw := &AppMainWindow{
		AppName: appName,
	}
	cfg.OnCurrentPageChanged = amw.OnCurrentPageChanged
	cfg.OnCurrentPageToChange = amw.OnCurrentPageToChange

	mpmw, err := NewMultiPageMainWindow(cfg)
	if err != nil {
		return nil, err
	}

	amw.MultiPageMainWindow = mpmw
	return amw, nil
}

func (mw *AppMainWindow) OnCurrentPageToChange() {
	service.Game.Listener().UnregisterAll()
	service.Inject.Listener().UnregisterAll()
	service.IPv6.Listener().UnregisterAll()
	service.IPv6.ErrorListener().UnregisterAll()
}

func (mw *AppMainWindow) OnCurrentPageChanged() {
	mw.RefreshTitle()
}

func (mw *AppMainWindow) RefreshTitle() {
	mw.updateTitle(mw.CurrentPageTitle())
}

func (mw *AppMainWindow) updateTitle(prefix string) {
	title := ""
	if prefix != "" {
		title = prefix + " - "
	}
	title += mw.AppName
	if utils.IsAdmin() {
		title += "（管理员）"
	}
	mw.SetTitle(title)
}
