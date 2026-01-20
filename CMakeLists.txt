cmake_minimum_required(VERSION 3.16)
project(CloudDeck)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core WebEngine WebEngineWidgets)

# Create the library
add_library(clouddeck STATIC
    src/clouddeckmanager.cpp
    src/clouddeckmanager.h
)

target_link_libraries(clouddeck
    Qt6::Core
    Qt6::WebEngine
    Qt6::WebEngineWidgets
)

target_include_directories(clouddeck PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Installation
install(TARGETS clouddeck
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES src/clouddeckmanager.h
    DESTINATION include/clouddeck
)
