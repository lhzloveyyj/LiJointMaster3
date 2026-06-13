# LiJointMaster3 V1.1.1

## 更新日志 (Changelog)

### 新功能
- **电角度零偏设置**：新增设置电角度零偏功能（[d8d09fe](https://github.com/lhzloveyyj/LiJointMaster3/commit/d8d09fe)）
- **电角度数据**：界面新增电角度数据显示（[df6068a](https://github.com/lhzloveyyj/LiJointMaster3/commit/df6068a)）
- **零点位验证**：增加零点位验证功能（[889f412](https://github.com/lhzloveyyj/LiJointMaster3/commit/889f412)）
- **机械角度打印**：机械角度打印信息中添加电角度显示（[31a0ad7](https://github.com/lhzloveyyj/LiJointMaster3/commit/31a0ad7)）

### 优化
- **串口通信重写**：修改了串口发送接收逻辑（[a75100b](https://github.com/lhzloveyyj/LiJointMaster3/commit/a75100b)）
- **绘图优化**：优化绘图管理器和界面控件（[6a481e6](https://github.com/lhzloveyyj/LiJointMaster3/commit/6a481e6)）
- **代码清理**：去掉无感（sensorless）相关代码（[488113b](https://github.com/lhzloveyyj/LiJointMaster3/commit/488113b)）

### 工程
- 版本号从 1.1.0 更新至 1.1.1
- 新增 `build.bat` 一键构建脚本（Windows）
- 更新打包脚本和 Inno Setup 配置

## 变更文件

| 文件 | 变更说明 |
|------|----------|
| `src/serial/serialcommand.h` | 串口命令协议更新 |
| `src/serial/serialmanager.cpp` | 串口收发逻辑重写 |
| `src/plot/plotmanager.cpp/h` | 绘图管理优化 |
| `src/ui/widget.cpp/h` | 界面功能增强 |
| `CMakeLists.txt` | 版本号更新 |
| `packaging/LiJointMaster3.iss` | 安装包版本更新 |
| `scripts/create-release.ps1` | 发布脚本更新 |
| `build.bat` | 新增 Windows 一键构建脚本 |

## Included Artifacts

- source/: 源码归档
- windows/: Windows 安装包和便携版
- linux/: Linux 发布物目录

## Notes

- Version source: CMakeLists.txt and packaging/LiJointMaster3.iss
- Windows installer: dist\installer\LiJointMaster3-V1.1.1-Setup.exe
- Windows portable: dist\windows
