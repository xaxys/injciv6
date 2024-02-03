package components

import (
	"fmt"

	"injbg3-gui/service"
	"injbg3-gui/utils"

	"github.com/lxn/walk"
	. "github.com/lxn/walk/declarative"
)

type ServerPage struct {
	*walk.Composite
	*BaseInjectPage
	serverAddrEdit     *walk.LineEdit
	baseInjectPageComp *walk.Composite
	infoLabel          *walk.Label
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
		PageName:                "服务器",
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

func (p *ServerPage) LogInfo(msg string) {
	p.infoLabel.SetText(msg)
	p.infoLabel.SetTextColor(ColorBlack)
}

func (p *ServerPage) LogError(err error) {
	p.infoLabel.SetText(err.Error())
	p.infoLabel.SetTextColor(ColorRed)
	fmt.Println(err)
}

func (p *ServerPage) OnServerAddrChanged() {
	if p.serverAddrEdit.Text() != service.IPv6.IPv6() {
		p.serverAddrEdit.SetText(service.IPv6.IPv6())
	}
}

func (p *ServerPage) OnIPv6StatusChanged(status service.IPv6WithStatus) {
	switch status.Status {
	case utils.IPv6StatusOnlineFail:
		p.serverAddrEdit.SetText(status.IPv6)

	case utils.IPv6StatusOnlineFailMultiple:
		p.serverAddrEdit.SetText("多个IPv6地址，请手动选择一个地址")

	case utils.IPv6StatusDNSFail,
		utils.IPv6StatusDNSFailMultiple,
		utils.IPv6StatusDNSNotPreferred,
		utils.IPv6StatusDNSNotPreferredMultiple,
		utils.IPv6StatusSupported,
		utils.IPv6StatusSupportedMultiple:
		p.serverAddrEdit.SetText(status.IPv6)

	default:
		p.serverAddrEdit.SetText("")

	}
}

func (p *ServerPage) OnIPv6Error(err error) {
}

func (p *ServerPage) OnGameStatusChanged(status utils.BG3Status) {
}

func (p *ServerPage) OnInjectStatusChanged(injectStatus utils.InjectStatus) {
}

func (p *ServerPage) GetStartStopButtonReady() bool {
	return true
}

func (p *ServerPage) GetConfigContent() string {
	return "::0"
}
