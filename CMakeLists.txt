cmake_minimum_required(VERSION 3.20)
project(TaskbarHardwareMonitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(MSVC)
    # Keep all C++ string literals deterministic across developer/system code pages.
    # The UI uses wide Win32 APIs; /utf-8 makes source literals such as Chinese text safe.
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
endif()

add_executable(TaskbarHardwareMonitor WIN32
    src/main.cpp
    src/app/App.cpp
    src/app/App.h
    src/app/SingleInstance.h
    src/config/Config.cpp
    src/config/Config.h
    src/monitor/CpuMonitor.cpp
    src/monitor/CpuMonitor.h
    src/monitor/CpuTemperature.cpp
    src/monitor/CpuTemperature.h
    src/monitor/TemperatureProvider.cpp
    src/monitor/TemperatureProvider.h
    src/monitor/PowerProvider.cpp
    src/monitor/PowerProvider.h
    src/monitor/WmiTemperatureProvider.cpp
    src/monitor/WmiTemperatureProvider.h
    src/hardware/HardwareAccess.h
    src/hardware/CpuVendor.cpp
    src/hardware/CpuVendor.h
    src/hardware/bus/PciAccess.cpp
    src/hardware/bus/PciAccess.h
    src/hardware/amd/AmdTemperature.cpp
    src/hardware/amd/AmdTemperature.h
    src/hardware/amd/AmdCpuIdentity.cpp
    src/hardware/Sensor.h
    src/hardware/SensorRegistry.cpp
    src/hardware/SensorRegistry.h
    src/monitor/SnapshotSensorProvider.cpp
    src/monitor/SnapshotSensorProvider.h
    src/monitor/GpuProvider.h
    src/monitor/StorageProvider.h
    src/monitor/NetworkProvider.h
    src/monitor/PawnIoTemperature.cpp
    src/monitor/PawnIoTemperature.h
    src/monitor/NetworkMonitor.cpp
    src/monitor/NetworkMonitor.h
    src/monitor/SensorManager.cpp
    src/monitor/SensorManager.h
    src/monitor/SensorDemand.cpp
    src/monitor/SensorDemand.h
    src/monitor/SensorTypes.h
    src/ipc/SharedSensorSnapshot.h
    src/ipc/SharedSnapshotPublisher.cpp
    src/ipc/SharedSnapshotPublisher.h
    src/monitor/GpuMonitor.cpp
    src/monitor/GpuMonitor.h
    src/monitor/StorageMonitor.cpp
    src/monitor/StorageMonitor.h
    src/monitor/SystemMonitor.cpp
    src/monitor/SystemMonitor.h
    src/ui/SettingsWindow.cpp
    src/ui/SettingsWindow.h
    src/ui/TaskbarLayout.cpp
    src/ui/TaskbarLayout.h
    resources/app.rc
    resources/app.manifest
)

target_compile_definitions(TaskbarHardwareMonitor PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
    _WIN32_WINNT=0x0603 WINVER=0x0603 NTDDI_VERSION=0x06030000 WINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP
)

target_compile_options(TaskbarHardwareMonitor PRIVATE /W4 /permissive- $<$<CONFIG:Release>:/O2;/GL;/MT>)
target_link_options(TaskbarHardwareMonitor PRIVATE
    /MANIFESTUAC:NO
    $<$<CONFIG:Release>:/LTCG>
)
target_link_libraries(TaskbarHardwareMonitor PRIVATE iphlpapi user32 gdi32 advapi32 wbemuuid comdlg32 PowrProf taskschd ole32 oleaut32)

add_library(TaskbarBand SHARED
    src/band/TaskbarBand.cpp
    src/band/TaskbarBand.h
    src/band/BandExports.cpp
    src/band/TaskbarBand.def
    src/ui/TaskbarLayout.cpp
    src/ui/TaskbarLayout.h
    src/ipc/SharedSensorSnapshot.h
    src/monitor/SensorTypes.h
)
target_compile_definitions(TaskbarBand PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
    _WIN32_WINNT=0x0603 WINVER=0x0603 NTDDI_VERSION=0x06030000
)
target_compile_options(TaskbarBand PRIVATE /W4 /permissive-)
target_link_libraries(TaskbarBand PRIVATE user32 gdi32 ole32 comctl32)

add_executable(TaskbarBandActivator
    src/activator/main.cpp
)
target_compile_definitions(TaskbarBandActivator PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
    _WIN32_WINNT=0x0603 WINVER=0x0603 NTDDI_VERSION=0x06030000
)
target_compile_options(TaskbarBandActivator PRIVATE /W4 /permissive-)
target_link_libraries(TaskbarBandActivator PRIVATE ole32)

option(BUILD_UNIT_TESTS "Build native unit tests" ON)
if(BUILD_UNIT_TESTS)
    enable_testing()
    add_executable(TaskbarHardwareMonitorTests
        tests/TestMain.cpp
        src/monitor/CpuTemperature.cpp
        src/monitor/CpuTemperature.h
        src/monitor/TemperatureProvider.cpp
        src/monitor/TemperatureProvider.h
        src/monitor/PowerProvider.cpp
        src/monitor/PowerProvider.h
        src/monitor/WmiTemperatureProvider.cpp
        src/monitor/WmiTemperatureProvider.h
        src/hardware/CpuVendor.cpp
        src/hardware/CpuVendor.h
        src/hardware/bus/PciAccess.cpp
        src/hardware/bus/PciAccess.h
        src/hardware/amd/AmdTemperature.cpp
        src/hardware/amd/AmdTemperature.h
        src/hardware/amd/AmdCpuIdentity.cpp
        src/hardware/Sensor.h
        src/hardware/SensorRegistry.cpp
        src/hardware/SensorRegistry.h
        src/monitor/SnapshotSensorProvider.cpp
        src/monitor/SnapshotSensorProvider.h
        src/monitor/GpuProvider.h
        src/monitor/StorageProvider.h
        src/monitor/NetworkProvider.h
        src/monitor/PawnIoTemperature.cpp
        src/monitor/PawnIoTemperature.h
        src/ui/TaskbarLayout.cpp
        src/ui/TaskbarLayout.h
        src/monitor/SensorTypes.h
        src/monitor/SensorDemand.cpp
        src/monitor/SensorDemand.h
        src/config/Config.h
        src/config/Config.cpp
        src/ipc/SharedSensorSnapshot.h
    )
    target_compile_definitions(TaskbarHardwareMonitorTests PRIVATE UNICODE _UNICODE NOMINMAX)
    target_link_libraries(TaskbarHardwareMonitorTests PRIVATE advapi32 taskschd ole32 oleaut32 wbemuuid)
    add_test(NAME TaskbarHardwareMonitorTests COMMAND TaskbarHardwareMonitorTests)
endif()
