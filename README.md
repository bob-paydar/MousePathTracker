# Mouse Path Tracker

**Mouse Path Tracker** is a lightweight Windows desktop utility that
monitors your mouse movements across all displays and calculates the
total distance traveled in **meters, kilometers, and miles**.\
The project is written in **C++ (Win32 API)** and designed to be built
with **Microsoft Visual Studio 2022**.

------------------------------------------------------------------------

## ✨ Features

-   Tracks global mouse movement using a **low-level mouse hook
    (WH_MOUSE_LL)**.
-   Converts pixel movement into real-world distances using each
    monitor's **reported physical size (EDID)**.
-   Shows live totals in a simple read-only window (no buttons or
    hotkeys).
-   **Minimize to system tray** with tray icon restore and menu options.
-   **Tray menu actions**:
    -   Restore
    -   Start / Pause tracking
    -   Reset counter
    -   Exit
-   Saves progress automatically to an **INI file** every minute and
    upon exit.
-   Restores saved totals and tracking state at the next launch.
-   Fixed-size window (not resizable, no maximize button).

------------------------------------------------------------------------

## 🛠️ Build Instructions

1.  Open **Visual Studio 2022**.
2.  Create a new **Windows Desktop Application (Win32)** project.
3.  Add the `MousePathTracker.cpp` source file to the project.
4.  Project settings:
    -   **Character Set**: Use Unicode Character Set
    -   **Linker → System → Subsystem**: Windows (/SUBSYSTEM:WINDOWS)
5.  Build and run.

------------------------------------------------------------------------

## 📂 Persistence

-   The application creates an INI file (with the same name as the EXE)
    in the same directory as the executable.
-   Example: `MousePathTracker.exe` → `MousePathTracker.ini`
-   Stored values:
    -   `TotalMM` → accumulated distance (in millimeters)
    -   `Running` → `1` (tracking) or `0` (paused)

------------------------------------------------------------------------

## 📥 Tray Menu Usage

Right-click the tray icon to open the context menu with options to
**Restore**, **Start/Pause**, **Reset**, or **Exit**.

------------------------------------------------------------------------

## 📜 License

This project is released under the **MIT License**.\
Programmer: **Bob Paydar**
