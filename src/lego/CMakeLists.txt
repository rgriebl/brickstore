

qt_add_library(lego_module STATIC
	pickabrick.h
	pickabrick.cpp
	pccfetcher.h
	pccfetcher.cpp
)

target_link_libraries(lego_module PRIVATE
	Qt6::Core
	Qt6::Network
	Qt6::Concurrent
	bricklink_module
)

target_link_libraries(${PROJECT_NAME} PRIVATE lego_module)