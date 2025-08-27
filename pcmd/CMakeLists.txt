CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(CMD)

SET(CMAKE_SYSTEM_NAME Windows)
SET(CMAKE_C_COMPILER /usr/bin/x86_64-w64-mingw32-gcc)
SET(CMAKE_CXX_COMPILER /usr/bin/x86_64-w64-mingw32-g++)
SET(CMAKE_RC_COMPILER /usr/bin/x86_64-w64-mingw32-windres)

SET(CMAKE_VERBOSE_MAKEFILE ON)

SET(CMAKE_EXE_LINKER_FLAGS "")
SET(CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS "")

SET(SOURCES
	alias.c
	assoc.c
	attrib.c
	batch.c
	beep.c
	call.c
	chcp.c
	choice.c
	cls.c
	cmd.c
	cmddbg.c
	cmdinput.c
	cmdtable.c
	color.c
	console.c
	copy.c
	date.c
	del.c
	delay.c
	dir.c
	dirstack.c
	echo.c
	error.c
	filecomp.c
	for.c
	free.c
	goto.c
	history.c
	if.c
	internal.c
	label.c
	locale.c
	main.c
	memory.c
	misc.c
	mklink.c
	move.c
	parser.c
	path.c
	ren.c
	replace.c
	screen.c
	set.c
	setlocal.c
	shift.c
	start.c
	time.c
	title.c
	type.c
	ver.c
	verify.c
	vol.c
	where.c
	cmd.rc
)

IF(WIN32)
	ADD_DEFINITIONS(-DWIN32)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4996")
ENDIF(WIN32)

IF(UNIX)
	ADD_DEFINITIONS(-DUNIX)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-pointer-sign -Wno-format")
ENDIF(UNIX)

INCLUDE_DIRECTORIES(.)

ADD_EXECUTABLE(pcmd ${SOURCES})
