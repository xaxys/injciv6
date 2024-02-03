package components

import (
	"fmt"
	"os/exec"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
)

var (
	BuildTags = "unknown"
	BuildTime = "unknown"
	GitCommit = "unknown"
	GoVersion = "unknown"
)

type AboutPage struct {
	*walk.Composite
}

func NewAboutPage(parent walk.Container) (Page, error) {
	p := &AboutPage{}

	if err := (Composite{
		AssignTo: &p.Composite,
		Name:     "关于",
		Layout:   VBox{},
		Children: []Widget{
			Composite{
				Layout: HBox{},
				Children: []Widget{
					Label{
						Font:          Font{PointSize: 10},
						TextAlignment: AlignNear,
						Text:          "injbg3-gui",
					},
					HSpacer{},
					Label{
						Font:          Font{PointSize: 10},
						TextAlignment: AlignNear,
						Text:          "作者: xaxys",
					},
				},
				Alignment: AlignHNearVCenter,
			},
			VSeparator{},
			Composite{
				Layout: VBox{MarginsZero: true},
				Children: []Widget{
					Label{
						Font:          Font{PointSize: 10},
						TextAlignment: AlignNear,
						Text:          "版本: " + BuildTags,
					},
					Label{
						Font:          Font{PointSize: 10},
						TextAlignment: AlignNear,
						Text:          "编译时间: " + BuildTime,
					},
					Label{
						Font:          Font{PointSize: 10},
						TextAlignment: AlignNear,
						Text:          "Commit: " + GitCommit,
					},
					Label{
						Font:          Font{PointSize: 10},
						TextAlignment: AlignNear,
						Text:          "Go版本: " + GoVersion,
					},
					Label{
						Font:          Font{PointSize: 10},
						TextAlignment: AlignNear,
						Text:          "GitHub: github.com/xaxys/injbg3",
					},
				},
				Alignment: AlignHCenterVCenter,
			},
			VSpacer{},
		},
	}).Create(NewBuilder(parent)); err != nil {
		return nil, err
	}

	if err := walk.InitWrapperWindow(p); err != nil {
		return nil, err
	}

	return p, nil
}

func (p *AboutPage) OpenRepo(link *walk.LinkLabelLink) {
	url := "https://github.com/xaxys/injbg3"
	err := exec.Command("cmd", "/c", "start", url).Start()
	if err != nil {
		fmt.Println(err)
	}
}
