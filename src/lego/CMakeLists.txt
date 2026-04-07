

qt_add_library(lego_module STATIC
	pickabrick.h
	pickabrick.cpp
)

target_link_libraries(lego_module PRIVATE
	Qt6::Core
	bricklink_module
)

target_link_libraries(${PROJECT_NAME} PRIVATE lego_module)