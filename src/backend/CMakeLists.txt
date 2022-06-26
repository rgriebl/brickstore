
qt_add_library(backend_module STATIC
    backendapplication.h
    backendapplication.cpp
    rebuilddatabase.cpp
    rebuilddatabase.h
)

target_link_libraries(backend_module PRIVATE
    Qt6::Core
    Qt6::Gui
)

target_link_libraries(${PROJECT_NAME} PRIVATE backend_module)
