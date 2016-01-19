add_definitions(-DTRANSLATION_DOMAIN=\"kdevplatform\")

find_package(Qt5WebKitWidgets ${QT_MIN_VERSION} CONFIG)
set_package_properties(Qt5WebKitWidgets PROPERTIES TYPE RECOMMENDED
        PURPOSE "Use QtWebkit for rendering documentation"
        URL "http://qt-project.org/"
        )
if(Qt5WebKitWidgets_FOUND)
    set(HAVE_QTWEBKIT TRUE)
    set(KDEVPLATFORM_WEBVIEW_QT_MODULE Qt5WebKitWidgets CACHE STRING "" FORCE)
    set(KDEVPLATFORM_WEBVIEW_LIBRARY Qt5::WebKitWidgets CACHE STRING "" FORCE)
else()
    set(KDEVPLATFORM_WEBVIEW_QT_MODULE Qt5Widgets CACHE STRING "" FORCE)
    set(KDEVPLATFORM_WEBVIEW_LIBRARY Qt5::Widgets CACHE STRING "" FORCE)
endif()

configure_file(config-documentation.h.cmake "${CMAKE_CURRENT_BINARY_DIR}/config-documentation.h")

set(KDevPlatformDocumentation_LIB_SRCS
    standarddocumentationview.cpp
    documentationfindwidget.cpp
    documentationview.cpp
)

ki18n_wrap_ui(KDevPlatformDocumentation_LIB_SRCS documentationfindwidget.ui)
kdevplatform_add_library(KDevPlatformDocumentation SOURCES ${KDevPlatformDocumentation_LIB_SRCS})

target_link_libraries(KDevPlatformDocumentation
        PUBLIC KDev::Interfaces ${KDEVPLATFORM_WEBVIEW_LIBRARY})

install(FILES
    documentationfindwidget.h
    standarddocumentationview.h
    "${CMAKE_CURRENT_BINARY_DIR}/config-documentation.h"
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/kdevplatform/documentation COMPONENT Devel
 )
