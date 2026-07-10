add_executable(waechterd
        Waechterd.cpp
        DaemonConfig.cpp
        DaemonConfig.hpp
        Daemon.cpp
        Daemon.hpp
        MemoryUsage.cpp
        MemoryUsage.hpp
)


target_compile_options(waechterd PRIVATE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wformat=2 -Wconversion -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion -Wcast-align
        -Wduplicated-cond -Wredundant-decls -Wpointer-arith
        -Werror
)

if (WAECHTER_WITH_WEBSOCKETSERVER)
    target_compile_definitions(waechterd PRIVATE WAECHTER_WITH_WEBSOCKETSERVER)
    # find libwebsockets
    find_package(LibWebSockets REQUIRED)
    target_include_directories(waechterd PRIVATE ${LIBWEBSOCKETS_INCLUDE_DIRS})
    target_link_libraries(waechterd PRIVATE ${LIBWEBSOCKETS_LIBRARIES})
endif ()

include(FindPkgConfig)
pkg_check_modules(LIBBPF REQUIRED libbpf)
find_package(SQLite3 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(CURL REQUIRED)

target_include_directories(waechterd PRIVATE ${LIBBPF_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(waechterd PRIVATE ${LIBBPF_LIBRARIES}
        CURL::libcurl ZLIB::ZLIB waechter::util thirdparty::spdlog thirdparty::mini thirdparty::json11 thirdparty::cereal thirdparty::deps_includes
        thirdparty::tracy thirdparty::sqlpp11
        ebpf_includes
)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "4.3.2")
    target_link_libraries(waechterd PRIVATE SQLite3::SQLite3)
else ()
    target_link_libraries(waechterd PRIVATE SQLite::SQLite3)
endif ()

add_dependencies(waechterd bpfobj)

add_subdirectory(Net)
add_subdirectory(Data)
add_subdirectory(Db)
add_subdirectory(Communication)
add_subdirectory(Rules)
add_subdirectory(EBPF)

target_compile_definitions(waechterd PRIVATE
        $<$<CONFIG:Debug>:WDEBUG>
)

install(TARGETS waechterd
        RUNTIME DESTINATION bin)

set(WAECHTER_MIGRATIONS_SOURCE_DIR ${CMAKE_SOURCE_DIR}/Meta/Database/Migrations)
set(WAECHTER_MIGRATIONS_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/migrations)
set(WAECHTER_MIGRATIONS_STAMP ${CMAKE_CURRENT_BINARY_DIR}/migrations.stamp)

file(GLOB CONFIGURE_DEPENDS WAECHTER_MIGRATION_FILES
        ${WAECHTER_MIGRATIONS_SOURCE_DIR}/*.sql)

add_custom_command(
        OUTPUT ${WAECHTER_MIGRATIONS_STAMP}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${WAECHTER_MIGRATIONS_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${WAECHTER_MIGRATIONS_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${WAECHTER_MIGRATIONS_SOURCE_DIR}
                ${WAECHTER_MIGRATIONS_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E touch ${WAECHTER_MIGRATIONS_STAMP}
        DEPENDS ${WAECHTER_MIGRATION_FILES}
        COMMENT "Copying database migrations"
)

add_custom_target(copy_waechterd_migrations DEPENDS ${WAECHTER_MIGRATIONS_STAMP})
add_dependencies(waechterd copy_waechterd_migrations)
