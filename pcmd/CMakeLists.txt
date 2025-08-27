cmake_minimum_required(VERSION 2.8)
project(pcmd)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_VERBOSE_MAKEFILE ON)

include_directories(
    ../include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(SOURCES
    memory.c
    beep.c
    cmdinput.c
    screen.c
    type.c
    replace.c
    timer.c
    cls.c
    echo.c
    time.c
    where.c
    misc.c
    prompt.c
    alias.c
    cmddbg.c
    strtoclr.c
    free.c
    console.c
    if.c
    goto.c
    ver.c
    attrib.c
    for.c
    error.c
    start.c
    history.c
    dir.c
    filecomp.c
    chcp.c
    batch.c
    ren.c
    delay.c
    dirstack.c
    setlocal.c
    label.c
    redir.c
    mklink.c
    vol.c
    window.c
    del.c
    date.c
    title.c
    set.c
    cmdtable.c
    msgbox.c
    verify.c
    call.c
    path.c
    choice.c
    internal.c
    copy.c
    cmd.c
    move.c
    assoc.c
    parser.c
    pause.c
    color.c
    main.c
    locale.c
    shift.c
)

if(WIN32)
    add_definitions(-DWIN32)
endif(WIN32)

add_executable(pcmd ${SOURCES})
target_link_libraries(pcmd user32 shell32)

install(TARGETS pcmd DESTINATION bin)
