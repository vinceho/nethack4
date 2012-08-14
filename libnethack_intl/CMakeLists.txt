# libnethack_intl/CMakeLists.txt: build internationalisation libs

find_program(FLEX_EXECUTABLE NAMES flex flex.exe )
find_program(BISON_EXECUTABLE NAMES bison bison.exe )

set (LNHI_SRC ${NetHack4_SOURCE_DIR}/libnethack_intl)
set (LNHI_SRC_GEN ${NetHack4_BINARY_DIR}/libnethack_intl)

set (LIBNETHACKINTL_SRC lang-en.c)
set (LIBNETHACKINTL_GENERATED_SRC
     ${LNHI_SRC_GEN}/grammar.c
     ${LNHI_SRC_GEN}/grammar.h
     ${LNHI_SRC_GEN}/gramlex.c
    )

set_source_files_properties(${LIBNETHACKINTL_GENERATED_SRC} GENERATED)

add_custom_command(OUTPUT ${LNHI_SRC_GEN}/grammar.c
                          ${LNHI_SRC_GEN}/grammar.h
                   COMMAND ${BISON_EXECUTABLE}
                   ARGS -y --defines=${LNHI_SRC_GEN}/grammar.h
                        -o ${LNHI_SRC_GEN}/grammar.c
                        ${LNHI_SRC}/grammar.y
                   DEPENDS ${LNHI_SRC}/grammar.y)

add_custom_command(OUTPUT ${LNHI_SRC_GEN}/gramlex.c
                   COMMAND ${FLEX_EXECUTABLE}
                   ARGS -o${LNHI_SRC_GEN}/gramlex.c
                        ${LNHI_SRC}/gramlex.l
                   DEPENDS ${LNHI_SRC}/gramlex.l)

include_directories ( ${LNHI_SRC} ${LNHI_SRC_GEN} )

add_library(nethack_intl ${LIB_TYPE} ${LIBNETHACKINTL_SRC}
    ${LIBNETHACKINTL_GENERATED_SRC})

install (TARGETS nethack_intl
         DESTINATION ${LIBDIR})
