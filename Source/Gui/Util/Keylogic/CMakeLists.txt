add_library(keylogic STATIC keylogic.c)

target_link_libraries(waechter PRIVATE keylogic)