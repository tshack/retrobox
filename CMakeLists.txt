cmake_minimum_required (VERSION 2.6)
Project ( retrobox )

# Generate deb with
# cpack -G DEB <source tree>

## DEB PACKAGE GENERATION STUFF ########################
set (CPACK_DEBIAN_PACKAGE_NAME "retrobox")
set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
#set (CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl1.2debian (>=1.2.14-4ubuntu1.1)")
set (CPACK_PACKAGE_CONTACT "tshack@retro-box.com")
set (CPACK_DEBIAN_PACKAGE_DESCRIPTION "retrobox\n Play your favorite old school games with this emulator for the Nintendo Entertainment System!")
set (CPACK_DEBIAN_PACKAGE_SECTION "devel")
########################################################

## GENERAL PACKAGE STUFF ###############################
SET (CPACK_PACKAGE_VERSION_MAJOR "0")
SET (CPACK_PACKAGE_VERSION_MINOR "1")
SET (CPACK_PACKAGE_VERSION_PATCH "0-beta")
INCLUDE (CPack)
########################################################


## BUILD TARGET SOURCE FILES ###########################
set ( SRC_RETRODBG
    retrodbg.c
    6502_types.h
    6502.c 6502.h
    2C02.c 2C02.h
    timer.c timer.h
    disasm.c disasm.h
    romreader.c romreader.h
    memory.c memory.h
    display.c display.h
)

set ( SRC_RETROBOX
    retrobox.c
    6502_types.h
    6502.c 6502.h
    2C02.c 2C02.h
    display.c display.h
    romreader.c romreader.h
    memory.c memory.h
)
########################################################


## DEAL WITH SDL DEPENDS ###############################
Find_Package (SDL REQUIRED)

if ( SDL_INCLUDE_DIR )
	include_directories( ${SDL_INCLUDE_DIR} )
	link_libraries ( ${SDL_LIBRARY} )
endif ( SDL_INCLUDE_DIR )
########################################################


## BUILD TARGETS #######################################
add_executable (
    retrodbg          # executable name
    ${SRC_RETRODBG}   # sources
)

add_executable (
    retrobox
    ${SRC_RETROBOX}
)
########################################################


## INSTALL TARGETS (used by CPack) #####################
INSTALL (
    TARGETS retrobox retrodbg
    RUNTIME DESTINATION local/bin
)
########################################################

