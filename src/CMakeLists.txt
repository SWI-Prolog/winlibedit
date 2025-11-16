cmake_minimum_required(VERSION 3.20)
project(winlibedit LANGUAGES C)

set(LT_VERSION "3:1:0" CACHE STRING "Libedit version info (ignored on MSVC)")

# -------------------------------------------------------------------------
# Generate the headers that were previously built by makelist
# -------------------------------------------------------------------------
set(GENERATED_HEADERS
    ${CMAKE_CURRENT_BINARY_DIR}/vi.h
    ${CMAKE_CURRENT_BINARY_DIR}/emacs.h
    ${CMAKE_CURRENT_BINARY_DIR}/common.h
    ${CMAKE_CURRENT_BINARY_DIR}/fcns.h
    ${CMAKE_CURRENT_BINARY_DIR}/help.h
    ${CMAKE_CURRENT_BINARY_DIR}/func.h
)

set(SCRIPT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/vi.h
    COMMAND ${CMAKE_COMMAND} -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/vi.h
            -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/vi.c
            -P ${SCRIPT_DIR}/gen_header.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/vi.c ${SCRIPT_DIR}/gen_header.cmake
    COMMENT "Generating vi.h"
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/emacs.h
    COMMAND ${CMAKE_COMMAND} -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/emacs.h
            -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/emacs.c
            -P ${SCRIPT_DIR}/gen_header.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/emacs.c ${SCRIPT_DIR}/gen_header.cmake
    COMMENT "Generating emacs.h"
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/common.h
    COMMAND ${CMAKE_COMMAND} -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/common.h
            -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/common.c
            -P ${SCRIPT_DIR}/gen_header.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/common.c ${SCRIPT_DIR}/gen_header.cmake
    COMMENT "Generating common.h"
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fcns.h
  COMMAND ${CMAKE_COMMAND}
          -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/fcns.h
          -DINPUTS="${CMAKE_CURRENT_BINARY_DIR}/vi.h\;${CMAKE_CURRENT_BINARY_DIR}/emacs.h\;${CMAKE_CURRENT_BINARY_DIR}/common.h"
          -P ${SCRIPT_DIR}/gen_fcns.cmake
  DEPENDS
          ${CMAKE_CURRENT_BINARY_DIR}/vi.h
          ${CMAKE_CURRENT_BINARY_DIR}/emacs.h
          ${CMAKE_CURRENT_BINARY_DIR}/common.h
          ${SCRIPT_DIR}/gen_fcns.cmake
  COMMENT "Generating fcns.h (#defines for el_action_t symbols)"
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/help.h
  COMMAND ${CMAKE_COMMAND}
          "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/help.h"
          "-DINPUTS=${CMAKE_CURRENT_SOURCE_DIR}/vi.c\;${CMAKE_CURRENT_SOURCE_DIR}/emacs.c\;${CMAKE_CURRENT_SOURCE_DIR}/common.c"
          -P ${SCRIPT_DIR}/gen_help.cmake
  DEPENDS
          ${CMAKE_CURRENT_SOURCE_DIR}/vi.c
          ${CMAKE_CURRENT_SOURCE_DIR}/emacs.c
          ${CMAKE_CURRENT_SOURCE_DIR}/common.c
          ${SCRIPT_DIR}/gen_help.cmake
  COMMENT "Generating help.h (function help table)"
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/func.h
  COMMAND ${CMAKE_COMMAND}
          "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/func.h"
          "-DINPUTS=${CMAKE_CURRENT_BINARY_DIR}/vi.h\;${CMAKE_CURRENT_BINARY_DIR}/emacs.h\;${CMAKE_CURRENT_BINARY_DIR}/common.h"
          -P ${SCRIPT_DIR}/gen_func.cmake
  DEPENDS
          ${CMAKE_CURRENT_BINARY_DIR}/vi.h
          ${CMAKE_CURRENT_BINARY_DIR}/emacs.h
          ${CMAKE_CURRENT_BINARY_DIR}/common.h
          ${SCRIPT_DIR}/gen_func.cmake
  COMMENT "Generating func.h (function table)"
)

add_custom_target(generate_headers DEPENDS ${GENERATED_HEADERS})

# -------------------------------------------------------------------------
# Core sources
# -------------------------------------------------------------------------
set(LIBEDIT_SOURCES
    chared.c common.c el.c eln.c emacs.c hist.c keymacro.c map.c chartype.c parse.c
    prompt.c read.c refresh.c search.c sig.c terminal.c tty.c vi.c
    reallocarr.c wcsdup.c tokenizer.c tokenizern.c
    history.c historyn.c filecomplete.c readline.c chared.h literal.c
    el.h hist.h histedit.h keymacro.h map.h chartype.h parse.h prompt.h
    read.h refresh.h search.h sig.h sys.h terminal.h tty.h vis.h
    filecomplete.h editline/readline.h literal.h
)

if(WIN32)
  list(APPEND LIBEDIT_SOURCES win_ncurses.c utf8.c)
endif()

include(CheckSymbolExists)
check_symbol_exists(strlcpy "string.h" HAVE_STRLCPY)
check_symbol_exists(strlcat "string.h" HAVE_STRLCAT)
check_symbol_exists(memchr "string.h" HAVE_MEMCHR)
check_symbol_exists(memset "string.h" HAVE_MEMSET)
check_symbol_exists(getline "stdio.h" HAVE_GETLINE)
check_symbol_exists(vis "vis.h" HAVE_VIS)
check_symbol_exists(unvis "vis.h" HAVE_UNVIS)
check_symbol_exists(endpwent "pwd.h" HAVE_ENDPWENT)
check_symbol_exists(getpwuid_r "pwd.h" HAVE_GETPW_R_POSIX)
check_symbol_exists(isascii "ctype.h" HAVE_ISASCII)
check_symbol_exists(secure_getenv "stdlib.h" HAVE_SECURE_GETENV)

include(CheckIncludeFile)
check_include_file("stdint.h" HAVE_STDINT_H)
check_include_file("wchar.h" HAVE_WCHAR_H)
check_include_file("dirent.h" HAVE_DIRENT_H)
check_include_file("dlfcn.h" HAVE_DLFCN_H)
check_include_file("fcntl.h" HAVE_FCNTL_H)
check_include_file("limits.h" HAVE_LIMITS_H)
check_include_file("ncurses.h" HAVE_NCURSES_H)
check_include_file("curses.h" HAVE_CURSES_H)
check_include_file("termcap.h" HAVE_TERMCAP_H)
check_include_file("term.h" HAVE_TERM_H)
check_include_file("sys/wait.h" HAVE_SYS_WAIT_H)
check_include_file("pwd.h" HAVE_PWD_H)

if(NOT HAVE_STRLCPY)
    list(APPEND LIBEDIT_SOURCES strlcpy.c)
endif()
if(NOT HAVE_STRLCAT)
    list(APPEND LIBEDIT_SOURCES strlcat.c)
endif()
if(NOT HAVE_GETLINE)
    list(APPEND LIBEDIT_SOURCES getline.c)
endif()
if(NOT HAVE_VIS)
    list(APPEND LIBEDIT_SOURCES vis.c)
endif()
if(NOT HAVE_UNVIS)
    list(APPEND LIBEDIT_SOURCES unvis.c)
endif()

# -------------------------------------------------------------------------
# Build library
# -------------------------------------------------------------------------
add_library(edit ${LIBEDIT_SOURCES} ${GENERATED_HEADERS})
set_target_properties(edit PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_dependencies(edit generate_headers)

target_include_directories(edit
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/editline
)

target_compile_definitions(edit PRIVATE
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_WARNINGS
)

install(TARGETS edit
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES histedit.h editline/readline.h DESTINATION include)

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES ${GENERATED_HEADERS})
