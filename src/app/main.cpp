/**
 * @file main.cpp
 * @brief LiJointMaster3 应用程序入口
 *
 * FOC 电机上位机软件的启动点。
 * 流程：
 * 1. 创建 QApplication 实例
 * 2. 创建主窗口 Widget 实例（构造中完成 UI 搭建和串口初始化）
 * 3. 显示主窗口
 * 4. 进入 Qt 事件循环
 *
 * 主窗口使用无边框窗口风格，所有 UI 构建逻辑在 Widget 构造函数中完成。
 */

#include "ui/widget.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/icons/motor.ico"));
    Widget w;          // 构造时自动创建 SerialManager 并构建界面
    w.show();          // 显示无边框主窗口
    return QCoreApplication::exec();  // 进入事件循环
}
