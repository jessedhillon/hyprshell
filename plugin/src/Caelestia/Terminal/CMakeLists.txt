if(NOT DEFINED GHOSTTY_VT_INCLUDE_DIR)
    message(FATAL_ERROR "GHOSTTY_VT_INCLUDE_DIR not set - libghostty-vt headers required")
endif()
if(NOT DEFINED GHOSTTY_VT_LIB_DIR)
    message(FATAL_ERROR "GHOSTTY_VT_LIB_DIR not set - libghostty-vt library required")
endif()

add_library(ghostty-vt SHARED IMPORTED)
set_target_properties(ghostty-vt PROPERTIES
    IMPORTED_LOCATION "${GHOSTTY_VT_LIB_DIR}/libghostty-vt.so"
    INTERFACE_INCLUDE_DIRECTORIES "${GHOSTTY_VT_INCLUDE_DIR}"
)

qml_module(caelestia-terminal
    URI Caelestia.Terminal
    SOURCES
        pty.hpp pty.cpp
        terminalstate.hpp terminalstate.cpp
        terminalview.hpp terminalview.cpp
    LIBRARIES
        Qt::Gui
        Qt::Quick
        ghostty-vt
        util
)
