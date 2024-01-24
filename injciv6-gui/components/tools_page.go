package components

import (
	"fmt"
	"injciv6-gui/service"
	"injciv6-gui/utils"
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
	p.infoLabel.SetSuspended(true)
	defer p.infoLabel.SetSuspended(false)

	p.infoLabel.SetText(msg)
	p.infoLabel.SetTextColor(ColorBlack)
}

func (p *ToolsPage) LogError(err error) {
	p.infoLabel.SetSuspended(true)
	defer p.infoLabel.SetSuspended(false)

	p.infoLabel.SetText(err.Error())
	p.infoLabel.SetTextColor(ColorRed)
	fmt.Println(err)
}

func (p *ToolsPage) OpenConfigFile() {
	status := service.Game.Status()

	if status != utils.Civ6StatusRunningDX11 && status != utils.Civ6StatusRunningDX12 {
		p.LogError(fmt.Errorf("请先运行游戏"))
		return
	}
	dir, err := utils.GetCiv6Dir()
	if err != nil {
		errStr := strings.TrimSpace(err.Error())
		p.LogError(fmt.Errorf("获取配置文件路径失败: %v（请尝试以管理员身份运行）", errStr))
		return
	}
	path := filepath.Join(dir, "injciv6-config.txt")
	cmd := exec.Command("cmd", "/C", path)
	cmd.SysProcAttr = &syscall.SysProcAttr{HideWindow: true}
	if err := cmd.Start(); err != nil {
		errStr := strings.TrimSpace(err.Error())
		p.LogError(fmt.Errorf("打开配置文件失败: %v", errStr))
	}
	p.LogInfo("打开配置文件成功")
}

func (p *ToolsPage) OpenLogFile() {
	status := service.Game.Status()
	if status != utils.Civ6StatusRunningDX11 && status != utils.Civ6StatusRunningDX12 {
		p.LogError(fmt.Errorf("请先运行游戏"))
		return
	}
	dir, err := utils.GetCiv6Dir()
	if err != nil {
		errStr := strings.TrimSpace(err.Error())
		p.LogError(fmt.Errorf("获取日志文件路径失败: %v（请尝试以管理员身份运行）", errStr))
		return
	}
	path := filepath.Join(dir, "injciv6-log.txt")
	cmd := exec.Command("cmd", "/C", path)
	cmd.SysProcAttr = &syscall.SysProcAttr{HideWindow: true}
	if err := cmd.Start(); err != nil {
		errStr := strings.TrimSpace(err.Error())
		p.LogError(fmt.Errorf("打开日志文件失败: %v", errStr))
	}
	p.LogInfo("打开日志文件成功")
}
