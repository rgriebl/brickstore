# Copyright (C) 2004-2026 Robert Griebl
# SPDX-License-Identifier: GPL-3.0-only

qt_add_library(mcp_server_module STATIC
    mcptool.h
    mcptool.cpp
    mcpserver.h
    mcpserver.cpp
)

target_link_libraries(mcp_server_module PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::HttpServer
)

target_link_libraries(${PROJECT_NAME} PRIVATE mcp_server_module)
