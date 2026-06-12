#pragma once

/**
 * @file serialcommand.h
 * @brief 串口通信协议命令定义
 *
 * 定义上位机与 AT32 MCU 下位机之间的通信协议：
 * - 帧头 FRAME_HEAD (0xA5)
 * - 帧尾 FRAME_TAIL (0x49)
 * - 所有命令字枚举 (SerialCommand)
 *
 * 发送帧格式（8 字节固定长度）：
 *   [0xA5][CMD][4字节float数据][CHECKSUM][0x49]
 *
 * 接收帧格式（可变长度）：
 *   [0xA5][CMD][LEN][LEN字节载荷][CHECKSUM][0x49]
 *   其中 LEN 为 4 的倍数，载荷为多个 float
 */

#include <cstdint>

/** @brief 帧头标识 */
constexpr uint8_t FRAME_HEAD = 0xA5;

/** @brief 帧尾标识 */
constexpr uint8_t FRAME_TAIL = 0x49;

/**
 * @brief 串口命令枚举
 *
 * 命令字分类：
 * - 0x01-0x07：连接与基础配置（电机连接、机械角度、极对数、方向、零位校准）
 * - 0x08-0x19：电压/电流/ADC 采样与控制
 * - 0x20-0x27：控制模式切换与 PID 设置
 * - 0x28-0x45：速度/位置/PID 参数设置
 * - 0x46-0x5A：ADC 采样、电机参数与反馈源
 *
 * 每个波形数据流都有一个 OPEN 和 CLOSE 配对命令，
 * 用于开启/关闭下位机的特定数据回传。
 */
enum class SerialCommand : uint8_t
{
    CMD_NONE = 0x00,

    /* ========== 连接与基础配置 (0x01-0x07) ========== */
    CMD_CONNECT_MOTOR = 0x01,            ///< 连接电机（回包带回填参数）
    CMD_MECHANICALANGLE = 0x02,          ///< 开启机械角度回传
    CMD_MECHANICALANGLE_CLOSE = 0x03,    ///< 关闭机械角度回传
    CMD_SETPAIRS = 0x04,                 ///< 设置电机极对数
    CMD_SETDIR = 0x05,                   ///< 设置角度方向
    CMD_ZEROCALIBRATIO = 0x06,           ///< 启动零电位校准
    CMD_ZEROCALIBRATIO_OVER = 0x07,      ///< 结束零电位校准（回包带回偏值和电角度）

    /* ========== 三相电压/电流/ADC 采样 (0x08-0x19) ========== */
    CMD_UABC = 0x08,                     ///< 开启三相电压 Ua/Ub/Uc 回传
    CMD_UABC_CLOSE = 0x09,               ///< 关闭三相电压回传
    CMD_SETUQ = 0x0A,                    ///< 设置 Uq 目标值（q 轴电压）

    CMD_ADC = 0x0B,                      ///< 开启三相 ADC 采样值回传
    CMD_ADC_CLOSE = 0x0C,                ///< 关闭三相 ADC 回传
    CMD_DCVBUS = 0x0D,                   ///< 母线电压值（DC bus voltage）

    CMD_TABC = 0x0E,                     ///< 开启三相 SVPWM 占空比 Ta/Tb/Tc 回传
    CMD_TABC_CLOSE = 0x0F,               ///< 关闭三相 SVPWM 回传

    CMD_IABC = 0x10,                     ///< 开启三相电流 Ia/Ib/Ic 回传
    CMD_IABC_CLOSE = 0x11,               ///< 关闭三相电流回传

    CMD_UALPHA_BETA = 0x12,              ///< 开启 Uα/Uβ（静止坐标系电压）回传
    CMD_UALPHA_BETA_CLOSE = 0x13,        ///< 关闭 Uα/Uβ 回传
    CMD_IALPHA_BETA = 0x14,              ///< 开启 Iα/Iβ（静止坐标系电流）回传
    CMD_IALPHA_BETA_CLOSE = 0x15,        ///< 关闭 Iα/Iβ 回传

    CMD_IQ_ID = 0x16,                    ///< 开启 Iq/Id（旋转坐标系电流）回传
    CMD_IQ_ID_CLOSE = 0x17,              ///< 关闭 Iq/Id 回传
    CMD_SETIQ = 0x18,                    ///< 设置 Iq 目标值（q 轴电流）
    CMD_SETID = 0x19,                    ///< 设置 Id 目标值（d 轴电流）

    /* ========== 控制模式与 PID 设置 (0x20-0x27) ========== */
    CMD_OPEN_LOOP = 0x20,                ///< 切换为开环模式
    CMD_CURRENT_LOOP = 0x21,             ///< 切换为电流环模式
    CMD_SPEED_LOOP = 0x22,               ///< 切换为速度环模式
    CMD_POSITION_LOOP = 0x23,            ///< 切换为位置环模式

    CMD_MOSTEMP = 0x24,                  ///< MOS 管温度回传（自动上报）
    CMD_SETUD = 0x25,                    ///< 设置 Ud 目标值（d 轴电压）
    CMD_SETIQPIDKP = 0x26,               ///< 设置电流环 PID 比例系数
    CMD_SETIQPIDKI = 0x27,               ///< 设置电流环 PID 积分系数

    /* ========== 速度/位置控制与 PID (0x28-0x45) ========== */
    CMD_SPEED = 0x28,                    ///< 开启速度回传
    CMD_SPEED_CLOSE = 0x29,              ///< 关闭速度回传
    CMD_SETSPEEDDIR = 0x30,              ///< 设置速度方向
    CMD_SPEEDOUT = 0x31,                 ///< 开启速度环输出值回传
    CMD_SPEEDOUT_CLOSE = 0x32,           ///< 关闭速度环输出值回传
    CMD_SETSPEEDTAR = 0x33,              ///< 设置速度目标值
    CMD_SETSPEEDPIDKP = 0x34,            ///< 设置速度环 PID 比例系数
    CMD_SETSPEEDPIDKI = 0x35,            ///< 设置速度环 PID 积分系数
    CMD_SETLOCALTAR = 0x36,              ///< 设置位置目标值
    CMD_LOCAL = 0x37,                    ///< 开启位置回传
    CMD_LOCAL_CLOSE = 0x38,              ///< 关闭位置回传
    CMD_LOCALOUT = 0x39,                 ///< 开启位置环输出值回传
    CMD_LOCALOUT_CLOSE = 0x40,           ///< 关闭位置环输出值回传
    CMD_SETLOCALPIDKP = 0x41,            ///< 设置位置环 PID 比例系数
    CMD_SETLOCALPIDKD = 0x42,            ///< 设置位置环 PID 微分系数
    CMD_SETIQPIDOUT = 0x43,              ///< 设置电流环输出限制
    CMD_SETSPEEDPIDOUT = 0x44,           ///< 设置速度环输出限制
    CMD_SETLOCALPIDOUT = 0x45,           ///< 设置位置环输出限制

    /* ========== 反馈源切换与电机参数 (0x46-0x5A) ========== */
    CMD_ADCVBUS = 0x46,                  ///< 开启母线 ADC 原始值回传
    CMD_ADCVBUS_CLOSE = 0x47,            ///< 关闭母线 ADC 回传
    CMD_SETMOTORRS = 0x4E,               ///< 设置电机相电阻 Rs（单位 Ω）
    CMD_SETMOTORLQ = 0x4F,               ///< 设置电机 q 轴电感 Lq（单位 H）
    CMD_SETMOTORLD = 0x50,               ///< 设置电机 d 轴电感 Ld（单位 H）
    CMD_ELECTRICALANGLE = 0x51,          ///< 开启实际电角度回传（有感）
    CMD_ELECTRICALANGLE_CLOSE = 0x52,    ///< 关闭实际电角度回传
    CMD_SETIQMAX = 0x5A,                 ///< 设置 Iq 参考电流硬上限
    CMD_VERIFY_OFFSET = 0x5B,            ///< 多点静态锁定验证零偏（回包6个误差值）
    CMD_TELEMETRY_BUNDLE = 0x5C,         ///< 合并遥测包（bitmask+values）
    CMD_SETELECOFFSET = 0x60,            ///< 手动设置电角度零偏值（单位 rad）
};
