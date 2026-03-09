#pragma once

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

static inline void chsp_randomize( void )
{
    srand( (unsigned int)time( NULL ) );
}

static inline void chsp_randomize_seed( int seed )
{
    srand( (unsigned int)seed );
}

static inline int chsp_rnd( int limit )
{
    if ( limit <= 0 ) {
        return 0;
    }
    return rand() % limit;
}

static inline int chsp_hsp_int( double value )
{
    return (int)value;
}

static inline double chsp_hsp_double( double value )
{
    return value;
}

static inline int chsp_hsp_abs( int value )
{
    return abs( value );
}

static inline double chsp_hsp_absf( double value )
{
    return fabs( value );
}

static inline double chsp_hsp_sin( double value )
{
    return sin( value );
}

static inline double chsp_hsp_cos( double value )
{
    return cos( value );
}

static inline double chsp_hsp_tan( double value )
{
    return tan( value );
}

static inline double chsp_hsp_atan( double value )
{
    return atan( value );
}

static inline double chsp_hsp_sqrt( double value )
{
    return sqrt( value );
}

static inline double chsp_hsp_expf( double value )
{
    return exp( value );
}

static inline double chsp_hsp_logf( double value )
{
    return log( value );
}

static inline double chsp_hsp_powf( double lhs, double rhs )
{
    return pow( lhs, rhs );
}

static inline int chsp_hsp_limit( int value, int min_value, int max_value )
{
    if ( value < min_value ) {
        return min_value;
    }
    if ( value > max_value ) {
        return max_value;
    }
    return value;
}

static inline double chsp_hsp_limitf( double value, double min_value, double max_value )
{
    if ( value < min_value ) {
        return min_value;
    }
    if ( value > max_value ) {
        return max_value;
    }
    return value;
}
