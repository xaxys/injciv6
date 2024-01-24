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

type ServerPage struct {
	*walk.Composite
	serverAddrEdit          *walk.LineEdit
	ipv6StatusLabel         *walk.Label
	ipv6ExplainLabel        *walk.Label
	gameStatusLabel         *walk.Label
	injectStatusLabel       *walk.Label
	closeAfterStartCheckBox *walk.CheckBox
	serverStartStopButton   *walk.PushButton
	infoLabel               *walk.Label
}

func NewServerPage(parent walk.Container) (Page, error) {
	p := &ServerPage{}

	if err := (Composite{
		AssignTo: &p.Composite,
		Name:     "服务器",
		Layout:   VBox{},
		Children: []Widget{
			Label{
				Font: Font{PointSize: 10},
				Text: "本机IPv6地址：",
			},
			LineEdit{
				AssignTo:      &p.serverAddrEdit,
				OnTextChanged: p.OnServerAddrChanged,
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
								Text:       "以服务器模式注入",
								AssignTo:   &p.serverStartStopButton,
								OnClicked:  p.OnServerStartStopButtonClicked,
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

	service.IPv6.Listener().Register(service.NewFuncListener(p.OnIPv6StatusChanged))
	service.IPv6.ErrorListener().Register(service.NewFuncListener(p.OnIPv6Error))
	service.Game.Listener().Register(service.NewFuncListener(p.OnGameStatusChanged))
	service.Inject.Listener().Register(service.NewFuncListener(p.OnInjectStatusChanged))

	return p, nil
}

func (p *ServerPage) LogInfo(msg string) {
	p.infoLabel.SetText(msg)
	p.infoLabel.SetTextColor(ColorBlack)
}

func (p *ServerPage) LogError(err error) {
	p.infoLabel.SetText(err.Error())
	p.infoLabel.SetTextColor(ColorRed)
	fmt.Println(err)
}

func (p *ServerPage) OnCloseAfterStartChanged() {
	service.Set("CloseAfterStart", p.closeAfterStartCheckBox.Checked())
}

func (p *ServerPage) OnServerAddrChanged() {
	if p.serverAddrEdit.Text() != service.IPv6.IPv6() {
		p.serverAddrEdit.SetText(service.IPv6.IPv6())
	}
}

func (p *ServerPage) OnIPv6StatusChanged(status service.IPv6WithStatus) {
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
		p.serverAddrEdit.SetText(status.IPv6)
		p.ipv6ExplainLabel.SetVisible(true)
		p.ipv6ExplainLabel.SetToolTipText("在本地找到的IPv6地址，但无法连接IPv6网站，除非您未连接到互联网，否则您大概率不支持IPv6")

	case utils.IPv6StatusOnlineFailMultiple:
		p.ipv6StatusLabel.SetText("可能不支持（无法上网）")
		p.ipv6StatusLabel.SetTextColor(ColorOrange)
		p.ipv6StatusLabel.SetToolTipText("多个IPv6地址，请手动选择一个地址")
		p.serverAddrEdit.SetText(status.IPv6)
		p.ipv6ExplainLabel.SetVisible(true)
		p.ipv6ExplainLabel.SetToolTipText("在本地找到的IPv6地址，但无法连接IPv6网站，除非您未连接到互联网，否则您大概率不支持IPv6")

	case utils.IPv6StatusDNSFail:
		p.ipv6StatusLabel.SetText("部分支持（DNS解析失败）")
		p.ipv6StatusLabel.SetTextColor(ColorYellow)
		p.ipv6StatusLabel.SetToolTipText(status.IPv6)
		p.serverAddrEdit.SetText(status.IPv6)
		p.ipv6ExplainLabel.SetVisible(true)
		p.ipv6ExplainLabel.SetToolTipText("您可以通过IPv6上网，但您的DNS服务器不支持IPv6，域名功能将无法使用")

	case utils.IPv6StatusDNSFailMultiple:
		p.ipv6StatusLabel.SetText("部分支持（DNS解析失败）")
		p.ipv6StatusLabel.SetTextColor(ColorYellow)
		p.ipv6StatusLabel.SetToolTipText(status.IPv6)
		p.serverAddrEdit.SetText(status.IPv6)
		p.ipv6ExplainLabel.SetVisible(true)
		p.ipv6ExplainLabel.SetToolTipText("您可以通过IPv6上网，但您的DNS服务器不支持IPv6，域名功能将无法使用")

	case utils.IPv6StatusSupported, utils.IPv6StatusSupportedMultiple:
		p.ipv6StatusLabel.SetText("支持")
		p.ipv6StatusLabel.SetTextColor(ColorGreen)
		p.ipv6StatusLabel.SetToolTipText(status.IPv6)
		p.serverAddrEdit.SetText(status.IPv6)
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

func (p *ServerPage) OnIPv6Error(err error) {
	p.ipv6StatusLabel.SetToolTipText(err.Error())
	p.LogError(err)
}

func (p *ServerPage) OnGameStatusChanged(status utils.Civ6Status) {
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

	case utils.Civ6StatusRunningDX12:
		p.gameStatusLabel.SetText("运行中 (DX12)")
		p.gameStatusLabel.SetTextColor(ColorGreen)

		path, err := utils.GetCiv6Path()
		if err != nil {
			errStr := strings.TrimSpace(err.Error())
			p.gameStatusLabel.SetToolTipText(fmt.Sprintf("获取游戏路径失败: %s（请尝试以管理员身份运行）", errStr))
		}
		p.gameStatusLabel.SetToolTipText(path)

	default:
		p.gameStatusLabel.SetText("未运行")
		p.gameStatusLabel.SetTextColor(ColorRed)
	}
	p.gameStatusLabel.SetSuspended(false)
	go p.updateClientStartStopButton()
}

func (p *ServerPage) OnInjectStatusChanged(injected bool) {
	if injected {
		p.injectStatusLabel.SetText("已注入")
		p.injectStatusLabel.SetTextColor(ColorGreen)
	} else {
		p.injectStatusLabel.SetText("未注入")
		p.injectStatusLabel.SetTextColor(ColorRed)
	}
	go p.updateClientStartStopButton()
}

func (p *ServerPage) updateClientStartStopButton() {
	enabled := false
	defer func() {
		p.serverStartStopButton.SetEnabled(enabled)
	}()

	// 检查游戏是否运行
	status := service.Game.Status()
	if status != utils.Civ6StatusRunningDX11 && status != utils.Civ6StatusRunningDX12 {
		return
	}

	// 检查注入状态
	injected := service.Inject.IsInjected()
	if injected {
		p.serverStartStopButton.SetText("移除注入")
	} else {
		p.serverStartStopButton.SetText("以服务器模式注入")
	}
	enabled = true
}

func (p *ServerPage) OnServerStartStopButtonClicked() {
	if service.Inject.IsInjected() {
		p.StopServer()
	} else {
		p.StartServer()
	}
}

func (p *ServerPage) StartServer() {
	if err := utils.WriteConfig("::0"); err != nil {
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

func (p *ServerPage) StopServer() {
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
