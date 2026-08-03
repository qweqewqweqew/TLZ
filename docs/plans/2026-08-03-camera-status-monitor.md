# Camera Status Monitor Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Publish the physical camera connection state on a dedicated ROS2 topic and display it in the Qt running-status panel.

**Architecture:** `camera_node` publishes a compact `CameraStatus` heartbeat once per second with transient-local reliable QoS. `Ros2Bridge` subscribes and forwards the payload to `RunningStatusPanel`, which changes to Unknown when no heartbeat arrives for three seconds.

**Tech Stack:** C++14/17, ROS2 Foxy, `rclcpp`, Qt 5.15 Widgets

---

### Task 1: Define and publish camera status

**Files:**
- Create: `../ros/mz_interfaces/msg/CameraStatus.msg`
- Modify: `../ros/mz_interfaces/CMakeLists.txt`
- Modify: `../ros/mz_sensor/src/CameraNode.cpp`

**Steps:**

1. Add the four status fields and register the message with `rosidl_generate_interfaces`.
2. Add a `camera/status` publisher using Reliable + Transient Local QoS.
3. Publish immediately after initial connection and every second thereafter.
4. Build `mz_interfaces` and `mz_sensor`; expect both packages to finish successfully.
5. Commit only these ROS repository changes.

### Task 2: Subscribe and display status

**Files:**
- Modify: `Ros2Bridge.cpp`
- Modify: `Ros2Bridge.h`
- Modify: `mainwindow.cpp`
- Modify: `RunningStatusPanel.cpp`
- Modify: `RunningStatusPanel.h`

**Steps:**

1. Subscribe to `camera/status` with matching QoS and expose a Qt signal.
2. Add an “设备状态” camera row below “当前速度”.
3. Map fresh messages to Online/Offline and a three-second timeout to Unknown.
4. Build the Qt application and inspect the affected layout.
5. Stage only camera-status hunks, preserving all pre-existing uncommitted changes, and commit.
