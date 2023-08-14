import qbs 1.0

QtcPlugin {
    name: "Terminal"

    Depends { name: "Core" }
    Depends { name: "ProjectExplorer" }
    Depends { name: "vterm" }
    Depends { name: "ptyqt" }

    files: [
        "celliterator.cpp",
        "celliterator.h",
        "glyphcache.cpp",
        "glyphcache.h",
        "keys.cpp",
        "keys.h",
        "scrollback.cpp",
        "scrollback.h",
        "shellmodel.cpp",
        "shellmodel.h",
        "shellintegration.cpp",
        "shellintegration.h",
        "shortcutmap.cpp",
        "shortcutmap.h",
        "terminal.qrc",
        "terminalconstants.h",
        "terminalicons.h",
        "terminalpane.cpp",
        "terminalpane.h",
        "terminalplugin.cpp",
        "terminalprocessimpl.cpp",
        "terminalprocessimpl.h",
        "terminalsearch.cpp",
        "terminalsearch.h",
        "terminalsettings.cpp",
        "terminalsettings.h",
        "terminalsurface.cpp",
        "terminalsurface.h",
        "terminaltr.h",
        "terminalwidget.cpp",
        "terminalwidget.h",
    ]
}

