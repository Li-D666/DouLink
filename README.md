# DouLink

DouLink 是一个基于 **C++11 + Qt 5 Widgets** 的串口 / 网口调试工具。支持串口、TCP 客户端、TCP 服务端、UDP，多设备会话可同时连接、并行收发，并以时间戳 + 方向颜色区分收/发报文。

> 本项目按要求采用分层架构，且**没有链接或使用 Qt 自带的 Modbus 模块**（`Qt5::SerialBus` / `QModbusClient` / `QModbusServer` 均未使用）。协议分帧在应用自己的 `FrameCodec` 中实现，后续可以在此基础上手写 Modbus RTU/TCP 支持。

## 功能

- **传输方式**
  - 串口：端口扫描、波特率、数据位、校验位、停止位、流控。
  - TCP 客户端：目标地址/端口、连接超时、异步连接、断线状态提示。
  - TCP 服务端：监听地址/端口，一个会话可同时接入多个客户端；发送广播到所有客户端，接收按客户端独立分帧并标注来源。
  - UDP：本机端口、目标地址/端口；接收任意来源并标注发送方；支持发送到 `255.255.255.255` 广播地址。
- **多设备并发**：每个连接是独立的 `Session`，拥有独立的传输、分帧状态、统计和日志；可同时打开任意多个串口/TCP/UDP 会话。
- **会话筛选与多选发送**：会话列表可按串口 / TCP / UDP 类型过滤；每个会话前有勾选框，发送时面向全部勾选设备。
- **明暗主题**：内置深色 / 亮色两套主题，右上角“亮色/暗色”按钮即时切换并记住选择。
- **报文日志**
  - 完整时间戳（`yyyy-MM-dd HH:mm:ss.zzz`）。
  - 接收绿色 `RX`、发送蓝色 `TX`、系统灰色 `SYS`、错误红色 `ERR`。
  - 文本 / HEX 显示切换、按方向筛选、关键字搜索、清空、保存到 UTF-8 日志文件。
  - 会话切换时保留各自历史；单会话最多保留 5000 条。
  - 为保持高消息量下流畅，界面最多渲染最近 1000 条；完整历史仍保留，保存日志会全部导出。
- **发送**
  - 文本或 HEX 输入，发送结尾可选无 / CRLF / LF / CR / 自定义 HEX。
  - `Ctrl+Enter` 快捷发送。
  - 定时发送（50 ms 起）。
  - 在设备前勾选多个会话后，“发送到勾选”会把同一报文发送到所有勾选的在线设备。
- **其它**
  - 每会话 RX/TX 字节、帧数、实时速率、对端数量。
  - 接收分帧：原始数据块 / 换行符 / 空闲间隔（适合 Modbus RTU 等无分隔符协议）。
  - 会话重命名、复制配置、删除、全部连接/断开。
  - 深色界面、日志自动跟随（用户上翻阅读时不会强制滚到底部）。

## 架构

```
                 main.cpp  (composition root)
                     │ 注入 TransportFactory / SerialPortLister
        ┌────────────┴─────────────┐
        ▼                          ▼
   ui (Qt Widgets)            infra (Qt Network / SerialPort)
        │                          │
        ▼                          ▼
   app (Session / SessionManager) ──► core (ITransport / FrameCodec / Types)
```

| 层 | 目录 | 职责 | 依赖 |
|---|---|---|---|
| core | `src/core` | 值类型、日志条目、分帧器、字节/HEX 工具、传输接口 | QtCore |
| app | `src/app` | `Session` 聚合、`SessionManager` 多会话编排、统计、日志历史 | core |
| infra | `src/infra` | 串口、TCP 客户端/服务端、UDP 的具体实现 | core + QtNetwork + QtSerialPort |
| ui | `src/ui` | 主窗口、会话列表、日志视图、发送面板、新建连接对话框 | app + core + QtWidgets |
| 组合根 | `src/main.cpp` | 创建工厂并注入，装配 UI/服务 | 全部 |

关键约束：

- UI 不包含任何 `QTcpSocket` / `QSerialPort` / `QUdpSocket` 头文件，也不直接操作传输层。
- `app` 不包含 `QtNetwork` / `QtSerialPort` 头文件；传输对象通过 `SessionManager` 构造函数注入的工厂创建。
- 具体传输实现只实现 `ITransport` 接口，业务层不感知连接类型。
- 所有 I/O 在 GUI 线程中异步进行，不使用阻塞读；并发来自事件驱动 + 每会话/每对端独立状态，而不是为每个连接开线程。

详细设计见 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)。

## 构建

依赖：

- CMake ≥ 3.16
- C++11 编译器
- Qt 5.12+，模块：`Core`、`Gui`、`Widgets`、`Network`、`SerialPort`

Windows + MinGW 示例（按本机 Qt 安装路径调整）：

```powershell
cmake -S . -B build -G "MinGW Makefiles" `
  -DCMAKE_PREFIX_PATH="D:/install/Qt/5.14.2/mingw73_64" `
  -DCMAKE_BUILD_TYPE=Release

cmake --build build -j
```

运行：

```powershell
.\build\DouLink.exe
```

部署 Qt 运行库：

```powershell
& "D:/install/Qt/5.14.2/mingw73_64/bin/windeployqt.exe" --release .\build\DouLink.exe
```

> CMakeLists 已处理 CMake 4.x 读取旧版 Qt 5 CMake 配置时的 policy 兼容问题。
>
> Windows 可执行文件图标由 `resources/app.rc.in` 在链接时嵌入；`QApplication::setWindowIcon()` 只设置运行中的窗口图标。若资源管理器仍显示旧图标，可刷新图标缓存或重新创建快捷方式。
>
> 界面布局位于 `src/ui/mainwindow.ui`、`sessiondialog.ui`、`sendpanel.ui`，由 CMake 的 `AUTOUIC` 生成 `ui_*.h`；修改 `.ui` 后重新执行 CMake 配置/构建即可。
>
> 如果使用 MSVC，请选择与 Qt 套件 ABI 匹配的编译器，并把 `CMAKE_PREFIX_PATH` 指向对应的 `msvc20xx_64` 目录。

### VS Code 调试

仓库提供了 [.vscode/launch.json](.vscode/launch.json)，其中把 Qt 的 `bin` 和 `plugins` 目录加入了调试进程环境。请确认：

- `program` 指向你实际的构建产物，例如 `build/Debug/DouLink.exe`；
- Qt 路径与实际安装一致（示例为 `D:/install/Qt/5.14.2/msvc2017_64`）。

在 VS Code 的“运行和调试”中选择 **DouLink (Debug + Qt runtime)** 启动。若使用 CMake Tools 自动生成的调试配置，请在配置中同样设置：

```text
PATH=D:/install/Qt/5.14.2/msvc2017_64/bin;%PATH%
QT_PLUGIN_PATH=D:/install/Qt/5.14.2/msvc2017_64/plugins
QT_QPA_PLATFORM_PLUGIN_PATH=D:/install/Qt/5.14.2/msvc2017_64/plugins/platforms
```

或者先手动部署运行库，之后直接启动 exe 即可：

```powershell
& "D:/install/Qt/5.14.2/msvc2017_64/bin/windeployqt.exe" --debug .\build\Debug\DouLink.exe
```

## 使用

1. 点击右上角“新建连接”或空白页的串口/TCP/UDP 卡片。
2. 选择连接类型并填写参数：
   - 串口：选择端口（可点“刷新”重新扫描）和串口参数。
   - TCP 客户端：填目标 IP/主机名与端口。
   - TCP 服务端：填监听地址与端口；启动后可用其它客户端连接，本工具会为每个客户端标注来源并广播发送。
   - UDP：本机端口设 0 表示自动分配；目标地址留空时仅接收；目标填 `255.255.255.255` 可广播。
3. 在右侧“接收分帧”中选择切分方式：
   - 原始数据：每个接收块一条日志；
   - 换行符分帧：按 CRLF/LF/CR/自定义 HEX 分隔；
   - 空闲间隔：总线静默超过设定毫秒数后结束一帧（适合 Modbus RTU 等）。
4. 连接建立后，日志区显示带时间戳和方向颜色的报文；可在工具栏切换 HEX、搜索、筛选或保存。
5. 在左侧勾选一个或多个设备，底部输入内容、选择文本/HEX 和结尾符后点击“发送到勾选”；勾选“定时发送”会按周期向当前勾选的在线设备发送。
6. 会话列表上方可按类型过滤；右键会话可重命名、复制或删除。
7. 右上角“亮色/暗色”按钮可切换主题，选择会写入用户设置并在下次启动时恢复。

### 本机快速自测（无需硬件）

1. 新建一个 **TCP 服务端** 会话，监听 `0.0.0.0:9000`（或其它空闲端口）。
2. 再新建一个 **TCP 客户端** 会话，目标 `127.0.0.1:9000`。
3. 服务端会话的日志会出现“客户端接入”，并显示客户端地址。
4. 只勾选客户端会话，在发送区输入内容发送：客户端日志显示蓝色 `TX`，服务端日志显示绿色 `RX` 并标注来源。
5. 只勾选服务端会话发送：会广播给已接入客户端，客户端日志显示 `RX`。
6. 同时再建一个 UDP 会话，即可观察多个连接会话并行收发时各自的日志、统计和状态。

## 目录

```
DouLink/
├─ CMakeLists.txt
├─ README.md
├─ docs/ARCHITECTURE.md
├─ resources/
│  ├─ app.ico
│  ├─ app.png
│  ├─ app.rc.in
│  ├─ resources.qrc
│  ├─ theme_dark.qss
│  └─ theme_light.qss
├─ src/
│  ├─ main.cpp
│  ├─ core/        Types、ByteCodec、FrameCodec、ITransport
│  ├─ app/         Session、SessionManager
│  ├─ infra/       SerialTransport、TcpClientTransport、
│  │               TcpServerTransport、UdpTransport、TransportFactory
│  └─ ui/          MainWindow、LogView、SendPanel、SessionDialog、
│                  SessionListDelegate、BrandMark、Theme
│                  mainwindow.ui / sessiondialog.ui / sendpanel.ui
```

## 说明与边界

- 当前实现聚焦需求中的串口/TCP/UDP 调试基础功能，没有内置 Modbus 报文编辑器；但分帧与协议扩展点独立于 Qt Modbus，可在 `FrameCodec` 中增加 Modbus RTU CRC/空闲分帧、在应用层增加 MBAP/PDU 解析。
- UDP 主机名在打开连接时解析一次；常用 IP/广播地址无需 DNS。
- TCP 服务端采用“一个监听会话 + 多客户端子流”的模型，日志会显示客户端地址；所有客户端共享同一条发送内容。
- 日志为内存环形缓冲（5000 条/会话），界面只渲染最近 1000 条以保持流畅；“保存”会导出完整历史。
