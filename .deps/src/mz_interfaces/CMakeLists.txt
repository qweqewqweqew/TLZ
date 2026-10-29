cmake_minimum_required(VERSION 3.5)
project(mz_interfaces)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(builtin_interfaces REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  # 相机 / 图像检测
  "msg/GrabCommand.msg"
  "msg/CameraStatus.msg"
  "msg/ImageHandle.msg"
  "msg/ImageRelease.msg"
  "msg/ScanResult.msg"
  "msg/ScanGeometry.msg"
  "msg/SickCameraParameters.msg"
  "msg/SickCameraParameterResult.msg"

  # PLC 通讯
  "msg/PlcPathParams.msg"
  "msg/PlcFeedback.msg"

  # 打磨路径 / 进度
  "msg/MillingPath.msg"
  "msg/MillingPaths.msg"
  "msg/MillingProgress.msg"
  "msg/MillingCommand.msg"
  "msg/MillingCommandResult.msg"

  # 前端 → 后端参数
  "msg/BackendOffset.msg"

  DEPENDENCIES builtin_interfaces
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
