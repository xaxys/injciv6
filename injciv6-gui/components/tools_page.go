package components

import (
	"fmt"
	"injciv6-gui/service"
	"injciv6-gui/utils"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"syscall"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
)

type ToolsPage struct {
	*walk.Composite
	infoLabel *walk.Label
}

func NewToolsPage(parent walk.Container) (Page, error) {
	p := &ToolsPage{}

	if err := (Composite{
		AssignTo: &p.Composite,
		Name:     "工具",
		Layout:   VBox{},
		Children: []Widget{
			VSpacer{},
			Composite{
				Layout: HBox{},
				Children: []Widget{
					HSpacer{},
					PushButton{
						Text:      "打开配置文件",
						OnClicked: p.OpenConfigFile,
					},
					HSpacer{},
					PushButton{
						Text:      "打开日志文件",
						OnClicked: p.OpenLogFile,
					},
					HSpacer{},
				},
			},
			VSpacer{},
			VSeparator{},
			Label{
				Font:          Font{PointSize: 10},
				TextAlignment: AlignFar,
				AssignTo:      &p.infoLabel,
				Text:          "　",
				Background:    SolidColorBrush{Color: ColorBackground},
			},
		},
	}).Create(NewBuilder(parent)); err != nil {
		return nil, err
	}

	if err := walk.InitWrapperWindow(p); err != nil {
		return nil, err
	}

	return p, nil
}

func (p *ToolsPage) LogInfo(msg string) {
	p.infoLabel.SetText(msg)
	p.infoLabel.SetTextColor(ColorBlack)
}

func (p *ToolsPage) LogError(err error) {
	p.infoLabel.SetText(err.Error())
	p.infoLabel.SetTextColor(ColorRed)
	fmt.Println(err)
}

func (p *ToolsPage) OpenFile(name, filename string) {
	status := service.Game.Status()

	if status != utils.Civ6StatusRunningDX11 && status != utils.Civ6StatusRunningDX12 {
		p.LogError(fmt.Errorf("请先运行游戏"))
		return
	}
	dir, err := utils.GetCiv6Dir()
	if err != nil {
		errStr := strings.TrimSpace(err.Error())
		p.LogError(fmt.Errorf("获取%s文件路径失败: %s", name, errStr))
		go RerunAsAdmin(fmt.Sprintf("获取%s文件路径失败: %s", name, errStr))
		return
	}
	path := filepath.Join(dir, filename)
	if _, err := os.Stat(path); err != nil {
		if os.IsNotExist(err) {
			p.LogError(fmt.Errorf("打开%s文件失败: 文件不存在", name))
			return
		}
		p.LogError(fmt.Errorf("打开%s文件失败: %s", name, err.Error()))
		return
	}

	cmd := exec.Command("cmd", "/C", path)
	cmd.SysProcAttr = &syscall.SysProcAttr{HideWindow: true}
	if err := cmd.Start(); err != nil {
		p.LogError(fmt.Errorf("打开%s文件失败: %s", name, err.Error()))
	}
	p.LogInfo(fmt.Sprintf("打开%s文件成功", name))
}

func (p *ToolsPage) OpenConfigFile() {
	p.OpenFile("配置文件", "injciv6-config.txt")
}

func (p *ToolsPage) OpenLogFile() {
	p.OpenFile("日志文件", "injciv6-log.txt")
}
