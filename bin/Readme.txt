使用方法：

============================== 使用 injciv6-gui 工具 =====================================

先启动游戏，然后双击 injciv6-gui.exe 打开图形界面。

进行 IPv6 连接时
在服务端，选择【服务器】模式，告诉其他玩家本机的 IPv6 地址，然后点击【以服务器模式注入】按钮。
在客户端，选择【客户端】模式，在【服务器地址】中填写服务端的 IPv6 地址，然后点击【以客户端模式注入】按钮。
注入状态显示为【已注入】时，即可开始游戏。

进行 IPv4 连接时
需要先行搭建虚拟局域网，服务端无需注入。
在客户端，选择【客户端】模式，在【服务器地址】中填写服务端的 IPv4 地址，然后点击【以客户端模式注入】按钮。

IPv6状态有以下几种：
【不支持】：本机不支持 IPv6，检查IPv6设置，如适配器是否勾选了 IPv6 协议。
【可能不支持（无法上网）】：在本地发现 IPv6 单播地址，但是无法连接到互联网，检查互联网连接是否正常。（除非你想用 IPv6 进行局域网联机）
【部分支持（DNS解析失败）】：在本地发现 IPv6 单播地址，且可以连接到互联网，但是由于 DNS 不支持 IPv6，请不要在【服务器地址】中填写 IPv6 域名，只应当填写 IP 地址。
【支持（非首选地址）】：在本地发现 IPv6 单播地址，且可以连接到互联网，DNS 也支持 IPv6，但是 IPv6 地址不是首选地址，在【服务器地址】中填写 IPv4/IPv6 双栈域名有可能会导致连接失败。
【支持】：IPv6完全受支持，但是您仍然可能需要自行检查防火墙设置。

================================ 直接使用原始注入工具 ======================================

双击 injciv6.exe 自动注入正在运行的 文明6 进程
双击 civ6remove.exe 解除注入

第一次注入后，会在 文明6 目录下生成一个 injciv6-config.txt 文件，用于配置要连接的服务器地址。
injciv6-config.txt 文件默认只有一行，初始为 255.255.255.255
请改成要连接的服务器地址，如 192.168.1.100, 或 2001::1234:5678:abcd:ef01, 或 abc.com 等

要使配置生效，请再次双击 injciv6.exe 根据提示重新注入。

然后就可以使用基于 IP 的游戏发现功能了。

当使用 ipv4 联机时，一般在客户端上进行注入，配置成服务器的 ipv4 地址即可。服务端无需注入
当使用 ipv6 联机时，在客户端注入并配置成服务器的 ipv6 地址的同时。服务端也需要注入，理论上地址可以任意 ipv6 地址，一般建议填 ::0 。

注：文明6 目录一般为 \Sid Meier's Civilization VI\Base\Binaries\Win64Steam 或 \Sid Meier's Civilization VI\Base\Binaries\Win64EOS

================================= 疑难解答 =============================================

Q: 为什么我注入时显示“注入失败”？
A: 最常见的原因是“拒绝访问”，若您以管理员身份注入仍然失败，大概率是您的杀毒软件在内核层面拦截了注入操作，请尝试关闭杀毒软件后再次注入。

Q: 为什么我注入后，游戏崩溃了？
A: 如果您是启动游戏后第一次注入后，经过特定操作后，能稳定复现崩溃，那么请加群反馈 BUG。

Q: 为什么我解除注入后，游戏崩溃了？
A: 请不要在游戏中，或者游戏房间，或者非游戏主菜单以外的界面解除注入。

Q: 为什么我注入后，无法发现房间？
A: 最有可能的原因是您的防火墙阻止了游戏的网络连接，可以尝试先关闭 Windows 防火墙，如果不行，则可能是路由器或运营商的防火墙。
   让作客户端的玩家尝试 ping 服务器地址，如果 ping 不通，则说明服务器侧防火墙阻止了连接。（服务器侧需要关闭防火墙）
   同时，作为服务器的玩家也可以尝试 ping 客户端的地址，如果 ping 不通，则说明可能是客户端侧防火墙阻止了连接。（取决于防火墙类型，默认情况客户端不需要关闭防火墙）

Q: 为什么我注入后，能发现房间，但是一直卡在“正在检索创建人信息”？
A: 请按照上一条的方法检查防火墙设置。但也不排除是游戏版本/DLC不一致导致的问题，尝试调整房间设置后再次尝试。

Q: 为什么我能正常发现房间，也可以正常加入房间，但是3个人及以上无法同时加入游戏？
A: 这是一个已知的奇怪问题，如果您使用的是 IPv4 公网联机，暂时没有很好的解决方案。如果您使用的是 IPv6 公网联机，可以在服务端使用 >=v0.3.3 版本的 injciv6-gui工具。
   如果更改版本后客户端侧变为了无法发现房间，可以尝试在客户端使用 <=v0.3.2 版本的 injciv6-gui工具。(有待解决)
