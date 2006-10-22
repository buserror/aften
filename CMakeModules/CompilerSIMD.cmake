MACRO(CHECK_MM_MALLOC)
CHECK_C_SOURCE_COMPILES(
"#include <xmmintrin.h>
int main(){_mm_malloc(1024,16);}
" HAVE_MM_MALLOC)

IF(NOT HAVE_MM_MALLOC)
  ADD_DEFINE(EMU_MM_MALLOC)
ENDIF(NOT HAVE_MM_MALLOC)
ENDMACRO(CHECK_MM_MALLOC)

MACRO(CHECK_CASTSI128)
CHECK_C_SOURCE_COMPILES(
"#include <pmmintrin.h>
int main(){__m128i X; _mm_castsi128_ps(X);}
" HAVE_CASTSI128)

IF(NOT HAVE_CASTSI128)
  ADD_DEFINE(EMU_CASTSI128)
ENDIF(NOT HAVE_CASTSI128)
ENDMACRO(CHECK_CASTSI128)