# 文明6联机-基于IP的游戏发现

> 改自：Hook文明6游戏sendto函数dll和注入程序

原仓库：[https://gitcode.net/PeaZomboss/miscellaneous](https://gitcode.net/PeaZomboss/miscellaneous)

> xaxys: 
> 
> 原作者使用 hook 方式使得 255.255.255.255 的三层广播包发到所有网卡，本人直接修改发包，使得游戏发现的 UDP 包直接单播到指服务器 IP，对于稳定的局域网，效果更好
> 
> IPv6 正在支持，实现原理同 Clash 的 FakeIP, 还没调试完，若成功，将不再需要局域网，直接公网上单播 UDP 包，实现公网联机
>
> 使用方法：见 `bin\Readme.txt` 或 Release 中的 `Readme.txt`

仅支持Windows下的x86和x64平台

请确保熟悉基础的Windows命令行操作

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

## 使用方法

成功编译之后，切换到bin目录，运行injector32.exe或者injector64.exe，具体视游戏而定

例如你要注入文明6，那么请使用`injector64.exe -x=CivilizationVI.exe`或者`injector64.exe -x=CivilizationVI_DX12.exe`

针对文明6，现在可以直接双击`injciv6.exe`来注入，根据提示即可知道结果

如果游戏以管理员权限运行，injector也要以管理员权限运行

> xaxys:
> 
> 双击 injciv6.exe 自动注入正在运行的 文明6 进程（一个进程不要多次注入）
> 
> 双击 civ6remove.exe 解除注入
> 
> 第一次注入后，会在 文明6 目录下生成一个 injciv6-config.txt 文件，用于配置要连接的服务器地址。
> 
> injciv6-config.txt 文件默认只有一行，初始为 255.255.255.255
> 
> 请改成要连接的服务器地址，如 192.168.1.100, 或 2001::1234:5678:abcd:ef01, 或 abc.com 等
> 
> 要使配置生效，请解除注入后重新注入。
> 
> 然后就可以使用基于 IP 的游戏发现功能了。
> 
> 注：文明6 目录一般为 \Sid Meier's Civilization VI\Base\Binaries\Win64Steam

