# SQL Server Bootstrap Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Let a deployed frontend create and initialize its SQL Server database automatically on first launch.

**Architecture:** A new bootstrap module reads deployment-local settings, connects to `master`, creates the configured database when absent, and creates the base schema idempotently. The existing `Database` class opens the discovered endpoint, after which the existing repository migration adds newer task/frame fields.

**Tech Stack:** Qt 5.15 Core/SQL, QODBC, SQL Server, CMake, MSVC 2019, Git.

---

### Task 1: Add deployment database settings

**Files:**
- Create: `database.ini`
- Modify: `CMakeLists.txt`

**Step 1: Add defaults**

Create an INI file with `Server=localhost\SQLEXPRESS` and `Name=MzTLZ`.

**Step 2: Copy settings beside the build executable**

Use `configure_file(... COPYONLY)` so development and deployment use the same file shape.

**Step 3: Verify**

Run CMake configure and confirm `database.ini` exists in the Release build directory.

### Task 2: Implement database bootstrap

**Files:**
- Create: `DatabaseBootstrap.h`
- Create: `DatabaseBootstrap.cpp`
- Modify: `Database.cpp`
- Modify: `main.cpp`

**Step 1: Read and validate settings**

Read `database.ini` from `QCoreApplication::applicationDirPath()`. Reject database names outside `[A-Za-z_][A-Za-z0-9_]{0,127}`.

**Step 2: Discover a working SQL Server connection**

Try configured/default server candidates and installed ODBC driver candidates against `master` with Windows integrated authentication.

**Step 3: Create the database and base tables**

Execute `CREATE DATABASE` only when `DB_ID(?)` is null. Reconnect to the target database and run `IF OBJECT_ID(...) IS NULL CREATE TABLE ...` for the four base tables.

**Step 4: Reuse the discovered endpoint**

Set `MZTLZ_DB_SERVER` and `MZTLZ_DB_NAME` for the current process. Make `Database.cpp` build its normal connection string from these values.

**Step 5: Invoke bootstrap before the existing connection**

Call the bootstrap in `main.cpp`; reuse the current non-blocking UI error path on failure.

**Step 6: Commit**

Stage new files and only the database-related hunks from already modified files, then commit.

### Task 3: Build and verify with an isolated database

**Files:**
- Generated test configuration: Release build `database.ini`

**Step 1: Build the Release target**

Expected: CMake configure and `MzTLZ` link succeed.

**Step 2: Use a unique test database**

Temporarily set the generated INI database name to `MzTLZ_CodexBootstrapTest_20260804`, launch the frontend, and wait for startup initialization.

**Step 3: Query SQL Server**

Confirm the database exists and contains `InspectionRecord`, `ParticleDetection`, `ProcessParameter`, `ProcessParameterHistory`, and `InspectionFrame`.

**Step 4: Clean up the test database**

Drop only the uniquely named test database and restore the generated INI from the committed default.

### Task 4: Update deployment

**Files:**
- Update: `D:\QtProj\tongliiz\deploy_20260804\MzTLZ`

**Step 1: Copy new runtime files**

Copy the rebuilt EXE and `database.ini` while preserving the existing Qt/ROS runtime contents.

**Step 2: Verify direct launch**

Start the deployed EXE for three seconds and confirm the process remains responsive.

**Step 3: Refresh manifests**

Regenerate `dependency_manifest.txt` and `文件清单_SHA256.txt`.
