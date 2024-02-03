package components

import (
	"fmt"
	"injbg3-gui/service"
	"injbg3-gui/utils"
	"os"

	"github.com/lxn/walk"
)

var (
	ColorGreen       = walk.RGB(0x35, 0xA0, 0x28)
	ColorYellowGreen = walk.RGB(0x8C, 0xB4, 0x14)
	ColorYellow      = walk.RGB(0xBB, 0xAA, 0x00)
	ColorOrange      = walk.RGB(0xEB, 0x5C, 0x20)
	ColorRed         = walk.RGB(0xE8, 0x11, 0x23)
	ColorGray        = walk.RGB(0x6C, 0x6C, 0x6C)
	ColorBlack       = walk.RGB(0x00, 0x00, 0x00)
	ColorBackground  = walk.RGB(0xF0, 0xF0, 0xF0)
)

func RerunAsAdmin(msg string) error {
	fullMsg := fmt.Sprintf("%s。是否以管理员权限重试？", msg)
	ret := walk.MsgBox(nil, "错误", fullMsg, walk.MsgBoxIconError|walk.MsgBoxYesNo)
	if ret == walk.DlgCmdNo {
		return nil
	}
	amw, ok := service.Get[*AppMainWindow]("AppMainWindowHandle")
	if ok {
		amw.Hide()
	}
	for {
		path := os.Args[0]
		success, err := utils.RunAsAdmin(path)
		if err != nil {
			return err
		}
		if success {
			if ok {
				amw.Close()
			}
			return nil
		}
		retry := walk.MsgBox(nil, "错误", "请在弹出的窗口中点击“是”", walk.MsgBoxIconError|walk.MsgBoxRetryCancel)
		if retry == walk.DlgCmdCancel {
			if ok {
				amw.Show()
			}
			return nil
		}
	}
}
