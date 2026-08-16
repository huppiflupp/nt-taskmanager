cmake_minimum_required(VERSION 3.20)
project(nt-taskmanager VERSION 1.0.0 LANGUAGES CXX)

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
    src/anzeigen.cpp
    src/anzeigen.h
    src/gpuquelle.cpp
    src/gpuquelle.h
    src/leistungsseite.cpp
    src/leistungsseite.h
    src/netzquelle.cpp
    src/netzquelle.h
    src/netzseite.cpp
    src/netzseite.h
)

target_link_libraries(nt-taskmanager PRIVATE Qt6::Widgets Qt6::DBus ${CMAKE_DL_LIBS})

# Die Version kommt aus dem project()-Aufruf oben und nur von dort. Sie
# stand vorher ein zweites Mal im Quelltext, und solche Paare laufen
# frueher oder spaeter auseinander.
target_compile_definitions(nt-taskmanager PRIVATE
    NT_TASKMANAGER_VERSION="${PROJECT_VERSION}")

include(GNUInstallDirs)
install(TARGETS nt-taskmanager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/nt-taskmanager.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
# Ohne die AppStream-Datei erscheint das Programm in keiner
# Software-Zentrale - weder in Discover noch in GNOME Software.
install(FILES data/io.github.huppiflupp.nt-taskmanager.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo)
