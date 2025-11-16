# Usage:
#   cmake -DOUTPUT=... -DINPUTS="vi.h;emacs.h;common.h" -P gen_fcns.cmake

if(NOT DEFINED OUTPUT)
  message(FATAL_ERROR "gen_fcns.cmake: OUTPUT not set")
endif()
if(NOT DEFINED INPUTS)
  message(FATAL_ERROR "gen_fcns.cmake: INPUTS not set")
endif()

set(all_names)

foreach(hdr IN LISTS INPUTS)
  file(READ "${hdr}" txt)

  # Normalize whitespace and strip block comments (headers should be clean already,
  # but do it anyway for safety).
  string(REGEX REPLACE "[\r\n\t]+" " " txt "${txt}")
  string(REGEX REPLACE "/\\*[^*]*\\*+([^/*][^*]*\\*+)*/" "" txt "${txt}")

  # Insert unique markers right after 'el_action_t ' and before '('.
  # This makes it easy to grab just the function name, without relying on MATCHALL captures.
  # Example: "el_action_t   ed_insert  (" -> "@@ed_insert@@("
  string(REGEX REPLACE "el_action_t[ ]+([A-Za-z_][A-Za-z0-9_]*)[ ]*\\(" "@@\\1@@(" txt "${txt}")

  # Now collect all marked names: @@NAME@@
  string(REGEX MATCHALL "@@[A-Za-z_][A-Za-z0-9_]*@@" marked "${txt}")

  foreach(m IN LISTS marked)
    # Strip the @@ delimiters
    string(REGEX REPLACE "^@@" "" name "${m}")
    string(REGEX REPLACE "@@$" "" name "${name}")
    list(APPEND all_names "${name}")
  endforeach()
endforeach()

# Deduplicate and sort
list(REMOVE_DUPLICATES all_names)
list(SORT all_names)

# Uppercase
set(names_upper)
foreach(n IN LISTS all_names)
  string(TOUPPER "${n}" u)
  list(APPEND names_upper "${u}")
endforeach()

# Emit header
file(WRITE "${OUTPUT}" "/* Automatically generated file, do not edit */\n")

# Align like awk printf("%-30.30s %3d")
set(space_pad "                              ") # 30 spaces
set(i 0)
foreach(u IN LISTS names_upper)
  string(LENGTH "${u}" L)
  math(EXPR remain "30 - ${L}")
  if(remain LESS 0)
    set(remain 0)
  endif()
  string(SUBSTRING "${space_pad}" 0 ${remain} pad)
  file(APPEND "${OUTPUT}" "#define\t${u}${pad}\t${i}\n")
  math(EXPR i "${i} + 1")
endforeach()

file(APPEND "${OUTPUT}" "#define\tEL_NUM_FCNS                  \t${i}\n")
