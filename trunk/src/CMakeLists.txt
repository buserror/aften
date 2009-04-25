include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})

set(Aften_SRCS Aften.cpp main.cpp)
qt4_automoc(${Aften_SRCS})
add_executable(Aften ${Aften_SRCS})
target_link_libraries(Aften ${QT_QTCORE_LIBRARY})
