# DouLink 架构说明

## 1. 目标

DouLink 的核心目标不是“把 socket 包一层”，而是用清晰的分层把**连接管理、传输实现、应用协议状态、展示**拆开，从而支持：

- 串口 / TCP 客户端 / TCP 服务端 / UDP 四种链路；
- 任意多个设备会话同时存在、同时收发；
- 每个会话独立的日志、统计、分帧状态；
- 不依赖 Qt Modbus 模块，协议层可自行实现。

## 2. 分层与依赖规则

```
┌──────────────────────────────────────────────────────────────┐
│ main.cpp（组合根）                                            │
│  - 创建 TransportFactory / SerialPortLister                   │
│  - 注入 SessionManager，创建 MainWindow                       │
└───────────────┬──────────────────────────────┬───────────────┘
                │                              │
                ▼                              ▼
┌──────────────────────────────┐   ┌───────────────────────────┐
│ ui（Qt Widgets）              │   │ infra（QtNetwork/Serial） │
│ MainWindow / LogView /       │   │ SerialTransport            │
│ SendPanel / SessionDialog    │   │ TcpClientTransport         │
│ SessionListDelegate          │   │ TcpServerTransport         │
└───────────────┬──────────────┘   │ UdpTransport               │
                │                  └─────────────┬─────────────┘
                ▼                                │
┌──────────────────────────────┐                 │
│ app（应用服务）                │                 │
│ Session / SessionManager     │                 │
└───────────────┬──────────────┘                 │
                │                                │
                ▼                                ▼
┌──────────────────────────────────────────────────────────────┐
│ core（领域层）                                                │
│ Types / LogEntry / FrameCodec / ByteCodec / ITransport       │
└──────────────────────────────────────────────────────────────┘
```

依赖方向约束：

1. `core` 只依赖 QtCore，不知道 TCP、串口、Widget。
2. `app` 只依赖 `core`；它通过构造函数注入的 `std::function<std::unique_ptr<ITransport>(TransportKind)>` 取得传输对象，因此不包含任何 `QtNetwork` / `QtSerialPort` 头文件。
3. `infra` 实现 `ITransport`，是唯一接触 `QSerialPort`、`QTcpSocket`、`QUdpSocket` 的层。
4. `ui` 只依赖 `app` 与 `core` 的类型；不直接包含传输实现。新建连接对话框需要串口列表时，通过 `SessionManager::serialPorts()`（内部是注入的 `SerialPortLister`）获得纯数据。
5. `main.cpp` 是唯一的组合根，知道所有具体类型。

界面布局使用 Qt Designer 的 `.ui` 文件（`mainwindow.ui`、`sessiondialog.ui`、`sendpanel.ui`），由 CMake `AUTOUIC` 生成 `ui_*.h`。`MainWindow` / `SessionDialog` / `SendPanel` 中只保留信号连接、状态刷新、配置校验等行为代码，不再用代码拼装布局；`LogView`、`BrandMark`、`SessionListDelegate` 属于自绘控件，仍以 C++ 实现。

这种约束可以通过检查 `src/app`、`src/ui` 的 include 直接验证。

## 3. 核心类型

### `EndpointConfig`
连接参数值对象，包含传输类型、串口参数、网络地址、分帧配置。UI 对话框产出该对象，`SessionManager` 消费它。

### `ITransport`
纯传输接口（QObject）：

```cpp
virtual void open(const EndpointConfig&) = 0;
virtual void close() = 0;
virtual bool isOpen() const = 0;
virtual qint64 write(const QByteArray&) = 0;
virtual QString lastError() const = 0;
virtual QString describe() const = 0;

signals:
    void opened();
    void closed();
    void errorOccurred(const QString&);
    void dataReceived(const QByteArray&, const QString& peer);
    void peerChanged(const QString& peer, bool connected);
```

`peer` 对点对点链路为空；TCP 服务端和 UDP 用 `address:port` 标识来源。

### `FrameCodec`
无 Qt 网络依赖的增量分帧器：

- `Raw`：每个接收块直接作为一帧。
- `Line`：按分隔符切分（保留分隔符），处理跨 chunk 的半包。
- `IdleGap`：只缓存，由 `Session` 的空闲定时器在总线静默后调用 `flush()`，适合 Modbus RTU 这类靠 3.5 字符间隔断帧的协议。

每个 peer 一个 `FrameCodec`，因此 TCP 服务端混合多个客户端时不会串帧。

### `Session`
应用层聚合根，一个实例对应左侧列表中的一个设备连接：

- 拥有一个 `ITransport`；
- 维护 `QHash<peer, FrameCodec>` 与空闲定时器；
- 将接收块转成带时间戳的 `LogEntry`；
- 维护 RX/TX 字节、帧数、速率、对端数；
- 维护状态机：`Disconnected → Connecting → Connected/Listening → Error/Disconnected`；
- 对外只暴露 `open/close/send/...` 和信号，UI 不触碰传输。

### `SessionManager`
- 通过注入的工厂创建传输；
- 管理会话生命周期、唯一命名；
- 提供 `broadcast()`、`openAll()`、`closeAll()`、`onlineCount()`；
- 不实现具体协议，也不依赖网络类型。

## 4. 接收数据流

```
QTcpSocket / QSerialPort / QUdpSocket readyRead
        │
        ▼
infra::XxxTransport::dataReceived(bytes, peer)
        │  (Qt signal, 同线程)
        ▼
Session::onDataReceived
        ├─ stats.rxBytes += bytes.size()
        ├─ 为该 peer 取得/创建 FrameCodec
        ├─ frames = codec.feed(bytes)
        ├─ 对每个完整帧：stats.rxFrames++ → appendLog(Rx, ...)
        └─ IdleGap 模式：重启该 peer 的空闲定时器
        │
        ▼
Session::logAppended(session, LogEntry)
        │
        ▼
MainWindow::onSessionLogAppended
        └─ 仅当该会话是当前选中项时，LogView::appendEntry()
                ├─ 按文本/HEX 与筛选条件渲染
                ├─ 时间戳 + 方向着色
                └─ 若用户停留在底部则自动跟随
```

时间戳在 `Session` 收到完整帧时生成，而不是在 UI 渲染时生成，保证日志与真实接收顺序一致。

## 5. 发送数据流

```
SendPanel（文本/HEX 解析 + 结尾符）
        │ sendRequested(payload, suffix)
        ▼
MainWindow（勾选设备集合）
        ▼
Session::send(payload, suffix)
        ├─ 检查在线状态
        ├─ frame = payload + suffix
        ├─ transport->write(frame)
        ├─ stats.txBytes += written; stats.txFrames++
        └─ appendLog(Tx, frame, txPeerLabel())
```

会话列表中的勾选框组成目标集合；发送时 `MainWindow` 收集勾选的 `SessionId`，交给 `SessionManager::sendTo()` 只向这些会话执行 `send()`。因此每个目标的 TX 日志、统计仍相互独立；TCP 服务端会话内部再向所有已接入客户端广播。

## 6. 并发模型

- **单 GUI 线程 + 异步 I/O**：串口、TCP、UDP 都是 Qt 事件驱动，不阻塞 GUI；无需为每个连接创建线程。
- **会话隔离**：每个 `Session` 有独立的 codec、timer、统计、日志。一个会话断开或报错不会影响其它会话。
- **多客户端隔离**：TCP 服务端以 socket 为键维护 peer；接收数据按 peer 进入各自的 codec，日志前缀显示客户端地址；发送广播到所有 socket。
- **UDP 来源隔离**：每个发送方地址/端口一个 codec，日志显示来源；发送目标由配置决定。
- **空闲定时器**：仅 `IdleGap` 模式创建，单次触发；收到新数据即重启；peer 断开时停止并释放。
- **日志上限**：每会话日志最多 5000 条，避免长时间调试导致内存无限增长；UI 只把最近 1000 条渲染进 `QPlainTextEdit`（`setMaximumBlockCount`），切换会话/文本-HEX/时间戳时不重排全部历史，导出仍覆盖完整 5000 条。

## 7. 状态与错误处理

- 连接失败、超时、串口拔出等进入 `Error` 状态，保留错误日志；按钮变为“重新连接”。
- `Error` 状态收到底层 `closed()` 时不会立刻变成“未连接”，以便用户看到异常原因；显式关闭或重新打开会清理。
- 发送失败不会把整个会话置为错误；只在日志中追加 `ERR` 条目并保留现有连接。
- TCP 服务端单个客户端错误不改变整个监听会话状态，只记录该 peer 的接入/断开。

## 8. 与 Qt Modbus 的关系

项目 **没有** 链接 `Qt5::SerialBus`，也没有使用 `QModbusClient`、`QModbusServer`、`QModbusRtuSerialMaster` 等类。

如果后续需要 Modbus：

1. **Modbus RTU**
   - 选 `FrameMode::IdleGap`，设置接近 3.5 字符时间的空闲间隔（例如 9600 波特率约 4 ms，调试工具通常取 10–30 ms）；
   - 在 `FrameCodec` 增加 `Crc16` 校验与 `appendCrc` 发送选项；
   - 在 `Session` 的完整帧回调中解析功能码/寄存器，或新增一个协议层 `IProtocolCodec`，让 `Session` 只负责字节流。
2. **Modbus TCP**
   - 在 `FrameCodec` 增加 `Mbap` 模式：解析 6 字节 MBAP 头中的长度字段，按长度组帧；
   - 事务 ID、单元 ID 可在日志的 note 字段中展示。
3. 无论哪种方式，CRC16、MBAP、功能码都手写在 core/app 层，不依赖 Qt Modbus。

## 9. 可扩展点

- 新传输：实现 `ITransport`，在 `infra::createTransport()` 注册，UI 的类型枚举与对话框增加一项即可。
- 新分帧：扩展 `FrameMode` 与 `FrameCodec`。
- 新协议：建议引入 `IProtocolCodec`，插在 `Session` 的帧回调与日志之间，保持传输/会话层不变。
- 日志持久化：当前导出为 UTF-8 文本；可换为 CSV/JSON，或在 `Session::appendLog` 旁增加落盘 sink。
