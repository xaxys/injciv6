使用方法：

双击 injciv6.exe 自动注入正在运行的 文明6 进程（一个进程不要多次注入）
双击 civ6remove.exe 解除注入

第一次注入后，会在 文明6 目录下生成一个 injciv6-config.txt 文件，用于配置要连接的服务器地址。
injciv6-config.txt 文件默认只有一行，初始为 255.255.255.255
请改成要连接的服务器地址，如 192.168.1.100, 或 2001::1234:5678:abcd:ef01, 或 abc.com 等

要使配置生效，请解除注入后重新注入。

然后就可以使用基于 IP 的游戏发现功能了。

注：文明6 目录一般为 \Sid Meier's Civilization VI\Base\Binaries\Win64Steam
