# BSD LibEdit for Windows

This is  a port of BSD  libedit-20250104-3.1 for Windows.  The  aim of
this port is to have a  fairly lightweight Windows-native port of this
library to  support SWI-Prolog.  We  notably want to  support multiple
instances on behalf of the upcoming XPCE based `swipl-win`.

Main features:

  - Compile using MinGW
  - Use Windows HANDLE to connect a Windows console
  - Use Windows Console API to read/write the console

Issues

  - No support for file completion yet.
  - No regex support for history search yet.

Plans

  - Possibly strip a lot of the functionality?
  - Add CMake build?
