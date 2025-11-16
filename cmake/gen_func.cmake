# Usage:
#   cmake -DOUTPUT=<func.h> -DINPUTS="vi.h;emacs.h;common.h" -P gen_func.cmake

if(NOT DEFINED OUTPUT)
  message(FATAL_ERROR "gen_func.cmake: OUTPUT not set")
endif()
if(NOT DEFINED INPUTS)
  message(FATAL_ERROR "gen_func.cmake: INPUTS not set")
endif()

file(WRITE "${OUTPUT}" "/* Automatically generated file, do not edit */\n")
file(APPEND "${OUTPUT}" "static const el_func_t el_func[] = {\n")

set(funcs "")

# Collect function names from each header
foreach(hdr IN LISTS INPUTS)
  file(READ "${hdr}" txt)
  # Strip comments and condense whitespace
  string(REGEX REPLACE "/\\*[^*]*\\*+([^/*][^*]*\\*+)*/" "" txt "${txt}")
  string(REGEX REPLACE "[\r\n\t]+" " " txt "${txt}")

  # Match function declarations: el_action_t <name>(
  string(REGEX MATCHALL "el_action_t[ ]+([A-Za-z_][A-Za-z0-9_]*)[ ]*\\(" matches "${txt}")
  foreach(m ${matches})
    string(REGEX REPLACE "^el_action_t[ ]+" "" name "${m}")
    string(REGEX REPLACE "[ ]*\\(.*" "" name "${name}")
    list(APPEND funcs "${name}")
  endforeach()
endforeach()

# Sort for deterministic output
list(REMOVE_DUPLICATES funcs)
list(SORT funcs)

# Write in two columns
set(i 0)
foreach(f ${funcs})
  math(EXPR col "${i} % 2")
  if(col EQUAL 0)
    file(APPEND "${OUTPUT}" "    ${f},")
  else()
    file(APPEND "${OUTPUT}" "\t${f},\n")
  endif()
  math(EXPR i "${i} + 1")
endforeach()

# If odd number, end the line
math(EXPR mod "${i} % 2")
if(mod EQUAL 1)
  file(APPEND "${OUTPUT}" "\n")
endif()

file(APPEND "${OUTPUT}" "};\n")
