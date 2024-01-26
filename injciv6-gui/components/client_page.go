package components

import (
	"fmt"
	"injciv6-gui/service"
	"injciv6-gui/utils"
	"os"
	"os/exec"
	"strings"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
)

type ClientPage struct {
	*walk.Composite
	serverAddrEdit          *walk.LineEdit
	serverAddrValid         bool
	ipv6StatusLabel         *walk.Label
	ipv6ExplainLabel        *walk.Label
	gameStatusLabel         *walk.Label
	injectStatusLabel       *walk.Label
	closeAfterStartCheckBox *walk.CheckBox
	clientStartStopButton   *walk.PushButton
	infoLabel               *walk.Label
}

func NewClientPage(parent walk.Container) (Page, error) {
	p := &ClientPage{}

	if err := (Composite{
		AssignTo: &p.Composite,
		Name:     "客户端",
		Layout:   VBox{},
		Children: []Widget{
			Label{
				Font: Font{PointSize: 10},
				Text: "服务器地址：",
			},
			LineEdit{
				AssignTo:          &p.serverAddrEdit,
				OnTextChanged:     p.OnServerAddrChanged,
				OnEditingFinished: p.OnServerAddrFinished,
			},
			VSeparator{},
			Composite{
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
										Text:          "未知",
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
								Text:       "以客户端模式注入",
								AssignTo:   &p.clientStartStopButton,
								OnClicked:  p.OnClientStartStopButtonClicked,
							},
						},
					},
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

	if service.Game.Status() == utils.Civ6StatusRunningDX11 || service.Game.Status() == utils.Civ6StatusRunningDX12 {
		addr, err := utils.ReadConfig()
		if err != nil {
			errStr := strings.TrimSpace(err.Error())
			p.LogError(fmt.Errorf("读取配置文件失败: %v（请尝试以管理员身份运行）", errStr))
		} else {
			p.serverAddrEdit.SetText(addr)
		}
	}

	service.IPv6.Listener().Register(service.NewFuncListener(p.OnIPv6StatusChanged))
	service.IPv6.ErrorListener().Register(service.NewFuncListener(p.OnIPv6Error))
	service.Game.Listener().Register(service.NewFuncListener(p.OnGameStatusChanged))
	service.Inject.Listener().Register(service.NewFuncListener(p.OnInjectStatusChanged))

	return p, nil
}

func (p *ClientPage) LogInfo(msg string) {
	p.infoLabel.SetText(msg)
	p.infoLabel.SetTextColor(ColorBlack)
}

func (p *ClientPage) LogError(err error) {
	p.infoLabel.SetText(err.Error())
	p.infoLabel.SetTextColor(ColorRed)
	fmt.Println(err)
}

func (p *ClientPage) OnCloseAfterStartChanged() {
	service.Set("CloseAfterStart", p.closeAfterStartCheckBox.Checked())
}

func (p *ClientPage) OnServerAddrFinished() {
	addr := p.serverAddrEdit.Text()
	if utils.IsValidDomainRegex(addr) {
		go func() {
			if !utils.IsValidDomain(addr) {
				p.serverAddrValid = false
				p.serverAddrEdit.SetTextColor(ColorRed)
				p.LogError(fmt.Errorf("域名解析失败"))
				p.updateClientStartStopButton()
			}
		}()
	}
}

func (p *ClientPage) OnServerAddrChanged() {
	addr := p.serverAddrEdit.Text()
	valid := false
	defer func() {
		var color walk.Color
		if valid {
			color = ColorBlack
		} else {
			color = ColorRed
		}

		p.serverAddrEdit.SetTextColor(color)
		p.serverAddrValid = valid

		go p.updateClientStartStopButton()
	}()

	if utils.IsValidDomainRegex(addr) {
		valid = true
		return
	}

	if utils.IsValidIPv4(addr) {
		valid = true
		return
	}

	if utils.IsValidIPv6(addr) {
		status := service.IPv6.Status()
		switch status {
		case utils.IPv6StatusSupported,
			utils.IPv6StatusSupportedMultiple,
			utils.IPv6StatusDNSFail,
			utils.IPv6StatusDNSFailMultiple,
			utils.IPv6StatusOnlineFail,
			utils.IPv6StatusOnlineFailMultiple:
			valid = true
			return
		}
	}

	valid = false
	return
}

func (p *ClientPage) OnIPv6StatusChanged(status service.IPv6WithStatus) {
	p.ipv6StatusLabel.SetSuspended(true)
	p.ipv6ExplainLabel.SetSuspended(true)
	switch status.Status {
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

	case utils.IPv6StatusDNSFail:
		p.ipv6StatusLabel.SetText("部分支持（DNS解析失败）")
		p.ipv6StatusLabel.SetTextColor(ColorYellow)
		p.ipv6StatusLabel.SetToolTipText(status.IPv6)
		p.ipv6ExplainLabel.SetVisible(true)
		p.ipv6ExplainLabel.SetToolTipText("您可以通过IPv6上网，但您的DNS服务器不支持IPv6，域名功能将无法使用")

	case utils.IPv6StatusDNSFailMultiple:
		p.ipv6StatusLabel.SetText("部分支持（DNS解析失败）")
		p.ipv6StatusLabel.SetTextColor(ColorYellow)
		p.ipv6StatusLabel.SetToolTipText(status.IPv6)
		p.ipv6ExplainLabel.SetVisible(true)
		p.ipv6ExplainLabel.SetToolTipText("您可以通过IPv6上网，但您的DNS服务器不支持IPv6，域名功能将无法使用")

	case utils.IPv6StatusSupported, utils.IPv6StatusSupportedMultiple:
		p.ipv6StatusLabel.SetText("支持")
		p.ipv6StatusLabel.SetTextColor(ColorGreen)
		p.ipv6StatusLabel.SetToolTipText(status.IPv6)
		p.ipv6ExplainLabel.SetVisible(false)

	default:
		p.ipv6StatusLabel.SetText("未知")
		p.ipv6StatusLabel.SetTextColor(ColorGray)
		p.ipv6ExplainLabel.SetVisible(false)

	}
	p.ipv6StatusLabel.SetSuspended(false)
	p.ipv6ExplainLabel.SetSuspended(false)
	go p.updateClientStartStopButton()
}

func (p *ClientPage) OnIPv6Error(err error) {
	p.ipv6StatusLabel.SetToolTipText(err.Error())
	if p.serverAddrValid && utils.IsValidIPv4(p.serverAddrEdit.Text()) {
		return
	}
	p.LogError(err)
}

func (p *ClientPage) OnGameStatusChanged(status utils.Civ6Status) {
	p.gameStatusLabel.SetSuspended(true)
	switch status {
	case utils.Civ6StatusRunningDX11:
		p.gameStatusLabel.SetText("运行中 (DX11)")
		p.gameStatusLabel.SetTextColor(ColorGreen)

		path, err := utils.GetCiv6Path()
		if err != nil {
			errStr := strings.TrimSpace(err.Error())
			p.gameStatusLabel.SetToolTipText(fmt.Sprintf("获取游戏路径失败: %s（请尝试以管理员身份运行）", errStr))
		}
		p.gameStatusLabel.SetToolTipText(path)

		if p.serverAddrEdit.Text() == "" {
			addr, err := utils.ReadConfig()
			if err != nil {
				errStr := strings.TrimSpace(err.Error())
				p.LogError(fmt.Errorf("读取配置文件失败: %v（请尝试以管理员身份运行）", errStr))
			} else {
				p.serverAddrEdit.SetText(addr)
			}
		}

	case utils.Civ6StatusRunningDX12:
		p.gameStatusLabel.SetText("运行中 (DX12)")
		p.gameStatusLabel.SetTextColor(ColorGreen)

		path, err := utils.GetCiv6Path()
		if err != nil {
			errStr := strings.TrimSpace(err.Error())
			p.gameStatusLabel.SetToolTipText(fmt.Sprintf("获取游戏路径失败: %s（请尝试以管理员身份运行）", errStr))
		}
		p.gameStatusLabel.SetToolTipText(path)

		if p.serverAddrEdit.Text() == "" {
			addr, err := utils.ReadConfig()
			if err != nil {
				errStr := strings.TrimSpace(err.Error())
				p.LogError(fmt.Errorf("读取配置文件失败: %v（请尝试以管理员身份运行）", errStr))
			} else {
				p.serverAddrEdit.SetText(addr)
			}
		}

	default:
		p.gameStatusLabel.SetText("未运行")
		p.gameStatusLabel.SetTextColor(ColorRed)
	}
	p.gameStatusLabel.SetSuspended(false)
	go p.updateClientStartStopButton()
}

func (p *ClientPage) OnInjectStatusChanged(injectStatus utils.InjectStatus) {
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

func (p *ClientPage) updateClientStartStopButton() {
	enabled := false
	defer func() {
		p.clientStartStopButton.SetEnabled(enabled)
	}()

	// 检查服务器地址是否有效
	if !p.serverAddrValid {
		return
	}

	// 检查游戏是否运行
	gameStatus := service.Game.Status()
	if gameStatus != utils.Civ6StatusRunningDX11 && gameStatus != utils.Civ6StatusRunningDX12 {
		return
	}

	// 检查注入状态
	injectStatus := service.Inject.IsInjected()
	switch injectStatus {
	case utils.InjectStatusRunningIPv6:
		p.clientStartStopButton.SetText("请先返回至主菜单")
		return
	case utils.InjectStatusInjected:
		p.clientStartStopButton.SetText("移除注入")
	case utils.InjectStatusNotInjected:
		p.clientStartStopButton.SetText("以客户端模式注入")
	default:
		p.clientStartStopButton.SetText("未知")
	}
	enabled = true
}

func (p *ClientPage) OnClientStartStopButtonClicked() {
	injectStatus := service.Inject.IsInjected()
	if injectStatus == utils.InjectStatusInjected || injectStatus == utils.InjectStatusRunningIPv6 {
		p.StopClient()
	} else {
		p.StartClient()
	}
}

func (p *ClientPage) StartClient() {
	addr := p.serverAddrEdit.Text()
	if err := utils.WriteConfig(addr); err != nil {
		errStr := strings.TrimSpace(err.Error())
		p.LogError(fmt.Errorf("写入配置文件失败: %v（请尝试以管理员身份运行）", errStr))
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

func (p *ClientPage) StopClient() {
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
