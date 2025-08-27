#!/bin/bash
find ./tests -iname *.h* -o -iname *.c* | xargs clang-format -i -verbose
find ./daemon -iname *.h* -o -iname *.c* | xargs clang-format -i -verbose
find ./shim -iname *.h* -o -iname *.c* | xargs clang-format -i -verbose

