# Compile each public header on its own so umbrella includes cannot hide missing dependencies.
file(GLOB MNGL_ISOLATED_HEADERS CONFIGURE_DEPENDS
	"${PROJECT_SOURCE_DIR}/MNGL/*.h"
	"${PROJECT_SOURCE_DIR}/MNGL/*.hpp"
	"${PROJECT_SOURCE_DIR}/MNGL/extras/*.hpp")
set(MNGL_HEADER_CHECK_SOURCES)
foreach(header IN LISTS MNGL_ISOLATED_HEADERS)
	file(RELATIVE_PATH relative_header "${PROJECT_SOURCE_DIR}" "${header}")
	string(REPLACE "/" "_" source_name "${relative_header}")
	set(source "${PROJECT_BINARY_DIR}/header-checks/${source_name}.cpp")
	file(GENERATE OUTPUT "${source}" CONTENT "#include <${relative_header}>\n")
	list(APPEND MNGL_HEADER_CHECK_SOURCES "${source}")
endforeach()
add_library(mngl_header_isolation OBJECT ${MNGL_HEADER_CHECK_SOURCES})
target_link_libraries(mngl_header_isolation PRIVATE mngl)
if(MSVC)
	target_compile_options(mngl_header_isolation PRIVATE /utf-8 /MP4)
endif()
