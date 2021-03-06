#include <complex.h>
#include <float.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "arg_ranges.h"

#ifdef MP_PREC
#include "ext_precision.h"

#include <mpfr.h>
#include <mpc.h>
#endif


/* Range of permissible complex numbers */
const complex COMPLEX_MIN = -10.0 - 10.0 * I;
const complex COMPLEX_MAX = 10.0 + 10.0 * I;

/* Range of permissible complex numbers (extended-precision) */
const long double complex COMPLEX_MIN_EXT = -10.0L - 10.0L * I;
const long double complex COMPLEX_MAX_EXT = 10.0L + 10.0L * I;

/* Range of permissible constant values */
const complex C_MIN = -2.0 - 2.0 * I;
const complex C_MAX = 2.0 + 2.0 * I;

/* Range of permissible constant values (extended-precision) */
const long double complex C_MIN_EXT = -2.0L - 2.0L * I;
const long double complex C_MAX_EXT = 2.0L + 2.0L * I;

#ifdef MP_PREC
/* Range of permissible constant values (multiple-precision) */
mpc_t C_MIN_MP;
mpc_t C_MAX_MP;
static const long double complex C_MIN_MP_ = -2.0L - 2.0L * I;
static const long double complex C_MAX_MP_ = 2.0L + 2.0L * I;
#endif

/* Range of permissible magnification values */
const double MAGNIFICATION_MIN = -256.0;
const double MAGNIFICATION_MAX = DBL_MAX;

/* Range of permissible magnification values (extended-precision) */
const long double MAGNIFICATION_MIN_EXT = -256.0L;
const long double MAGNIFICATION_MAX_EXT = LDBL_MAX;

/* Range of permissible iteration counts */
const unsigned long ITERATIONS_MIN = 0UL;
const unsigned long ITERATIONS_MAX = ULONG_MAX;

/* Range of permissible dimensions */
const size_t WIDTH_MIN = 1;
const size_t WIDTH_MAX = SIZE_MAX;
const size_t HEIGHT_MIN = 1;
const size_t HEIGHT_MAX = SIZE_MAX;

const uint16_t PORT_MIN = 1024;
const uint16_t PORT_MAX = 65534;

const int WORKERS_MIN = 1;
const int WORKERS_MAX = 32;

#ifdef MP_PREC
/* Range of permissible precisions (multiple-precision) */
const mpfr_prec_t MP_BITS_DEFAULT = 128;
const mpfr_prec_t MP_BITS_MIN = 1;
const mpfr_prec_t MP_BITS_MAX = 16384;


/* Initialise multiple-precision argument ranges */
void initialiseArgRangesMP(void)
{
    mpc_init2(C_MIN_MP, mpSignificandSize);
    mpc_init2(C_MAX_MP, mpSignificandSize);

    mpc_set_ldc(C_MIN_MP, C_MIN_MP_, MP_COMPLEX_RND);
    mpc_set_ldc(C_MAX_MP, C_MAX_MP_, MP_COMPLEX_RND);
}

/* Free multiple-precision argument ranges */
void freeArgRangesMP(void)
{
    mpc_clear(C_MIN_MP);
    mpc_clear(C_MAX_MP);
}
#endif