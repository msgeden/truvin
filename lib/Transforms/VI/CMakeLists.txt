# If we don't need RTTI or EH, there's no reason to export anything
# from the hello plugin.
if( NOT LLVM_REQUIRES_RTTI )
  if( NOT LLVM_REQUIRES_EH )
    set(LLVM_EXPORTED_SYMBOL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/Custom.exports)
  endif()
endif()

if(WIN32 OR CYGWIN)
  set(LLVM_LINK_COMPONENTS Core Support)
endif()

add_llvm_library( LLVMVI MODULE BUILDTREE_ONLY
	Alias.cpp
	CFA.cpp
	TVI.cpp
	FVI.cpp
  	Utils.cpp
	Variables.cpp

	ADDITIONAL_HEADER_DIRS
    ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/VI/

  	DEPENDS
  	intrinsics_gen
  	PLUGIN_TOOL
  	opt
  )
