import qbs.FileInfo

QtcManualtest {
    name: "Spinner example"
    type: ["application"]

    Depends { name: "Qt"; submodules: ["widgets"] }
    Depends { name: "Spinner" }

    files: [
        "main.cpp",
    ]
}
