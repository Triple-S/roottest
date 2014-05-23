#-------------------------------------------------------------------------------
#
#  RootMacros.cmake
#
#  Macros and definitions regarding ROOT components.
#
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
#
# macro ROOT_CHECK_OUT_OF_SOURCE_BUILD
#
# Ensures that the project is built out-of-source.
#
#-------------------------------------------------------------------------------
macro(ROOT_CHECK_OUT_OF_SOURCE_BUILD)
 string(COMPARE EQUAL ${ROOTTEST_DIR} ${CMAKE_BINARY_DIR} insource)
  if(insource)
     file(REMOVE_RECURSE ${ROOTTEST_DIR}/Testing)
     file(REMOVE ${ROOTTEST_DIR}/DartConfiguration.tcl)

     message(FATAL_ERROR "ROOT should be installed as an out of source build,"
                         " to keep the source directory clean. Please create "
                         "a extra build directory and run the command 'cmake "
                         "<path_to_source_dir>' in this newly created "
                         "directory. You have also to delete the directory "
                         "CMakeFiles and the file CMakeCache.txt in the source "
                         "directory. Otherwise cmake will complain even if you "
                         "run it from an out-of-source directory.")
  endif()
endmacro()

#-------------------------------------------------------------------------------
#
# function ROOTTEEST_TARGETNAME_FROM_FILE( <resultvar> <filename>
#
# Construct a target name for a given file <filename> and store its name into
# <resultvar>. The target name is of the form:
#
#   roottest-<directorypath>-<filename_WE> 
#
#-------------------------------------------------------------------------------
function(ROOTTEST_TARGETNAME_FROM_FILE resultvar filename)
  get_filename_component(realfp ${filename} REALPATH)
  get_filename_component(filename_we ${filename} NAME_WE)

  string(REPLACE "${ROOTTEST_DIR}" "" relativepath ${realfp}) 
  string(REPLACE "${filename}"     "" relativepath ${relativepath})

  string(REPLACE "/" "-" targetname ${relativepath}${filename_we})
  set(${resultvar} "roottest${targetname}" PARENT_SCOPE)

endfunction()

#-------------------------------------------------------------------------------
#
# function ROOT_COMPILE_MACRO( <filename> [BUILDOBJ object] [BUILDLIB lib] )
#
# This function compiles and loads a shared library containing
# the code from the file <filename>.
#
#-------------------------------------------------------------------------------
function(ROOT_COMPILE_MACRO filename)
  CMAKE_PARSE_ARGUMENTS(ARG "" "BUILDOBJ;BUILDLIB;TARGETNAME;DEPENDS" ""  ${ARGN})

  # Add defines to root_cmd, in order to have out-of-source builds
  # when using the scripts/build.C macro.
  get_directory_property(DirDefs COMPILE_DEFINITIONS)

  foreach(d ${DirDefs} )
    list(APPEND RootExeDefines "-e;#define ${d}")
  endforeach()

  set(RootClingDefines
        -e "#define CMakeEnvironment"
        -e "#define CMakeBuildDir \"${CMAKE_CURRENT_BINARY_DIR}\""
        ${RootExeDefines})

  set(root_cmd root.exe ${RootClingDefines} -q -l -b)

  set(_cwd WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  get_filename_component(realfp ${filename} REALPATH)

  set(command ${root_cmd}
              ${ROOTTEST_DIR}/scripts/build.C\(\"${realfp}\",\"${ARG_BUILDLIB}\",\"${ARG_BUILDOBJ}\"\) ${_cwd})

  if(ARG_DEPENDS)
    set(deps ${ARG_DEPENDS})
  endif()

  ROOTTEST_TARGETNAME_FROM_FILE(targetname ${filename})

  set(targetname "${targetname}-compile-macro")

  add_custom_target(${targetname} COMMAND ${command} ${_cwd} ${deps} VERBATIM)

  add_dependencies(${targetname} ${ROOTTEST_LIB_DEPENDS})

endfunction(ROOT_COMPILE_MACRO)

#-------------------------------------------------------------------------------
#
# function ROOTTEST_GENERATE_DICTIONARY(<dictname> [LINKDEF linkdef]
#                                                  [DEPENDS deps]
#                                                  [files ...]      )
#
# This function generates a dictionary <dictname> from the provided <files>.
#
#-------------------------------------------------------------------------------
function(ROOTTEST_GENERATE_DICTIONARY dictname)
  CMAKE_PARSE_ARGUMENTS(ARG "" "" "LINKDEF;DEPENDS" ${ARGN})

  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  set(CMAKE_ROOTTEST_DICT ON)

  ROOT_GENERATE_DICTIONARY(${dictname} ${ARG_UNPARSED_ARGUMENTS} MODULE ${dictname} LINKDEF ${ARG_LINKDEF} DEPENDENCIES ${ARG_DEPENDS})
endfunction()

#-------------------------------------------------------------------------------
#
# function ROOT_REFLEX_GENERATE_DICTIONARY( <dictionary> [SELECTION sel...] [headerfiles...])
#
# This function generates a reflexion dictionary and creates a shared library.
#
#-------------------------------------------------------------------------------
function(ROOT_REFLEX_GENERATE_DICTIONARY dictionary)
  CMAKE_PARSE_ARGUMENTS(ARG "" "SELECTION" ""  ${ARGN})

  include_directories(${ROOT_INCLUDE_DIRS} ${ROOT_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
  link_directories(${ROOT_LIBRARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  set(CMAKE_ROOTTEST_DICT ON)

  set(ROOT_genreflex_cmd ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/genreflex)

  REFLEX_GENERATE_DICTIONARY(${dictionary} ${ARG_UNPARSED_ARGUMENTS} SELECTION ${ARG_SELECTION})

  ROOTTEST_TARGETNAME_FROM_FILE(targetname ${dictionary})

  # targetname_dictgen is the targetname constructed by the REFLEX_GENERATE_DICTIONARY
  # macro and is used as a dependency.
  set(targetname_dictgen ${targetname}-dictgen)

  set(targetname ${targetname}-genreflex)

  add_library(${targetname} MODULE ${gensrcdict})

  set_property(TARGET ${targetname} PROPERTY OUTPUT_NAME ${dictionary})

  add_dependencies(${targetname} ${ROOTTEST_LIB_DEPENDS} ${targetname_dictgen})

  target_link_libraries(${targetname} ${ARG_LIBRARIES} ${ROOT_Reflex_LIBRARY})

endfunction(ROOT_REFLEX_GENERATE_DICTIONARY)

#-------------------------------------------------------------------------------
#
# function ROOT_BUILD_DICT( <dictname> [FILES files...]
#
# Build a simple ROOT dictionary <dictname> from the input <files>.
#
#-------------------------------------------------------------------------------
function(ROOT_BUILD_DICT dictname)
  CMAKE_PARSE_ARGUMENTS(ARG "" "" "FILES" ${ARGN})

  foreach(f ${ARG_FILES})
    get_filename_component(realpath ${f} REALPATH)
    set(buildfiles ${buildfiles} "${realpath}")
  endforeach()

  set(command ${rootcint_program} -f ${dictname} -c ${buildfiles})

  ROOTTEST_TARGETNAME_FROM_FILE(targetname ${dictname})

  set(targetname "${targetname}-build-dict")

  add_custom_target(${targetname} COMMAND ${command} ${_cwd} VERBATIM)

  add_dependencies(${targetname} ${ROOTTEST_LIB_DEPENDS})

endfunction()

#-------------------------------------------------------------------------------
#
# function ROOT_BUILD_COMPILE_DICT( <dictname> [FILES files...]
#
# Build a ROOT dictionary <dictname> and compile it into a shared library
# from the input <files>.
#
#-------------------------------------------------------------------------------
function(ROOT_BUILD_COMPILE_DICT dictname)
  CMAKE_PARSE_ARGUMENTS(ARG "" "BUILDOBJ;BUILDLIB" "FILES" ${ARGN})

  get_directory_property(DirDefs COMPILE_DEFINITIONS)

  foreach(d ${DirDefs} )
    list(APPEND RootExeDefines "-e;#define ${d}")
  endforeach()

  # Setup root.exe command for batch mode.
  set(root_cmd root.exe ${RootExeDefines} -q -l -b)

  set(_cwd WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  foreach(f ${ARG_FILES})
    get_filename_component(realpath ${f} REALPATH)
    set(buildfiles ${buildfiles} "${realpath}")
  endforeach()

  set(cintcmd ${rootcint_program} -f ${ARG_BUILDOBJ} -c ${buildfiles})

  if(ARG_BUILDLIB)
    get_filename_component(_buildlib ${ARG_BUILDLIB} REALPATH)
  else()
    set(_buildlib "")
  endif()

  if(ARG_BUILDOBJ)
    get_filename_component(_buildobj ${ARG_BUILDOBJ} REALPATH)
  else()
    set(_buildobj "")
  endif()

  STRING(REGEX REPLACE " " ";" _flags ${CMAKE_CXX_FLAGS})
  set(_flags ${_flags} "-std=c++11;-I${ROOT_INCLUDE_DIRS};-I${ROOT_INCLUDE_DIR}")
  set(createobj_cmd  ${CMAKE_CXX_COMPILER} ${_flags} -c ${ARG_BUILDOBJ} -o ${ARG_BUILDOBJ}.o)

  string(RANDOM _rdm)
  set(_tmp "tmp${_rdm}")

  set(command ${root_cmd} '${ROOTTEST_DIR}/scripts/build.C\(\"${_tmp}.C\",\"${_buildlib}\",\"${ARG_BUILDOBJ}.o\"\)')

  add_custom_command(
    OUTPUT ${dictname}${libsuffix}
    COMMAND ${cintcmd}
    COMMAND ${createobj_cmd}
    COMMAND touch ${_tmp}.C ${_cwd}
    COMMAND ${command}
    COMMAND mv ${_tmp}_C${libsuffix} ${dictname}${libsuffix}
    ${_cwd})

  string(REPLACE "/" "-" srcpath "${CMAKE_CURRENT_SOURCE_DIR}")
  add_custom_target(${srcpath}-${dictname}-dict ALL DEPENDS ${dictname}${libsuffix})

endfunction()
