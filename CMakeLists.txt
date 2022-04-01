cmake_minimum_required(VERSION 3.9)

project(ECT60ctrl
	VERSION 0.0.1
	DESCRIPTION "ECT60 servo control ncurses gui"
	LANGUAGES C 
)


find_package(EtherCAT REQUIRED)
find_package(Threads REQUIRED)


set(SOURCE main.c servo_gui.c)
set(NAME_EXE ECT60ctrl)

add_executable(${NAME_EXE} ${SOURCE})


target_link_libraries(${NAME_EXE}
	PRIVATE pigpio
	PRIVATE ethercat
	PRIVATE ncurses 
	PRIVATE pthread
	PRIVATE rt)


