# 文明6联机-基于IP的游戏发现 (IPv4/IPv6)

> 改自：Hook文明6游戏sendto函数dll和注入程序
>
> 原作者仓库：[https://gitcode.net/PeaZomboss/miscellaneous](https://gitcode.net/PeaZomboss/miscellaneous)

公网 UDP 单播联机，不再需要虚拟局域网！

利用 Hook 技术，拦截游戏对 UDP 的发送、接收等操作，实现基于 IP 的游戏发现和联机

至此，文明6联机不再需要诸如 n2n, OpenVPN, Hamachi 等虚拟局域网，直接公网联机！

主要实现以下两点

- 拦截客户端发现服务器时发出的 UDP 广播包，将其改为单播到指定服务器 IP，实现稳定的游戏发现 (IPv4/IPv6)
- 对于 IPv6
  - 重定向对 `0.0.0.0` 的监听到 `[::0]`
  - 建立 FakeIP 表，将 IPv6 地址映射到 `127.0.127.1 ~ 127.0.127.100` 并提供给 文明6 虚假的 IPv4 地址 (实现原理同 Clash 的 FakeIP)

## 使用方法

双击 `injciv6.exe` 自动注入正在运行的 文明6 进程（一个进程不要多次注入）

双击 `civ6remove.exe` 解除注入

第一次注入后，会在 文明6 目录下生成一个 `injciv6-config.txt` 文件，用于配置要连接的服务器地址。

`injciv6-config.txt` 文件默认只有一行，初始为 `255.255.255.255`

请改成要连接的服务器地址，如 `192.168.1.100`, 或 `2001::1234:5678:abcd:ef01`, 或 `abc.com` 等

要使配置生效，请解除注入后重新注入。

然后就可以使用基于 IP 的游戏发现功能了。

**如果发现不了房间，尝试关闭 Windows 防火墙，以及检查路由器防火墙是否放通**

**如果程序报毒，那我也不知道为啥报毒，把 Windows Defender 关了再运行吧**

- 当使用 ipv4 联机时，一般在客户端上进行注入，配置成服务器的 ipv4 地址即可。服务端无需注入
- 当使用 ipv6 联机时，在客户端注入并配置成服务器的 ipv6 地址的同时。服务端也需要注入，理论上地址可以任意 ipv6 地址，一般建议填 `::0` 。

注：文明6 目录一般为 `\Sid Meier's Civilization VI\Base\Binaries\Win64Steam` 或 `\Sid Meier's Civilization VI\Base\Binaries\Win64EOS`

仅支持 Windows 下的 ~~x86~~ 和 x64 平台 (x86 没测过)

---

也可以单独运行 `injector32.exe` 或者 `injector64.exe` 注入

例如你要注入文明6，那么请使用 `injector64.exe -x=CivilizationVI.exe` 或者 `injector64.exe -x=CivilizationVI_DX12.exe`

如果游戏以管理员权限运行，injector也要以管理员权限运行

## 目录说明

- bin存放编译后的二进制文件输出目录
- inc存放头文件
- src存放源代码
- hookdll存放dll的代码和Makefile
- injector存放注入程序的代码和Makefile
- utils存放实用程序代码
- test存放测试用代码

## 编译方法

首先确保同时安装了x86和x64的*MinGW-w64*工具链，已经有的可以略过
下载地址<https://github.com/niXman/mingw-builds-binaries/releases>  
注意要下载两个，一个带**i686**前缀，一个带**x86_64**前缀  
将它们分别解压到不同目录，然后将二者的bin目录都设置环境变量

当然如果你用msys2也可以，会配置完全没问题

切换工作目录到当前目录，运行`mingw32-make`即可  
如果你单独安装了*GNU make*，那么可以直接运行`make`

> xaxys:
>
> 下载上面的两个压缩包，一个解压到本文件夹下的 mingw32, 一个解压到本文件夹下的 mingw64
>
> 然后运行 env.bat, 会自动设置环境变量, 开箱即用

---

要编译utils下的辅助程序，请到[Lazarus官网](https://www.lazarus-ide.org/)下载并安装最新版的*Lazarus IDE*

然后用Lazarus打开auxtool.lpi工程文件，在菜单栏中依次选择**运行-构建**即可，exe会生成在bin目录下

## 后记

非常感谢原作者的注入框架，也非常感谢 DawningW 大佬的帮助
