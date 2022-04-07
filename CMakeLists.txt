cmake_minimum_required(VERSION 3.13)

set(ENABLE_PIGPIO "0" CACHE STRING "Description")

project(ECT60ctrl
	VERSION 0.0.1
	DESCRIPTION "ECT60 servo control ncurses gui"
	LANGUAGES C 
)

find_package(Curses REQUIRED)
find_package(EtherCAT REQUIRED)
find_package(Threads REQUIRED)

set(SOURCE main.c servo_gui.c)
set(NAME_EXE ECT60ctrl)

add_executable(${NAME_EXE} ${SOURCE})


target_link_libraries(${NAME_EXE}
	PRIVATE EtherLab::EtherCAT
	PRIVATE ${CURSES_LIBRARY} 
	PRIVATE pthread
	PRIVATE rt)

if(${ENABLE_PIGPIO} EQUAL "1")
	add_compile_definitions(PIGPIO_OUT)
	target_link_libraries(${NAME_EXE}
		PRIVATE pigpio)
endif()

