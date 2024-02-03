package main

import (
	"injbg3-gui/components"
	"injbg3-gui/service"
	"injbg3-gui/utils"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
)

var (
	amw        *components.AppMainWindow
	appIcon    *walk.Icon
	hostIcon   *walk.Icon
	remoteIcon *walk.Icon
	toolIcon   *walk.Icon
	aboutIcon  *walk.Icon
)

func init() {
	appIcon, _ = walk.NewIconFromResourceId(3)
	hostIcon, _ = walk.NewIconFromResourceId(4)
	remoteIcon, _ = walk.NewIconFromResourceId(5)
	toolIcon, _ = walk.NewIconFromResourceId(6)
	aboutIcon, _ = walk.NewIconFromResourceId(7)
}

func main() {
	if utils.IsAdmin() {
		seDebug := utils.GrantSeDebugPrivilege()
		service.Set("SeDebugPrivilege", seDebug)
	}
	cfg := &components.MultiPageMainWindowConfig{
		Name:    "mainWindow",
		Icon:    appIcon,
		Font:    Font{PointSize: 11},
		Size:    Size{Width: 550, Height: 220},
		MinSize: Size{Width: 550, Height: 220},
		PageCfgs: []components.PageConfig{
			{
				Title:   "服务器",
				Image:   hostIcon,
				NewPage: components.NewServerPage,
			},
			{
				Title:   "客户端",
				Image:   remoteIcon,
				NewPage: components.NewClientPage,
			},
			{
				Title:   "工具",
				Image:   toolIcon,
				NewPage: components.NewToolsPage,
			},
			{
				Title:   "关于",
				Image:   aboutIcon,
				NewPage: components.NewAboutPage,
			},
		},
	}
	amw, err := components.NewAppMainWindow("injbg3-gui", cfg)
	if err != nil {
		panic(err)
	}
	service.Set("AppMainWindowHandle", amw)
	amw.RefreshTitle()
	amw.Run()
}
