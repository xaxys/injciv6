package components

import (
	"fmt"
	"injciv6-gui/service"
	"injciv6-gui/utils"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
)

type ClientPage struct {
	*walk.Composite
	*BaseInjectPage
	serverAddrEdit     *walk.LineEdit
	serverAddrValid    bool
	baseInjectPageComp *walk.Composite
	infoLabel          *walk.Label
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
				AssignTo: &p.baseInjectPageComp,
				Layout:   HBox{MarginsZero: true, SpacingZero: true},
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

	cfg := &BaseInjectPageCfg{
		PageName:                "客户端",
		GetConfigContent:        p.GetConfigContent,
		GetStartStopButtonReady: p.GetStartStopButtonReady,
		OnIPv6Status:            p.OnIPv6StatusChanged,
		OnIPv6Error:             p.OnIPv6Error,
		OnGameStatus:            p.OnGameStatusChanged,
		OnInjectStatus:          p.OnInjectStatusChanged,
		OnInfo:                  p.LogInfo,
		OnError:                 p.LogError,
	}

	var err error
	if p.BaseInjectPage, err = NewBaseInjectPage(p.baseInjectPageComp, cfg); err != nil {
		return nil, err
	}

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

func (p *ClientPage) OnServerAddrFinished() {
	addr := p.serverAddrEdit.Text()
	if utils.IsValidDomainRegex(addr) {
		go func() {
			if !utils.IsValidDomain(addr) {
				p.serverAddrValid = false
				p.serverAddrEdit.SetTextColor(ColorRed)
				p.LogError(fmt.Errorf("%s域名解析失败", addr))
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
			utils.IPv6StatusDNSNotPreferred,
			utils.IPv6StatusDNSNotPreferredMultiple,
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
}

func (p *ClientPage) OnIPv6Error(err error) {
	p.ipv6StatusLabel.SetToolTipText(err.Error())
	if p.serverAddrValid && utils.IsValidIPv4(p.serverAddrEdit.Text()) {
		return
	}
	p.LogError(err)
}

func (p *ClientPage) OnGameStatusChanged(status utils.Civ6Status) {
	switch status {
	case utils.Civ6StatusRunningDX11, utils.Civ6StatusRunningDX12:
		if p.serverAddrEdit.Text() == "" {
			addr, err := utils.ReadConfig()
			if err != nil {
				p.LogError(fmt.Errorf("读取配置文件失败: %v", err))
			} else {
				p.serverAddrEdit.SetText(addr)
			}
		}
	default:
	}
}

func (p *ClientPage) OnInjectStatusChanged(injectStatus utils.InjectStatus) {
}

func (p *ClientPage) GetStartStopButtonReady() bool {
	// 检查服务器地址是否有效
	if !p.serverAddrValid {
		return false
	}
	return true
}

func (p *ClientPage) GetConfigContent() string {
	return p.serverAddrEdit.Text()
}
