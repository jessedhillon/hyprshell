pkg_check_modules(GhosttyVt IMPORTED_TARGET libghostty-vt REQUIRED)

qml_module(caelestia-terminal
    URI Caelestia.Terminal
    SOURCES
        pty.hpp pty.cpp
        terminalstate.hpp terminalstate.cpp
        terminalview.hpp terminalview.cpp
    LIBRARIES
        Qt::Gui
        Qt::Quick
        PkgConfig::GhosttyVt
        util
)
