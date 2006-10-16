MACRO(TEST_COMPILER_VISIBILITY)
IF(NOT WIN32)
  SET(CMAKE_REQUIRED_FLAGS "-fvisibility=hidden")
  CHECK_C_SOURCE_COMPILES(
"void __attribute__((visibility(\"default\"))) test() {}
#ifdef __INTEL_COMPILER
#error ICC breaks with binutils and visibility
#endif
int main(){}
" HAVE_VISIBILITY)
  SET(CMAKE_REQUIRED_FLAGS "")

  IF(HAVE_VISIBILITY)
    SET(ADD_CFLAGS "${ADD_CFLAGS} -fvisibility=hidden")
    ADD_DEFINE(HAVE_GCC_VISIBILITY)
  ENDIF(HAVE_VISIBILITY)
ENDIF(NOT WIN32)
ENDMACRO(TEST_COMPILER_VISIBILITY)
