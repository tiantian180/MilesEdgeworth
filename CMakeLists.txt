cmake_minimum_required(VERSION 3.15)

project(MilesEdgeworth)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# 整理IDE中的文件分类
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

file(GLOB_RECURSE SOURCES "*.cpp" "*.cxx" "*.cc")
file(GLOB_RECURSE HEADERS "*.h")
file(GLOB_RECURSE RESOURCES "*.qrc")
file(GLOB_RECURSE FORMS "*.ui")
file(GLOB_RECURSE STAMPS "*.stamp")

source_group("src" FILES ${SOURCES})
source_group("header" FILES ${HEADERS})
source_group("forms" FILES ${FORMS})
source_group("res" FILES ${RESOURCES})
source_group("stamps" FILES ${STAMPS})

if(MSVC)
    add_compile_options("/utf-8")
    add_definitions(-DUNICODE -D_UNICODE)
    add_compile_options(/MP)
endif ()

# ui*.h文件搜索路径
set(CMAKE_AUTOUIC_SEARCH_PATHS ${PROJECT_SOURCE_DIR}/codes/qt_ui)

set(QT_SRC
	codes/qt_src/MilesEdgeworth/MilesEdgeworth.cpp
	codes/qt_src/MilesEdgeworth/MilesEdgeworth.h

	codes/qt_src/PicViewer/PicViewer.cpp
	codes/qt_src/PicViewer/PicViewer.h

	codes/qt_src/ProsecutorBadge/ProsecutorBadge.cpp
	codes/qt_src/ProsecutorBadge/ProsecutorBadge.h
)

set(QT_UI
	codes/qt_ui/MilesEdgeworth.ui
	codes/qt_ui/PicViewer.ui
	codes/qt_ui/ProsecutorBadge.ui
)

set(RES
	resource/MilesEdgeworth.qrc
)

set(SRC
	${QT_SRC}
	${QT_UI}
	${RES}

	codes/src/MyGraphicsView.cpp
	codes/src/MyGraphicsView.h

	codes/src/main.cpp
)

# include路径前缀
include_directories(${PROJECT_SOURCE_DIR}/codes/qt_src)
include_directories(${PROJECT_SOURCE_DIR}/codes/src)

# 收集依赖的qt库
set(QT_LIB
	Core
	Widgets
	Gui
	Multimedia
	Network
)
message(LOG ${CMAKE_PREFIX_PATH})
find_package(Qt6 COMPONENTS REQUIRED
${QT_LIB}
)
# 组合连接时的名字
list(TRANSFORM QT_LIB PREPEND "Qt6::" OUTPUT_VARIABLE QT_LIB_LINK)

foreach(item ${QT_LIB_LINK})
	get_target_property(LibLocation ${item} LOCATION)
	get_filename_component(LibDir ${LibLocation} DIRECTORY)
	list(APPEND LIB_LOCATION ${LibLocation})
endforeach()

message(${LibDir})
message(${LIB_LOCATION})

add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME} ${QT_LIB_LINK})

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
	WIN32_EXECUTABLE ON
)

if(WIN32)
		set(ICON_NAME favicon.ico)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_BINARY_DIR}/appicon.rc")
    add_custom_command(OUTPUT ${APP_ICON_RESOURCE_WINDOWS}
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/resource/icon/${ICON_NAME} ${CMAKE_CURRENT_BINARY_DIR}/${ICON_NAME}
        COMMAND ${CMAKE_COMMAND} -E echo "IDI_ICON1 ICON DISCARDABLE \"${ICON_NAME}\"" > ${APP_ICON_RESOURCE_WINDOWS}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resource/icon/${ICON_NAME})
    target_sources(${PROJECT_NAME} PRIVATE ${APP_ICON_RESOURCE_WINDOWS})
endif()

if (WIN32)
		set(DeployArg
			--no-quick-import --no-translations --no-opengl-sw --no-system-d3d-compiler)
		add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD 
			COMMAND Qt6::windeployqt
			${DeployArg} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
	)
endif ()


set(CMAKE_INSTALL_PREFIX "../install")

# 查找 依赖的qt plugin
if(WIN32)
	find_program(QT_QMAKE_EXECUTABLE qmake HINTS ${LibDir})
	execute_process(COMMAND ${QT_QMAKE_EXECUTABLE} -query QT_INSTALL_PLUGINS
									OUTPUT_VARIABLE QT_PLUGINS_DIR
									OUTPUT_STRIP_TRAILING_WHITESPACE)
	set(QT_QWINDOWS_DLL ${QT_PLUGINS_DIR}/platforms/qwindows.dll)
	set(QT_STYLES_DLL ${QT_PLUGINS_DIR}/styles/qwindowsvistastyle.dll)
	set(QT_IMAGEFORMATS_DLL ${QT_PLUGINS_DIR}/imageformats/qgif.dll
		${QT_PLUGINS_DIR}/imageformats/qico.dll
		${QT_PLUGINS_DIR}/imageformats/qjpeg.dll
		${QT_PLUGINS_DIR}/imageformats/qsvg.dll
	)

	install(FILES ${QT_QWINDOWS_DLL} DESTINATION bin/platforms)
	install(FILES ${QT_STYLES_DLL} DESTINATION bin/styles)
	install(FILES ${QT_IMAGEFORMATS_DLL} DESTINATION bin/imageformats)
endif()

install(FILES ${LIB_LOCATION} DESTINATION bin/)
install (TARGETS ${PROJECT_NAME})

# 设置打包输出的路径
set(CPACK_PACKAGE_DIRECTORY ${CMAKE_HOME_DIRECTORY}/package)

# 其他CPack配置选项
set(CPACK_GENERATOR "ZIP") # 或其他生成器，如 DEB, RPM, TGZ 等

include (InstallRequiredSystemLibraries)
include (CPack)
