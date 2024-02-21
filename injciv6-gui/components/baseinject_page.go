package components

import (
	"fmt"
	"os"
	"os/exec"
	"strings"

	"injciv6-gui/service"
	"injciv6-gui/utils"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
)

type BaseInjectPageCfg struct {
	PageName                string
	GetConfigContent        func() string
	GetStartStopButtonReady func() bool
	OnIPv6Status            func(status service.IPv6WithStatus)
	OnIPv6Error             func(err error)
	OnGameStatus            func(status utils.Civ6Status)
	OnInjectStatus          func(injectStatus utils.InjectStatus)
	OnInfo                  func(msg string)
	OnError                 func(err error)
}

type BaseInjectPage struct {
	*walk.Composite
	baseInjectPageCfg       *BaseInjectPageCfg
	ipv6StatusLabel         *walk.Label
	ipv6ExplainLabel        *walk.Label
	gameStatusLabel         *walk.Label
	injectStatusLabel       *walk.Label
	closeAfterStartCheckBox *walk.CheckBox
	startStopButton         *walk.PushButton
}

func NewBaseInjectPage(parent walk.Container, cfg *BaseInjectPageCfg) (*BaseInjectPage, error) {
	p := &BaseInjectPage{
		baseInjectPageCfg: cfg,
	}

	if err := (Composite{
		Layout: HBox{MarginsZero: true},
		Children: []Widget{
			Composite{
				Layout: VBox{MarginsZero: true},
				Children: []Widget{
					Composite{
						Layout: HBox{MarginsZero: true},
						Children: []Widget{
							Label{
								Text:          "IPv6状态: ",
								TextAlignment: AlignNear,
							},
							Label{
								Font:          Font{Bold: true},
								AssignTo:      &p.ipv6StatusLabel,
								Text:          "未知（测试中）",
								TextColor:     ColorGray,
								Background:    SolidColorBrush{Color: ColorBackground},
								TextAlignment: AlignNear,
							},
							Label{
								Font:          Font{Underline: true},
								AssignTo:      &p.ipv6ExplainLabel,
								Text:          "解释",
								TextColor:     ColorGray,
								Background:    SolidColorBrush{Color: ColorBackground},
								TextAlignment: AlignNear,
								Visible:       false,
							},
						},
						Alignment: AlignHNearVCenter,
					},
					Composite{
						Layout: HBox{MarginsZero: true},
						Children: []Widget{
							Label{
								Text:          "游戏状态: ",
								TextAlignment: AlignNear,
							},
							Label{
								Font:          Font{Bold: true},
								AssignTo:      &p.gameStatusLabel,
								Text:          "未知",
								TextColor:     ColorGray,
								Background:    SolidColorBrush{Color: ColorBackground},
								TextAlignment: AlignNear,
							},
						},
						Alignment: AlignHNearVCenter,
					},
					Composite{
						Layout: HBox{MarginsZero: true},
						Children: []Widget{
							Label{
								Text:          "注入状态: ",
								TextAlignment: AlignNear,
							},
							Label{
								Font:          Font{Bold: true},
								AssignTo:      &p.injectStatusLabel,
								Text:          "未注入",
								TextColor:     ColorRed,
								Background:    SolidColorBrush{Color: ColorBackground},
								TextAlignment: AlignNear,
							},
						},
						Alignment: AlignHNearVCenter,
					},
					CheckBox{
						Text:             "注入后关闭此程序",
						Checked:          service.GetWithDef("CloseAfterStart", true),
						OnCheckedChanged: p.OnCloseAfterStartChanged,
						AssignTo:         &p.closeAfterStartCheckBox,
						Alignment:        AlignHNearVCenter,
					},
				},
			},
			HSpacer{},
			Composite{
				Layout: VBox{},
				Children: []Widget{
					PushButton{
						Persistent: true,
						MinSize:    Size{Width: 230, Height: 80},
						Enabled:    false,
						Text:       fmt.Sprintf("以%s模式注入", cfg.PageName),
						AssignTo:   &p.startStopButton,
						OnClicked:  p.OnServerStartStopButtonClicked,
					},
				},
			},
		},
	}).Create(NewBuilder(parent)); err != nil {
		return nil, err
	}

	service.IPv6.Listener().Register(service.NewFuncListener(p.OnIPv6StatusChanged))
	service.IPv6.ErrorListener().Register(service.NewFuncListener(p.OnIPv6Error))
	service.Game.Listener().Register(service.NewFuncListener(p.OnGameStatusChanged))
	service.Inject.Listener().Register(service.NewFuncListener(p.OnInjectStatusChanged))

	service.IPv6.Listener().Register(service.NewFuncListener(cfg.OnIPv6Status))
	service.IPv6.ErrorListener().Register(service.NewFuncListener(cfg.OnIPv6Error))
	service.Game.Listener().Register(service.NewFuncListener(cfg.OnGameStatus))
	service.Inject.Listener().Register(service.NewFuncListener(cfg.OnInjectStatus))

	return p, nil
}

func (p *BaseInjectPage) LogInfo(msg string) {
	p.baseInjectPageCfg.OnInfo(msg)
}

func (p *BaseInjectPage) LogError(err error) {
	p.baseInjectPageCfg.OnError(err)
}

func (p *BaseInjectPage) OnCloseAfterStartChanged() {
	service.Set("CloseAfterStart", p.closeAfterStartCheckBox.Checked())
}

func (p *BaseInjectPage) OnIPv6StatusChanged(status service.IPv6WithStatus) {
	p.ipv6StatusLabel.SetSuspended(true)
	p.ipv6ExplainLabel.SetSuspended(true)
	switch status.Status {
	case utils.IPv6StatusUnknown:
		p.ipv6StatusLabel.SetText("未知（测试中）")
		p.ipv6StatusLabel.SetTextColor(ColorGray)
		p.ipv6ExplainLabel.SetVisible(false)

	case utils.IPv6StatusNotSupported:
		p.ipv6StatusLabel.SetText("不支持")
		p.ipv6StatusLabel.SetTextColor(ColorRed)
		p.ipv6ExplainLabel.SetVisible(false)

	case utils.IPv6StatusOnlineFail:
		p.ipv6StatusLabel.SetText("可能不支持（无法上网）")
		p.ipv6StatusLabel.SetTextColor(ColorOrange)
		p.ipv6StatusLabel.SetToolTipText(status.IPv6)
		p.ipv6ExplainLabel.SetVisible(true)
		p.ipv6ExplainLabel.SetToolTipText("在本地找到的IPv6地址，但无法连接IPv6网站，除非您未连接到互联网，否则您大概率不支持IPv6")

	case utils.IPv6StatusOnlineFailMultiple:
		p.ipv6StatusLabel.SetText("可能不支持（无法上网）")
		p.ipv6StatusLabel.SetTextColor(ColorOrange)
		p.ipv6StatusLabel.SetToolTipText("多个IPv6地址，请手动选择一个地址")
		p.ipv6ExplainLabel.SetVisible(true)
		p.ipv6ExplainLabel.SetToolTipText("在本地找到的IPv6地址，但无法连接IPv6网站，除非您未连接到互联网，否则您大概率不支持IPv6")

	case utils.IPv6StatusDNSFail, utils.IPv6StatusDNSFailMultiple:
		p.ipv6StatusLabel.SetText("部分支持（DNS解析失败）")
		p.ipv6StatusLabel.SetTextColor(ColorYellow)
		p.ipv6StatusLabel.SetToolTipText(status.IPv6)
		p.ipv6ExplainLabel.SetVisible(true)
		p.ipv6ExplainLabel.SetToolTipText("您可以通过IPv6上网，但您的DNS服务器不支持IPv6。可以正常输入IPv6地址联机，但IPv6域名功能将无法使用")

	case utils.IPv6StatusDNSNotPreferred, utils.IPv6StatusDNSNotPreferredMultiple:
		p.ipv6StatusLabel.SetText("支持（非首选地址）")
		p.ipv6StatusLabel.SetTextColor(ColorGreen)
		p.ipv6StatusLabel.SetToolTipText(status.IPv6)
		p.ipv6ExplainLabel.SetVisible(true)
		p.ipv6ExplainLabel.SetToolTipText("您可以通过IPv6上网，您的DNS服务器也支持IPv6，但您的电脑似乎不太愿意使用IPv6。可以正常输入IPv6地址联机，但双栈域名功能可能无法使用")

	case utils.IPv6StatusSupported, utils.IPv6StatusSupportedMultiple:
		p.ipv6StatusLabel.SetText("支持")
		p.ipv6StatusLabel.SetTextColor(ColorGreen)
		p.ipv6StatusLabel.SetToolTipText(status.IPv6)
		p.ipv6ExplainLabel.SetVisible(false)

	default:
		p.ipv6StatusLabel.SetText("其他")
		p.ipv6StatusLabel.SetTextColor(ColorGray)
		p.ipv6ExplainLabel.SetVisible(false)

	}
	p.ipv6StatusLabel.SetSuspended(false)
	p.ipv6ExplainLabel.SetSuspended(false)
	go p.updateClientStartStopButton()
}

func (p *BaseInjectPage) OnIPv6Error(err error) {
	p.ipv6StatusLabel.SetToolTipText(err.Error())
	p.LogError(err)
}

func (p *BaseInjectPage) OnGameStatusChanged(status utils.Civ6Status) {
	p.gameStatusLabel.SetSuspended(true)

	setGamePathTip := func() {
		path, err := utils.GetCiv6Path()
		if err != nil {
			err = fmt.Errorf("获取游戏路径失败: %v", err)
			p.gameStatusLabel.SetToolTipText(err.Error())
			go RerunAsAdmin(err.Error())
		}
		p.gameStatusLabel.SetToolTipText(path)
	}

	switch status {
	case utils.Civ6StatusRunningDX11:
		p.gameStatusLabel.SetText("运行中 (DX11)")
		p.gameStatusLabel.SetTextColor(ColorGreen)
		setGamePathTip()

	case utils.Civ6StatusRunningDX12:
		p.gameStatusLabel.SetText("运行中 (DX12)")
		p.gameStatusLabel.SetTextColor(ColorGreen)
		setGamePathTip()

	default:
		p.gameStatusLabel.SetText("未运行")
		p.gameStatusLabel.SetTextColor(ColorRed)
		p.gameStatusLabel.SetToolTipText("请先运行游戏")
	}
	p.gameStatusLabel.SetSuspended(false)

	go p.updateClientStartStopButton()
}

func (p *BaseInjectPage) OnInjectStatusChanged(injectStatus utils.InjectStatus) {
	switch injectStatus {
	case utils.InjectStatusRunningIPv6:
		p.injectStatusLabel.SetText("运行中 (IPv6)")
		p.injectStatusLabel.SetTextColor(ColorGreen)
	case utils.InjectStatusInjected:
		p.injectStatusLabel.SetText("已注入")
		p.injectStatusLabel.SetTextColor(ColorGreen)
	case utils.InjectStatusNotInjected:
		p.injectStatusLabel.SetText("未注入")
		p.injectStatusLabel.SetTextColor(ColorRed)
	default:
		p.injectStatusLabel.SetText("未知")
		p.injectStatusLabel.SetTextColor(ColorGray)
	}
	go p.updateClientStartStopButton()
}

func (p *BaseInjectPage) updateClientStartStopButton() {
	enabled := false
	defer func() {
		p.startStopButton.SetEnabled(enabled)
	}()

	// 提前检查，如：检查服务器地址是否有效
	if !p.baseInjectPageCfg.GetStartStopButtonReady() {
		return
	}

	// 检查游戏是否运行
	gameStatus := service.Game.Status()
	switch gameStatus {
	case utils.Civ6StatusRunningDX11, utils.Civ6StatusRunningDX12:
		// pass
	default:
		return
	}

	// 检查注入状态
	injectStatus := service.Inject.IsInjected()
	switch injectStatus {
	case utils.InjectStatusRunningIPv6:
		p.startStopButton.SetText("请先返回至游戏主菜单")
		return
	case utils.InjectStatusInjected:
		p.startStopButton.SetText("移除注入")
	case utils.InjectStatusNotInjected:
		p.startStopButton.SetText(fmt.Sprintf("以%s模式注入", p.baseInjectPageCfg.PageName))
	default:
		p.startStopButton.SetText("未知")
	}
	enabled = true
}

func (p *BaseInjectPage) OnServerStartStopButtonClicked() {
	injectStatus := service.Inject.IsInjected()
	switch injectStatus {
	case utils.InjectStatusInjected, utils.InjectStatusRunningIPv6:
		p.StopInject()
	default:
		p.StartInject()
	}
}

func (p *BaseInjectPage) StartInject() {
	content := p.baseInjectPageCfg.GetConfigContent()
	if err := utils.WriteConfig(content); err != nil {
		errStr := strings.TrimSpace(err.Error())
		p.LogError(fmt.Errorf("写入配置文件失败: %v", errStr))
		return
	}
	path, ok := utils.GetInjectorPath()
	if !ok {
		p.LogError(fmt.Errorf("注入工具未找到"))
		return
	}
	cmd := exec.Command(path, "-s")
	err := cmd.Start()
	if err != nil {
		p.LogError(fmt.Errorf("启动注入工具失败: %v", err))
		return
	}
	if p.closeAfterStartCheckBox.Checked() {
		os.Exit(0)
	}
	p.LogInfo("启动注入工具成功")
}

func (p *BaseInjectPage) StopInject() {
	path, ok := utils.GetInjectRemoverPath()
	if !ok {
		p.LogError(fmt.Errorf("注入移除工具未找到"))
		return
	}
	cmd := exec.Command(path)
	err := cmd.Start()
	if err != nil {
		p.LogError(fmt.Errorf("启动注入移除工具失败: %v", err))
		return
	}
	p.LogInfo("启动注入移除工具成功")
}
