MACRO(CHECK_MM_MALLOC)
SET(CMAKE_REQUIRED_FLAGS "${SSE_FLAGS}")
CHECK_C_SOURCE_COMPILES(
"#include <xmmintrin.h>
int main(){_mm_malloc(1024,16);}
" HAVE_MM_MALLOC)
SET(CMAKE_REQUIRED_FLAGS "")

IF(NOT HAVE_MM_MALLOC)
  ADD_DEFINE(EMU_MM_MALLOC)
ENDIF(NOT HAVE_MM_MALLOC)
ENDMACRO(CHECK_MM_MALLOC)

MACRO(CHECK_CASTSI128)
SET(CMAKE_REQUIRED_FLAGS "${SSE3_FLAGS}")
CHECK_C_SOURCE_COMPILES(
"#include <pmmintrin.h>
int main(){__m128i X; _mm_castsi128_ps(X);}
" HAVE_CASTSI128)
SET(CMAKE_REQUIRED_FLAGS "")

IF(NOT HAVE_CASTSI128)
  ADD_DEFINE(EMU_CASTSI128)
ENDIF(NOT HAVE_CASTSI128)
ENDMACRO(CHECK_CASTSI128)

MACRO(CHECK_SSE)
IF(CMAKE_COMPILER_IS_GNUCC)
  SET(SSE_FLAGS "-mmmx -msse")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

SET(CMAKE_REQUIRED_FLAGS "${SSE_FLAGS}")
CHECK_C_SOURCE_COMPILES(
"#include <xmmintrin.h>
int main(){__m128 X = _mm_setzero_ps();}
" HAVE_SSE)
SET(CMAKE_REQUIRED_FLAGS "")
ENDMACRO(CHECK_SSE)

MACRO(CHECK_SSE2)
IF(CMAKE_COMPILER_IS_GNUCC)
  SET(SSE2_FLAGS "-mmmx -msse -msse2")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

SET(CMAKE_REQUIRED_FLAGS "${SSE2_FLAGS}")
CHECK_C_SOURCE_COMPILES(
"#include <emmintrin.h>
int main() {
__m128i X = _mm_setzero_si128();
__m128i Y = _mm_unpackhi_epi8(X, X);
}
" HAVE_SSE2)
SET(CMAKE_REQUIRED_FLAGS "")
ENDMACRO(CHECK_SSE2)

MACRO(CHECK_SSE3)
IF(CMAKE_COMPILER_IS_GNUCC)
  SET(SSE3_FLAGS "-mmmx -msse -msse2 -msse3")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

SET(CMAKE_REQUIRED_FLAGS "${SSE3_FLAGS}")
CHECK_C_SOURCE_COMPILES(
"#include <pmmintrin.h>
int main() {
__m128 X = _mm_setzero_ps();
__m128 Y = _mm_movehdup_ps(X);
}
" HAVE_SSE3)
SET(CMAKE_REQUIRED_FLAGS "")
ENDMACRO(CHECK_SSE3)
