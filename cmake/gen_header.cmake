# Inputs: -DINPUT=... -DOUTPUT=...
get_filename_component(base "${INPUT}" NAME)
string(REPLACE "." "_" guard_suffix "${base}")

file(READ "${INPUT}" src)

# Collapse newlines and tabs to spaces
string(REGEX REPLACE "[\r\n\t]+" " " src "${src}")

# Remove block comments (/* ... */)
string(REGEX REPLACE "/\\*[^*]*\\*+([^/*][^*]*\\*+)*/" "" src "${src}")

# Normalize multiple spaces
string(REGEX REPLACE " +" " " src "${src}")

# Merge any split 'libedit_private el_action_t' sequences so we can match them safely
# e.g. "libedit_private el_action_t vi_insert" or with comments previously removed
# becomes "libedit_private el_action_t vi_insert"
# Find all names after "libedit_private el_action_t"
string(REGEX MATCHALL "libedit_private el_action_t[ ]+[A-Za-z_][A-Za-z0-9_]*[ ]*\\(" matches "${src}")

set(lines "")
foreach(m ${matches})
  # Extract just the function name
  string(REGEX REPLACE "^libedit_private el_action_t[ ]+" "" name "${m}")
  string(REGEX REPLACE "[ ]*\\(.*" "" name "${name}")
  string(STRIP "${name}" name)
  string(APPEND lines "libedit_private el_action_t ${name}(EditLine *, wint_t);\n")
endforeach()

file(WRITE "${OUTPUT}" "/* Automatically generated file, do not edit */\n")
file(APPEND "${OUTPUT}" "#ifndef _h_${guard_suffix}\n#define _h_${guard_suffix}\n")
file(APPEND "${OUTPUT}" "${lines}")
file(APPEND "${OUTPUT}" "#endif /* _h_${guard_suffix} */\n")
