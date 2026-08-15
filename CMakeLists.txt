cmake_minimum_required(VERSION 3.20)
project(nt-taskmanager VERSION 0.1.0 LANGUAGES CXX)

# Genau eine Abhaengigkeit: Qt6.
#
# Naheliegend waeren libksysguard fuer die Prozesse und libtaskmanager
# fuer die Fensterliste - beide liegen auf jedem Plasma-System. Sie
# ziehen aber die KDE-Frameworks nach, und gebraucht werden sie nicht:
# systemd und KWin sprechen D-Bus, das steckt in Qt selbst, und die
# Prozessdaten stehen in /proc. Ein Programm dieser Groesse soll sich
# auf einem beliebigen Qt-System bauen lassen.
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets DBus)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

qt_standard_project_setup()

qt_add_executable(nt-taskmanager
    src/main.cpp
    src/hauptfenster.cpp
    src/hauptfenster.h
    src/prozessquelle.cpp
    src/prozessquelle.h
    src/prozessmodell.cpp
    src/prozessmodell.h
    src/dienstemodell.cpp
    src/dienstemodell.h
)

target_link_libraries(nt-taskmanager PRIVATE Qt6::Widgets Qt6::DBus)

include(GNUInstallDirs)
install(TARGETS nt-taskmanager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/nt-taskmanager.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
