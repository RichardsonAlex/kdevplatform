add_definitions(-DTRANSLATION_DOMAIN=\"kdevquickopen\")
add_subdirectory( tests )

set(kdevquickopen_PART_SRCS
    quickopenplugin.cpp
    quickopenmodel.cpp
    quickopenwidget.cpp
    projectfilequickopen.cpp
    duchainitemquickopen.cpp
    declarationlistquickopen.cpp
    projectitemquickopen.cpp
    documentationquickopenprovider.cpp
    actionsquickopenprovider.cpp
    expandingtree/expandingdelegate.cpp
    expandingtree/expandingtree.cpp
    expandingtree/expandingwidgetmodel.cpp
)
ki18n_wrap_ui(kdevquickopen_PART_SRCS
  quickopenwidget.ui
)
qt5_add_resources(kdevquickopen_PART_SRCS kdevquickopen.qrc)
kdevplatform_add_plugin(kdevquickopen JSON kdevquickopen.json SOURCES ${kdevquickopen_PART_SRCS})
target_link_libraries(kdevquickopen KF5::IconThemes KF5::GuiAddons KF5::TextEditor KDev::Language KDev::Interfaces KDev::Project KDev::Util)
