# LiJointMaster3 项目文档

## 1. 项目概述

`LiJointMaster3` 是一个基于 `Qt Widgets` 开发的 FOC 电机上位机软件，用于通过串口与电机控制板通信，实现参数配置、控制模式切换、实时波形监控、零电位校准、PID 参数整定以及主题外观设置。

项目的定位不是通用串口工具，而是一个面向 FOC 调试和控制流程的专用上位机。界面和协议都围绕电机调试场景设计。

## 2. 项目目标

本项目主要解决以下问题：

- 为 FOC 控制板提供可视化参数配置界面。
- 通过固定协议串口帧与 MCU 双向通信。
- 实时查看机械角度、电流、电压、ADC、速度、位置等运行数据。
- 在上位机完成控制模式切换、目标值写入和 PID 参数调试。
- 提供统一、风格化的调试界面，降低调试门槛。

## 3. 技术栈

- 语言：`C++17`
- UI 框架：`Qt 5/6 Widgets`
- 串口模块：`Qt SerialPort`
- 图表库：`QCustomPlot`
- 构建系统：`CMake`

## 4. 当前目录结构

```text
LiJointMaster3/
├─ src/
│  ├─ app/                 # 程序入口
│  ├─ ui/                  # 主界面与交互逻辑
│  ├─ plot/                # 曲线图管理
│  ├─ serial/              # 串口通信与协议解析
│  └─ third_party/
│     └─ qcustomplot/      # 第三方图表库
├─ docs/
│  ├─ debug-archive/       # 历史调试输出归档
│  └─ PROJECT_DOCUMENTATION.md
├─ build/                  # 构建输出目录
└─ CMakeLists.txt
```

## 5. 功能总览

项目当前已经实现的核心功能包括：

- 无边框主窗口与自定义标题栏
- 顶部模式切换与设置弹窗
- 串口端口扫描、热插拔检测、连接/断开
- 电机连接状态管理
- 电机基础参数配置
- 零电位校准
- 控制模式切换
- 实时波形订阅与取消订阅
- 图表实时刷新、拖拽、缩放、悬停读数
- 目标值下发
- PID 参数整定
- 主题与强调色切换
- 开发者留言页面

## 6. 程序启动流程

程序入口位于 `src/app/main.cpp`：

1. 创建 `QApplication`
2. 实例化主窗口 `Widget`
3. 显示主窗口
4. 启动事件循环

主窗口在构造时会：

1. 创建 `SerialManager`
2. 设置为无边框窗口
3. 构建整套 UI
4. 绑定串口与界面信号
5. 刷新串口列表

## 7. 主界面功能说明

### 7.1 自定义标题栏

主窗口使用 `Qt::FramelessWindowHint` 实现无边框外观，并在顶部标题栏区域补充了拖动逻辑。

标题栏包含：

- 项目标识条
- 主标题 `LiJointMaster`
- 风格标签 `cartoon`
- 模式切换下拉框
- 设置按钮
- 关闭按钮

支持：

- 鼠标拖动窗口
- 点击关闭窗口
- 点击进入设置对话框

### 7.2 模式切换

顶部模式下拉框目前包含：

- `有感`
- `无感`

当前状态说明：

- `有感` 页面功能完整，是目前主要工作页。
- `无感` 页面目前为预留区域，仅显示占位提示，尚未实现完整功能。

### 7.3 左侧串口配置区

左侧第一个功能卡片为“串口配置”，包括：

- 串口号选择
- 波特率选择
- 数据位选择
- 停止位选择
- 校验位选择
- 打开串口/关闭串口按钮
- 串口日志按钮
- 串口状态文本
- 串口状态指示灯

当前默认下拉值中提供了：

- 串口：`COM6`、`COM5`
- 波特率：`4000000`、`2000000`、`921600`、`115200`
- 数据位：`8`、`7`
- 停止位：`1`、`1.5`、`2`
- 校验位：`None`、`Even`、`Odd`

实际串口连接逻辑中，程序固定按 `8N1`、无流控配置打开串口。也就是说，界面上虽然有参数项，但当前实现真正使用的是：

- 数据位：`8`
- 校验位：`None`
- 停止位：`1`
- 流控：`None`

因此，这些下拉框目前主要起到界面展示作用，并未完全接入到底层配置。

### 7.4 串口热插拔与状态维护

程序通过定时器每 `300ms` 扫描系统串口列表，实现：

- 串口列表变化自动检测
- 串口拔出后自动断连
- 状态文字更新
- 串口下拉列表自动刷新

当已连接串口被移除时，会自动：

- 关闭串口
- 清空接收缓存
- 重置电机连接状态
- 清零 MOS 温度
- 清除当前活动波形命令
- 更新状态栏提示

### 7.5 串口日志

界面中保留了“串口日志”按钮与日志窗口，但当前日志输出被主动禁用。

原因：

- 高频串口日志会导致界面 `append` 过于频繁
- 影响图表刷新与整体流畅度

当前行为：

- 点击“串口日志”会弹出日志窗口
- 日志窗口中显示“为避免高频日志导致界面卡顿，串口日志输出已关闭”

也就是说，这个功能在 UI 上保留，但实际不承担实时日志查看职责。

## 8. 电机配置区功能

左侧第二个功能卡片为“电机配置”，主要负责基础参数下发与状态显示。

包含：

- 电机连接按钮
- 电机连接状态灯
- 设置极对数
- 设置角度方向
- 设置速度方向
- 设置电机相电阻 Rs
- 设置电机 q 轴电感 Lq
- 设置电机 d 轴电感 Ld
- MOS 温度显示
- MOS 温度滑条

### 8.1 电机连接

点击“连接”后，会先检查串口是否已连接：

- 若串口未连接，会弹出提示框“先打开串口，再连接电机呀~”
- 若串口已连接，则发送 `CMD_CONNECT_MOTOR`

连接成功后：

- 电机状态灯变绿
- 状态栏显示“电机连接成功”
- 回包中的参数会自动回填到对应输入框

### 8.2 基础参数设置

当前支持下发：

- 极对数：`CMD_SETPAIRS`
- 角度方向：`CMD_SETDIR`
- 速度方向：`CMD_SETSPEEDDIR`
- 电机相电阻 Rs：`CMD_SETMOTORRS`
- 电机 q 轴电感 Lq：`CMD_SETMOTORLQ`
- 电机 d 轴电感 Ld：`CMD_SETMOTORLD`

每一项都由“按钮 + 输入框”组成：

- 输入框用于填写数值
- 按钮点击后将数值转成 `float` 并通过串口发送

Rs/Lq/Ld 使用 SI 基本单位：

- Rs 单位为 R/Ω，默认值 `0.198`
- Lq 单位为 H，默认值 `0.000074`，即 `74uH`
- Ld 单位为 H，默认值 `0.000040`，即 `40uH`

AT32 固件收到这三个命令后会写入 flash，并立即更新后台 SMO 观测器使用的电机参数。其中 SMO 当前使用的等效电感为 `(Lq + Ld) / 2`。

### 8.3 MOS 温度显示

当串口收到 `CMD_MOSTEMP` 回包时：

- 更新文本显示，如 `35.4 ℃`
- 更新温度滑条位置

这部分是只读显示，不会向下位机写入。

### 8.4 母线 ADC

电机配置区不再保留母线电压输入框。

波形监控区域的“母线ADC”按钮用于订阅下位机回传的母线电压值。该功能不写入参数，只控制曲线数据流开关。

## 9. 图表区功能

右侧上半部分为实时图表区，基于 `QCustomPlot` 实现。

### 9.1 图表基础能力

图表支持：

- 实时追加数据
- 水平方向自动滚动
- 鼠标拖拽平移
- 鼠标滚轮缩放
- 鼠标悬停最近点读数提示
- 横轴范围调整

### 9.2 图表样式

图表使用深色背景，并定制了：

- 坐标轴颜色
- 网格颜色
- 边框和圆角容器

当前图表标签状态：

- X 轴显示 `Time (s)`
- Y 轴标签已移除

### 9.3 数据刷新机制

`PlotManager` 内部维护：

- 图表对象指针
- 各曲线映射表
- 当前时间键值 `m_key`
- 当前横轴范围 `m_xAxisRange`
- 重绘定时器

刷新机制：

- 每次追加数据时，`m_key += 0.0005`
- 新数据写入对应曲线
- X 轴自动右对齐滚动
- 使用 `30ms` 定时器定期 `replot`

### 9.4 鼠标悬停提示

鼠标移动时，程序会：

1. 在每条曲线中寻找最接近当前鼠标横坐标的数据点
2. 计算像素距离
3. 若距离小于阈值，则显示悬浮文本

当前提示内容：

- `Y: xx.xx`

提示位置会跟随当前点附近变化。

### 9.5 横轴范围调节

图表下方有“横轴范围”滑条。

范围说明文本为：

- `0.1s - 10s`

滑条变化后，会将数值映射到实际秒数范围，并调用 `PlotManager::setXAxisRange` 更新显示窗口。

## 10. 波形监控功能

图表下方中部区域有一组波形订阅按钮，用于请求下位机持续上传某类数据。

当前支持的波形种类包括：

- 机械角度
- 三相 ADC
- IAlpha_Beta
- 三相电压
- 三相电流
- IQ_ID
- 三相 SVPWM
- UAlpha_Beta
- 速度
- 位置
- 电流环输出
- 速度环输出
- 位置环输出
- 母线 ADC 原始值

### 10.1 单选订阅机制

这一组按钮是“单选订阅”逻辑：

- 打开一个波形时，会自动关闭其他已打开波形
- 再次点击同一按钮可关闭该波形

实现上，程序维护：

- 打开命令映射表
- 关闭命令映射表

### 10.2 波形命令映射

对应命令包括：

- `CMD_MECHANICALANGLE / CMD_MECHANICALANGLE_CLOSE`
- `CMD_ADC / CMD_ADC_CLOSE`
- `CMD_IALPHA_BETA / CMD_IALPHA_BETA_CLOSE`
- `CMD_UABC / CMD_UABC_CLOSE`
- `CMD_IABC / CMD_IABC_CLOSE`
- `CMD_IQ_ID / CMD_IQ_ID_CLOSE`
- `CMD_TABC / CMD_TABC_CLOSE`
- `CMD_UALPHA_BETA / CMD_UALPHA_BETA_CLOSE`
- `CMD_SPEED / CMD_SPEED_CLOSE`
- `CMD_LOCAL / CMD_LOCAL_CLOSE`
- `CMD_SPEEDOUT / CMD_SPEEDOUT_CLOSE`
- `CMD_LOCALOUT / CMD_LOCALOUT_CLOSE`
- `CMD_ADCVBUS / CMD_ADCVBUS_CLOSE`

### 10.3 图表数据追加规则

当收到下位机回包后，程序会根据命令类型，把数据写入对应图表曲线：

- 机械角度 -> `mechanicalAngle`
- 三相电压 -> `Ua` `Ub` `Uc`
- 三相 ADC -> `ADC1` `ADC2` `ADC3`
- 三相 SVPWM -> `Ta` `Tb` `Tc`
- 三相电流 -> `Ia` `Ib` `Ic`
- UAlpha_Beta -> `Ualpha` `Ubeta`
- IAlpha_Beta -> `Ialpha` `Ibeta`
- IQ_ID -> `Iq` `Id`
- 速度 -> `speed`
- 速度环输出 -> `speedOut`
- 位置 -> `local`
- 位置环输出 -> `localOut`
- 母线 ADC 原始值 -> `adcvbus`

`CMD_MOSTEMP` 仍用于 MOS 温度显示；`CMD_ADCVBUS` 独立用于母线电压 ADC 原始值曲线，避免两类数据复用同一个命令字。

## 11. 零电位校准功能

右下区域左侧有“零电位校准”功能。

包含：

- 零电位校准按钮
- 零偏值输入框
- 电角度输入框

### 11.1 启动与结束

点击按钮时：

- 如果按钮切换为选中，发送 `CMD_ZEROCALIBRATIO`
- 如果按钮切换为未选中，发送 `CMD_ZEROCALIBRATIO_OVER`

### 11.2 回包处理

当收到 `CMD_ZEROCALIBRATIO_OVER` 回包时：

- 状态栏更新为“零电位校准完成”
- 回填零偏值
- 回填电角度
- 自动取消按钮选中状态

## 12. 控制模式切换

控制模式下拉框位于右下区域中部。

当前支持：

- 开环模式
- 电流环
- 速度环
- 位置环

切换后对应发送：

- `CMD_OPEN_LOOP`
- `CMD_CURRENT_LOOP`
- `CMD_SPEED_LOOP`
- `CMD_POSITION_LOOP`

发送时同时附带当前模式索引值。

## 13. 目标值设置功能

右下区域右侧有“目标值设置”模块，支持用户向控制器下发控制目标。

当前支持：

- 设置 Uq
- 设置 Ud
- 设置 Iq
- 设置 Id
- 设置速度
- 设置位置

对应命令：

- `CMD_SETUQ`
- `CMD_SETUD`
- `CMD_SETIQ`
- `CMD_SETID`
- `CMD_SETSPEEDTAR`
- `CMD_SETLOCALTAR`

使用方式：

- 在输入框填写目标值
- 点击对应按钮发送

## 14. PID 参数整定功能

右下区域底部有 PID 参数整定模块，分为三组：

- 电流环 PID 参数整定
- 速度环 PID 参数整定
- 位置环 PID 参数整定

### 14.1 电流环 PID

支持：

- 设置 KP -> `CMD_SETIQPIDKP`
- 设置 KI -> `CMD_SETIQPIDKI`
- 输出限制 -> `CMD_SETIQPIDOUT`

### 14.2 速度环 PID

支持：

- 设置 KP -> `CMD_SETSPEEDPIDKP`
- 设置 KI -> `CMD_SETSPEEDPIDKI`
- 输出限制 -> `CMD_SETSPEEDPIDOUT`

### 14.3 位置环 PID

支持：

- 设置 KP -> `CMD_SETLOCALPIDKP`
- 设置 KD -> `CMD_SETLOCALPIDKD`
- 输出限制 -> `CMD_SETLOCALPIDOUT`

### 14.4 参数回填

当收到 `CMD_CONNECT_MOTOR` 回包且数据长度足够时，程序会将下位机当前参数回填到界面，包括：

- 极对数
- 角度方向
- 零偏值
- 电流环 PID
- 速度方向
- 电机参数 Rs/Lq/Ld
- 速度环 PID
- 位置环 PID
- 各环输出限制

这意味着“连接电机”不仅是连接动作，也承担了参数同步入口。

当前 `CMD_CONNECT_MOTOR` 回包中，Rs/Lq/Ld 位于扩展数据区：

- `values[14]`：Rs，单位 R/Ω
- `values[15]`：Lq，单位 H
- `values[16]`：Ld，单位 H

为了兼容旧固件或旧回包，界面仍允许只收到前 14 个参数；只有当回包长度达到 17 个 `float` 时才回填 Rs/Lq/Ld。

## 15. 设置对话框功能

点击顶部“设置”按钮会打开系统设置对话框。

### 15.1 对话框组成

设置对话框包含两个主要入口：

- 主题与颜色
- 开发者留言

### 15.2 主题与颜色

主题页支持：

- 主题风格切换
- 强调色切换

当前主题风格包括：

- 蓝色
- 浅色
- 纯黑
- 绿色
- 暖色
- 紫色

当前强调色包括：

- 蓝色
- 绿色
- 橙色
- 红色

修改后会：

- 即时刷新设置对话框自身样式
- 触发主界面重建
- 保持整体风格同步更新

### 15.3 开发者留言

开发者留言页展示：

- 作者信息
- QQ 联系方式
- 版本
- 日期
- 主要功能简介
- Git 仓库地址

这是一个展示型信息页，不参与业务逻辑。

## 16. 串口通信协议

项目实现了发送帧与接收帧两套协议。

### 16.1 基本帧标记

- 帧头：`0xA5`
- 帧尾：`0x49`

### 16.2 下发命令帧格式

发送时固定使用 8 字节帧：

```text
[HEAD][CMD][DATA0][DATA1][DATA2][DATA3][CHECKSUM][TAIL]
```

说明：

- `HEAD`：帧头 `0xA5`
- `CMD`：命令字
- `DATA0~DATA3`：一个 `float` 的 4 字节数据
- `CHECKSUM`：前 6 字节累加和
- `TAIL`：帧尾 `0x49`

### 16.3 接收回包帧格式

接收时使用变长帧：

```text
[HEAD][CMD][LEN][PAYLOAD...][CHECKSUM][TAIL]
```

说明：

- `LEN` 必须大于 0
- `LEN` 必须为 4 的倍数
- `PAYLOAD` 解释为 `float` 数组
- `CHECKSUM` 为前面所有字节的累加和

### 16.4 解析保护机制

为防止异常数据导致程序失控，接收逻辑加入了：

- 最小长度检查
- 帧头同步
- 最大载荷长度限制 `128`
- `LEN` 对齐检查
- 帧尾检查
- 校验和检查
- 校验失败按字节滑动重新同步

## 17. 命令字清单

项目中定义的命令字如下。

### 17.1 连接与基础配置

- `CMD_CONNECT_MOTOR`
- `CMD_MECHANICALANGLE`
- `CMD_MECHANICALANGLE_CLOSE`
- `CMD_SETPAIRS`
- `CMD_SETDIR`
- `CMD_ZEROCALIBRATIO`
- `CMD_ZEROCALIBRATIO_OVER`
- `CMD_SETMOTORRS`
- `CMD_SETMOTORLQ`
- `CMD_SETMOTORLD`

### 17.2 电压、电流、采样相关

- `CMD_UABC`
- `CMD_UABC_CLOSE`
- `CMD_SETUQ`
- `CMD_ADC`
- `CMD_ADC_CLOSE`
- `CMD_DCVBUS`
- `CMD_TABC`
- `CMD_TABC_CLOSE`
- `CMD_IABC`
- `CMD_IABC_CLOSE`
- `CMD_UALPHA_BETA`
- `CMD_UALPHA_BETA_CLOSE`
- `CMD_IALPHA_BETA`
- `CMD_IALPHA_BETA_CLOSE`
- `CMD_IQ_ID`
- `CMD_IQ_ID_CLOSE`
- `CMD_SETIQ`
- `CMD_SETID`
- `CMD_MOSTEMP`
- `CMD_ADCVBUS`
- `CMD_ADCVBUS_CLOSE`
- `CMD_SETUD`

### 17.3 控制模式与速度位置控制

- `CMD_OPEN_LOOP`
- `CMD_CURRENT_LOOP`
- `CMD_SPEED_LOOP`
- `CMD_POSITION_LOOP`
- `CMD_SPEED`
- `CMD_SPEED_CLOSE`
- `CMD_SETSPEEDDIR`
- `CMD_SPEEDOUT`
- `CMD_SPEEDOUT_CLOSE`
- `CMD_SETSPEEDTAR`
- `CMD_SETLOCALTAR`
- `CMD_LOCAL`
- `CMD_LOCAL_CLOSE`
- `CMD_LOCALOUT`
- `CMD_LOCALOUT_CLOSE`

### 17.4 PID 参数相关

- `CMD_SETIQPIDKP`
- `CMD_SETIQPIDKI`
- `CMD_SETIQPIDOUT`
- `CMD_SETSPEEDPIDKP`
- `CMD_SETSPEEDPIDKI`
- `CMD_SETSPEEDPIDOUT`
- `CMD_SETLOCALPIDKP`
- `CMD_SETLOCALPIDKD`
- `CMD_SETLOCALPIDOUT`

## 18. 数据与状态同步机制

界面并不是单向“发送控制”，也承担状态同步工作。

同步机制包括：

- 串口连接状态同步
- 电机连接状态同步
- 串口列表同步
- MOS 温度同步
- 母线 ADC 原始值曲线同步
- 连接电机后的参数回填
- 零电位校准结果回填
- 图表采样数据实时同步

## 19. 交互保护与用户体验设计

为了让调试过程更稳定，项目在多处加入了交互保护。

### 19.1 未连接串口保护

当用户在串口未打开的情况下尝试：

- 连接电机
- 启动零电位校准
- 切换波形采样
- 下发目标值
- 下发 PID 参数

程序会：

- 播放系统提示音
- 弹出提示对话框
- 阻止继续发送命令

### 19.2 波形互斥保护

波形订阅采用单选互斥机制，避免多个高频流同时开启，降低：

- 下位机通信压力
- 上位机绘图负担
- UI 卡顿风险

### 19.3 日志降载策略

串口日志默认关闭，避免高频数据显示导致界面刷新阻塞。

### 19.4 自动状态复位

在以下场景中程序会自动重置状态：

- 串口断开
- 串口被拔出
- 串口重新连接

重置内容包括：

- 电机连接灯
- MOS 温度
- 活动波形命令
- 接收缓存

## 20. 视觉设计特征

界面不是系统原生风格，而是强样式化界面，特征包括：

- 无边框窗口
- 自定义标题栏
- 渐变面板
- 卡片式布局
- 彩色按钮与状态灯
- 多主题切换
- 多强调色切换
- 深色图表区

按钮还带有轻微的按压/悬停动画效果，增强操作反馈。

## 21. 当前已知限制

从现有实现看，项目还存在以下限制或预留点：

- “无感”模式页仍为预留占位
- 串口参数下拉框未完全映射到底层打开参数，当前固定按 `8N1`
- 串口日志功能保留但默认停用
- 图表时间轴步进目前为固定值 `0.0005`
- 工程当前在本机 Qt/MinGW 环境下构建时，AutoMoc 阶段存在一个未输出详细诊断的构建问题，需后续单独排查

## 22. 典型使用流程

### 22.1 基本调试流程

1. 启动上位机
2. 选择串口和波特率
3. 点击“打开串口”
4. 点击“连接”连接电机
5. 检查参数是否自动回填
6. 选择控制模式
7. 下发目标值或 PID 参数
8. 选择一个波形按钮观察实时曲线
9. 根据需要进行零电位校准

### 22.2 PID 整定流程

1. 打开串口并连接电机
2. 切换到相应控制模式
3. 选择对应输出波形
4. 修改 KP/KI/KD/输出限制
5. 观察曲线变化
6. 继续微调

### 22.3 零电位校准流程

1. 打开串口
2. 连接电机
3. 点击“零电位校准”
4. 等待校准结束回包
5. 查看零偏值和电角度回填结果

## 23. 模块职责说明

### 23.1 `src/app`

职责：

- 程序启动入口

### 23.2 `src/ui`

职责：

- 主窗口搭建
- 主题与样式
- 界面交互逻辑
- 用户操作和串口命令绑定
- 回包结果回填

### 23.3 `src/plot`

职责：

- 图表曲线管理
- 图表刷新
- 鼠标悬停提示
- 横轴范围控制

### 23.4 `src/serial`

职责：

- 串口打开/关闭
- 端口轮询
- 命令帧发送
- 回包帧解析
- 状态同步

### 23.5 `src/third_party/qcustomplot`

职责：

- 提供底层绘图能力
- 作为第三方库直接集成

## 24. 文档结论

`LiJointMaster3` 当前已经具备一个 FOC 上位机的核心能力闭环：

- 能连串口
- 能连电机
- 能发控制命令
- 能收状态回包
- 能显示实时曲线
- 能做参数整定
- 能做主题切换

从工程状态看，它已经不是演示界面，而是一个具备明确协议、明确控制流程和明确调试场景的专用上位机项目。

后续如果继续迭代，优先建议完善的方向是：

- 补全无感模式页
- 让串口参数下拉真正生效
- 恢复可控的串口日志系统
- 解决当前构建环境中的 AutoMoc 异常

---

## 附录 A：源码注释与文档约定

本项目的源码已添加详细的中文 Doxygen 风格注释。注释约定如下：

### A.1 文件头注释

每个源文件（`.h` / `.cpp`）以 `@file` 标记开头，说明该文件的职责和核心功能：

```cpp
/**
 * @file serialmanager.cpp
 * @brief 串口管理器实现
 *
 * 实现串口枚举、连接管理、协议编解码的核心逻辑。
 * ...
 */
```

### A.2 类注释

类声明前用 `@brief` 描述类的职责：

```cpp
/**
 * @brief 串口管理器
 *
 * 核心职责：
 * 1. 串口热插拔检测（300ms 定时器轮询）
 * 2. 串口打开/关闭（固定 8N1 无流控）
 * ...
 */
class SerialManager : public QObject
```

### A.3 函数注释

公共接口和复杂函数前标注功能、参数和返回值：

```cpp
/**
 * @brief 发送浮点命令帧（核心发送接口）
 *
 * 构造固定 8 字节的命令帧并写入串口
 * @param command 命令字（0-255）
 * @param value   伴随的 float 参数值
 * @return true 发送成功
 */
bool sendFloatCommand(int command, double value);
```

### A.4 成员变量注释

成员变量使用行尾注释：

```cpp
QByteArray m_rxBuffer;             ///< 接收数据缓冲区
QTimer *m_portWatchTimer;          ///< 串口热插拔轮询定时器
```

### A.5 代码段注释

关键逻辑段使用 `// ----` 分隔线和大段注释：

```cpp
// ============================================================================
// buildUi —— 核心 UI 构建函数
// ============================================================================
```

---

## 附录 B：数据流与信号链

本附录描述数据从物理串口到 UI 显示或控制命令发送的完整路径。

### B.1 上行数据流（接收：MCU → 上位机）

```
[MCU 串口发送]
      ↓
QSerialPort::readyRead
      ↓
SerialManager::readSerialData()     // 读取全部可用字节
      ↓
m_rxBuffer.append(bytes)            // 拼接到接收缓冲区
      ↓
SerialManager::parseRxBuffer()       // 循环解析完整帧
      ↓
SerialManager::tryParseOneFrame()    // 验证帧头/帧尾/校验和，提取 float 数组
      ↓
emit frameParsed(cmd, values)        // 发射解析信号
      ↓
Widget 中 frameParsed 信号槽:
  ├── CMD_CONNECT_MOTOR     → 参数回填（极对数/PID 系数/Rs/Lq/Ld 等）
  ├── CMD_ZEROCALIBRATIO_OVER → 零偏值 + 电角度回填
  ├── CMD_MOSTEMP           → MOS 温度文本 + 滑条更新
  └── 其他波形命令          → appendTrendValues() → PlotManager::appendData()
```

### B.2 下行数据流（发送：上位机 → MCU）

```
用户操作界面控件
      ↓
信号槽触发 → m_serial->sendFloatCommand(cmd, value)
      ↓
构造 8 字节帧: [0xA5][CMD][4B float LE][校验和][0x49]
      ↓
QSerialPort::write(frame)
      ↓
[MCU 串口接收并解析执行]
```

### B.3 热插拔检测流程

```
m_portWatchTimer (300ms)
      ↓
SerialManager::updateAvailablePorts()
      ↓
QSerialPortInfo::availablePorts() + /dev/ttyUSB* 扫描
      ↓
检查已连接串口是否仍在列表中
  ├── 不在 → 自动 close + 状态复位（电机/温度/波形）
  └── 在   → 检查列表是否变化，更新 UI
```

---

## 附录 C：类图与依赖关系

```
┌─────────────────────────────────────────────────────────────┐
│                        Widget (主窗口)                       │
│  ┌──────────────────────────────────────────────────────┐  │
│  │                     buildUi()                         │  │
│  │  ┌──────────┐  ┌──────────────────┐  ┌───────────┐  │  │
│  │  │ 左侧面板  │  │    图表区        │  │ 底部调试台 │  │  │
│  │  │ 串口配置  │  │ QCustomPlot     │  │ 零位/控制 │  │  │
│  │  │ 电机参数  │  │ PlotManager     │  │ PID/目标  │  │  │
│  │  └──────────┘  └──────────────────┘  └───────────┘  │  │
│  └──────────────────────────────────────────────────────┘  │
│                          │                                  │
│                    ┌─────┴──────┐                          │
│                    │ SerialManager                         │
│                    │  串口通信 + 协议解析                    │
│                    │  sendFloatCommand()                   │
│                    │  tryParseOneFrame()                   │
│                    │  frameParsed 信号                      │
│                    └────────────┘                          │
│                          │                                  │
│                    QSerialPort                               │
│                    [AT32 MCU]                               │
└─────────────────────────────────────────────────────────────┘
```

---

## 附录 D：构建说明

### D.1 环境要求

| 依赖 | 版本要求 |
|------|---------|
| CMake | ≥ 3.16 |
| C++ 编译器 | 支持 C++17 |
| Qt | Qt 5.15+ 或 Qt 6.x |
| Qt SerialPort | 可选（无此模块时串口功能禁用） |

### D.2 构建步骤

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### D.3 CMake 选项

| 选项 | 说明 |
|------|------|
| `LIJOINT_HAS_SERIALPORT` | 自动检测，有 SerialPort 模块时为 `ON` |
| MinGW 特殊处理 | 启用 `-Wa,-mbig-obj` 避免大目标文件错误 |

---

## 附录 E：版本历史

| 日期 | 版本 | 说明 |
|------|------|------|
| 2025-12-02 | 1.0 | 初始版本，有感 FOC 调试功能完整实现 |
| 2026-xx-xx | 1.1 | 增加 SMO 无感观测调试页（预留）、完善代码注释和文档 |
