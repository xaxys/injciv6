// Copyright 2017 The Walk Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package components

import (
	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
)

type MultiPageMainWindowConfig struct {
	Name                  string
	Icon                  Property
	Enabled               Property
	Visible               Property
	Font                  Font
	MinSize               Size
	MaxSize               Size
	ContextMenuItems      []MenuItem
	OnKeyDown             walk.KeyEventHandler
	OnKeyPress            walk.KeyEventHandler
	OnKeyUp               walk.KeyEventHandler
	OnMouseDown           walk.MouseEventHandler
	OnMouseMove           walk.MouseEventHandler
	OnMouseUp             walk.MouseEventHandler
	OnSizeChanged         walk.EventHandler
	OnCurrentPageChanged  walk.EventHandler
	OnCurrentPageToChange walk.EventHandler
	Title                 string
	Size                  Size
	MenuItems             []MenuItem
	ToolBar               ToolBar
	PageCfgs              []PageConfig
}

type PageConfig struct {
	Title   string
	Image   walk.Image
	NewPage PageFactoryFunc
}

type PageFactoryFunc func(parent walk.Container) (Page, error)

type Page interface {
	// Provided by Walk
	walk.Container
	Parent() walk.Container
	SetParent(parent walk.Container) error
}

type MultiPageMainWindow struct {
	*walk.MainWindow
	navTB                        *walk.ToolBar
	pageCom                      *walk.Composite
	action2NewPage               map[*walk.Action]PageFactoryFunc
	pageActions                  []*walk.Action
	currentAction                *walk.Action
	currentPage                  Page
	currentPageChangedPublisher  walk.EventPublisher
	currentPageToChangePublisher walk.EventPublisher
}

func NewMultiPageMainWindow(cfg *MultiPageMainWindowConfig) (*MultiPageMainWindow, error) {
	mpmw := &MultiPageMainWindow{
		action2NewPage: make(map[*walk.Action]PageFactoryFunc),
	}

	if err := (MainWindow{
		AssignTo:         &mpmw.MainWindow,
		Name:             cfg.Name,
		Icon:             cfg.Icon,
		Title:            cfg.Title,
		Enabled:          cfg.Enabled,
		Visible:          cfg.Visible,
		Font:             cfg.Font,
		MinSize:          cfg.MinSize,
		MaxSize:          cfg.MaxSize,
		MenuItems:        cfg.MenuItems,
		ToolBar:          cfg.ToolBar,
		ContextMenuItems: cfg.ContextMenuItems,
		OnKeyDown:        cfg.OnKeyDown,
		OnKeyPress:       cfg.OnKeyPress,
		OnKeyUp:          cfg.OnKeyUp,
		OnMouseDown:      cfg.OnMouseDown,
		OnMouseMove:      cfg.OnMouseMove,
		OnMouseUp:        cfg.OnMouseUp,
		OnSizeChanged:    cfg.OnSizeChanged,
		Layout:           HBox{MarginsZero: true, SpacingZero: true},
		Children: []Widget{
			ScrollView{
				HorizontalFixed: true,
				Layout:          VBox{MarginsZero: true},
				Children: []Widget{
					Composite{
						Layout: VBox{MarginsZero: true},
						Children: []Widget{
							ToolBar{
								AssignTo:    &mpmw.navTB,
								Orientation: Vertical,
								ButtonStyle: ToolBarButtonImageAboveText,
								MaxTextRows: 1,
							},
						},
					},
				},
			},
			Composite{
				AssignTo: &mpmw.pageCom,
				Name:     "pageCom",
				Layout:   HBox{MarginsZero: true, SpacingZero: true},
			},
		},
	}).Create(); err != nil {
		return nil, err
	}

	succeeded := false
	defer func() {
		if !succeeded {
			mpmw.Dispose()
		}
	}()

	mpmw.SetSize(walk.Size{Width: 0, Height: 0})

	for _, pc := range cfg.PageCfgs {
		action, err := mpmw.newPageAction(pc.Title, pc.Image, pc.NewPage)
		if err != nil {
			return nil, err
		}

		mpmw.pageActions = append(mpmw.pageActions, action)
	}

	if err := mpmw.updateNavigationToolBar(); err != nil {
		return nil, err
	}

	if len(mpmw.pageActions) > 0 {
		if err := mpmw.setCurrentAction(mpmw.pageActions[0]); err != nil {
			return nil, err
		}
	}

	if cfg.OnCurrentPageChanged != nil {
		mpmw.CurrentPageChanged().Attach(cfg.OnCurrentPageChanged)
	}

	if cfg.OnCurrentPageToChange != nil {
		mpmw.CurrentPageToChange().Attach(cfg.OnCurrentPageToChange)
	}

	succeeded = true

	return mpmw, nil
}

func (mpmw *MultiPageMainWindow) CurrentPage() Page {
	return mpmw.currentPage
}

func (mpmw *MultiPageMainWindow) CurrentPageTitle() string {
	if mpmw.currentAction == nil {
		return ""
	}

	return mpmw.currentAction.Text()
}

func (mpmw *MultiPageMainWindow) CurrentPageChanged() *walk.Event {
	return mpmw.currentPageChangedPublisher.Event()
}

func (mpmw *MultiPageMainWindow) CurrentPageToChange() *walk.Event {
	return mpmw.currentPageToChangePublisher.Event()
}

func (mpmw *MultiPageMainWindow) newPageAction(title string, image walk.Image, newPage PageFactoryFunc) (*walk.Action, error) {
	action := walk.NewAction()
	action.SetCheckable(true)
	action.SetExclusive(true)
	action.SetImage(image)
	action.SetText(title)

	mpmw.action2NewPage[action] = newPage

	action.Triggered().Attach(func() {
		mpmw.setCurrentAction(action)
	})

	return action, nil
}

func (mpmw *MultiPageMainWindow) setCurrentAction(action *walk.Action) error {
	defer func() {
		if !mpmw.pageCom.IsDisposed() {
			mpmw.pageCom.RestoreState()
		}
	}()

	mpmw.currentPageToChangePublisher.Publish()

	mpmw.SetFocus()

	if prevPage := mpmw.currentPage; prevPage != nil {
		mpmw.pageCom.SaveState()
		prevPage.SetVisible(false)
		prevPage.(walk.Widget).SetParent(nil)
		prevPage.Dispose()
	}

	newPage := mpmw.action2NewPage[action]

	page, err := newPage(mpmw.pageCom)
	if err != nil {
		return err
	}

	action.SetChecked(true)

	mpmw.currentPage = page
	mpmw.currentAction = action

	mpmw.currentPageChangedPublisher.Publish()

	return nil
}

func (mpmw *MultiPageMainWindow) updateNavigationToolBar() error {
	mpmw.navTB.SetSuspended(true)
	defer mpmw.navTB.SetSuspended(false)

	actions := mpmw.navTB.Actions()

	if err := actions.Clear(); err != nil {
		return err
	}

	for _, action := range mpmw.pageActions {
		if err := actions.Add(action); err != nil {
			return err
		}
	}

	if mpmw.currentAction != nil {
		if !actions.Contains(mpmw.currentAction) {
			for _, action := range mpmw.pageActions {
				if action != mpmw.currentAction {
					if err := mpmw.setCurrentAction(action); err != nil {
						return err
					}

					break
				}
			}
		}
	}

	return nil
}
