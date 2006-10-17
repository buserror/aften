# cmake project file by Prakash Punnoor
CMAKE_MINIMUM_REQUIRED(VERSION 2.4)

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules")
Project(Aften C)

INCLUDE(${CMAKE_ROOT}/Modules/TestBigEndian.cmake)
INCLUDE(${CMAKE_ROOT}/Modules/CheckCSourceCompiles.cmake)
INCLUDE(${CMAKE_MODULE_PATH}/ConfigHelper.cmake)
INCLUDE(${CMAKE_MODULE_PATH}/HeaderTests.cmake)
INCLUDE(${CMAKE_MODULE_PATH}/CompilerVisibility.cmake)

OPTION(DOUBLE "build Aften with doubles instead of floats" OFF)
IF(DOUBLE)
  ADD_DEFINE(CONFIG_DOUBLE)
ENDIF(DOUBLE)

INCLUDE_DIRECTORIES(${Aften_SOURCE_DIR}/)
INCLUDE_DIRECTORIES(${Aften_SOURCE_DIR}/libaften)
INCLUDE_DIRECTORIES(${Aften_SOURCE_DIR}/aften)

SET(LIBAFTEN_SRCS libaften/a52enc.c
                  libaften/bitalloc.c
                  libaften/bitio.c
                  libaften/crc.c
                  libaften/window.c
                  libaften/mdct.c
                  libaften/exponent.c
                  libaften/filter.c
                  libaften/util.c)

SET(AFTEN_SRCS aften/aften.c)

IF(CMAKE_COMPILER_IS_GNUCC)
  SET(CMAKE_C_FLAGS_RELEASE "-funroll-loops ${CMAKE_C_FLAGS_RELEASE}")
  SET(ADD_CFLAGS "-Wall ${ADD_CFLAGS}")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

IF(CMAKE_BUILD_TYPE MATCHES "")
  SET(CMAKE_BUILD_TYPE "Release")
ENDIF(CMAKE_BUILD_TYPE MATCHES "")

IF(UNIX)
  SET(LIBM "m")
ENDIF(UNIX)

TEST_BIG_ENDIAN(CMAKE_WORDS_BIGENDIAN)

IF(CMAKE_WORDS_BIGENDIAN)
  ADD_DEFINE(WORDS_BIGENDIAN)
ENDIF(CMAKE_WORDS_BIGENDIAN)

TEST_COMPILER_VISIBILITY()
ADD_DEFINITIONS(-DAFTEN_BUILD_LIBRARY)

CHECK_INCLUDE_FILE_DEFINE(inttypes.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE_DEFINE(byteswap.h HAVE_BYTESWAP_H)

GENERATE_CONFIG_H()

SET(CMAKE_C_FLAGS "${ADD_CFLAGS} ${CMAKE_C_FLAGS}")

ADD_LIBRARY(aften SHARED ${LIBAFTEN_SRCS})
TARGET_LINK_LIBRARIES(aften ${LIBM})

ADD_LIBRARY(aften_static STATIC ${LIBAFTEN_SRCS})
#SET_TARGET_PROPERTIES(aften_static PROPERTIES OUTPUT_NAME aften)
TARGET_LINK_LIBRARIES(aften_static ${LIBM})

#building a static lib to prevent recompilation of wav.c
ADD_LIBRARY(aften_wav STATIC aften/wav.c)

ADD_EXECUTABLE(aften_exe ${AFTEN_SRCS})
SET_TARGET_PROPERTIES(aften_exe PROPERTIES OUTPUT_NAME aften)
TARGET_LINK_LIBRARIES(aften_exe aften_wav aften_static)

ADD_EXECUTABLE(wavinfo util/wavinfo.c)
TARGET_LINK_LIBRARIES(wavinfo aften_wav)

ADD_EXECUTABLE(wavrms util/wavrms.c)
TARGET_LINK_LIBRARIES(wavrms aften_wav ${LIBM})

ADD_EXECUTABLE(wavfilter util/wavinfo.c libaften/filter.c)
TARGET_LINK_LIBRARIES(wavfilter aften_wav ${LIBM})

INSTALL(TARGETS aften aften_exe wavinfo wavrms wavfilter
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
