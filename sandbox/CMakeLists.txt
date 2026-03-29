add_executable(sandbox)

target_sources(sandbox PRIVATE
	"sandbox.cpp"
	"sandboxPch.h"
)
set_target_properties(sandbox PROPERTIES CXX_STANDARD 26)
target_compile_options(sandbox PRIVATE /Wall)
target_precompile_headers(sandbox PUBLIC "sandboxPch.h")


target_link_libraries(sandbox PRIVATE
	czcore
)

