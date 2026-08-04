# 前端便携 ROS 前缀设计

## 问题

直接双击部署目录中的 `MzTLZ.exe` 时，Windows 能找到随程序复制的 ROS DLL，但 ROS2 的类型支持加载器无法通过 ament 索引定位 `mz_interfaces`，启动时报错：

`Failed to find library 'mz_interfaces__rosidl_typesupport_fastrtps_cpp'`

## 方案

- 部署包增加 `ros_prefix` 子目录，保留 `mz_interfaces` 的 `bin` 和 `share` 安装结构。
- 程序在 `rclcpp::init` 前，将 `<程序目录>/ros_prefix` 加入 `AMENT_PREFIX_PATH`。
- 保留用户已有的 `AMENT_PREFIX_PATH`，并在本机常用的 `C:/opt/ros/foxy/x64` 存在时补入该前缀。
- 同时将对应 `bin` 目录补入进程 `PATH`，确保 ROS 动态插件可加载。

不采用仅提供启动批处理的方式，因为部署目标是直接双击 `MzTLZ.exe`。

## 验收

- 不手工执行 ROS `setup.bat`，直接启动部署目录中的 `MzTLZ.exe`。
- `mz_interfaces` 类型支持库加载成功。
- 主窗口正常出现并保持响应。
