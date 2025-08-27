# IRTriageCMD
IRTriage Command Line Interpreter (Cmd.exe)

## Building

This project can be compiled with MinGW using CMake.

### Prerequisites

- A C compiler, such as GCC
- MinGW-w64
- CMake
- Make

### Instructions

1.  **Create a build directory:**
    ```bash
    mkdir build
    ```

2.  **Change into the build directory:**
    ```bash
    cd build
    ```

3.  **Run CMake to configure the project.** The `CMAKE_TOOLCHAIN_FILE` flag is used to specify the MinGW toolchain file.
    ```bash
    cmake ../pcmd -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw.cmake
    ```

4.  **Run make to build the project:**
    ```bash
    make
    ```

The executable, `pcmd.exe`, will be created in the `build/bin` directory.

---

### Original README content

The original README content is preserved below for reference.

Download [ReactOS source](http://downloads.sourceforge.net/reactos/ReactOS-0.4.0-src.zip)

Download [ReactOS build environment](http://sourceforge.net/projects/reactos/files/RosBE-Windows/i386/2.1.3/RosBE-2.1.3.exe/download)

Download [IRTriageCmd](https://github.com/AJMartel/IRTriageCMD/archive/master.zip)


    - Install build environment.
    - Extract source into source directory.
    - Run build environment
    - configure
    - cd output-MinGW-i386
    - make cmd
    - extract IRTriageCMD source over ReactOS source
    - delete output-MinGW-i386\base\shell\cmd\cmd.exe
    - make cmd

Done IRTriageCMD is located at "output-MinGW-i386\base\shell\cmd\cmd.exe" 




http://blog.didierstevens.com/2015/12/13/windows-backup-privilege-cmd-exe/

https://isc.sans.edu/forums/diary/Use+The+Privilege/20483/

http://www.riosec.com/articles

http://perldoc.perl.org/perlembed.html
