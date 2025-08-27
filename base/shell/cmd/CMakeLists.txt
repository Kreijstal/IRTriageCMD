cmake_minimum_required(VERSION 3.16)
project(cmd)

# Enable resource compilation for .rc files
ENABLE_LANGUAGE(RC)

add_definitions(
    -DANONYMOUSUNIONS
    -D_DEBUG_MEM)

# The include directories for ReactOS are not available in a standalone build.
# We will rely on the system headers and local project headers.
# include_directories(${REACTOS_SOURCE_DIR}/sdk/include/reactos/wine)
# include_directories(${REACTOS_SOURCE_DIR}/sdk/lib/conutils)

list(APPEND SOURCE
    alias.c
    assoc.c
    attrib.c
    batch.c
    beep.c
    call.c
    choice.c
    cls.c
    cmd.c
    cmddbg.c
    cmdinput.c
    cmdtable.c
    color.c
    console.c
    copy.c
    ctty.c
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
    memory.c
    misc.c
    mklink.c
    move.c
    msgbox.c
    parser.c
    path.c
    pause.c
    prompt.c
    redir.c
    ren.c
    replace.c
    screen.c
    set.c
    setlocal.c
    shift.c
    start.c
    strtoclr.c
    time.c
    timer.c
    title.c
    type.c
    ver.c
    verify.c
    vol.c
    where.c
    window.c
    precomp.h)

# Add the executable target, including the resource file
add_executable(cmd ${SOURCE} cmd.rc)

# Add the current source directory to the include path for config.h
target_include_directories(cmd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Use modern CMake for precompiled headers
target_precompile_headers(cmd PRIVATE precomp.h)

# Set properties for a Win32 console application and define UNICODE
set_target_properties(cmd PROPERTIES WIN32_EXECUTABLE OFF)
target_compile_definitions(cmd PRIVATE UNICODE _UNICODE)

# Link against standard Windows libraries
target_link_libraries(cmd advapi32 user32 kernel32 ntdll)
