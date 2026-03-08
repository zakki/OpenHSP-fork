#pragma once

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <ctime>
#ifdef HSPRANDMT
#include <random>
#endif

namespace chsp
{

#ifdef HSPRANDMT
inline std::mt19937 g_mt;
#endif

inline void randomize()
{
	const auto seed = static_cast<unsigned int>( std::time( nullptr ) );
#ifdef HSPRANDMT
	g_mt.seed( static_cast<std::mt19937::result_type>( seed ) );
#else
	std::srand( seed );
#endif
}

inline void randomize( int seed )
{
#ifdef HSPRANDMT
	g_mt.seed( static_cast<std::mt19937::result_type>( seed ) );
#else
	std::srand( seed );
#endif
}

inline int rnd( int limit )
{
	if ( limit <= 0 ) {
		return 0;
	}
#ifdef HSPRANDMT
	std::uniform_int_distribution<int> dist( 0, limit - 1 );
	return dist( g_mt );
#else
	return std::rand() % limit;
#endif
}

inline int hsp_int( int value )
{
	return value;
}

inline int hsp_int( double value )
{
	return static_cast<int>( value );
}

inline double hsp_double( int value )
{
	return static_cast<double>( value );
}

inline double hsp_double( double value )
{
	return value;
}

inline int hsp_abs( int value )
{
	return std::abs( value );
}

inline double hsp_absf( double value )
{
	return std::fabs( value );
}

inline double hsp_sin( double value )
{
	return std::sin( value );
}

inline double hsp_cos( double value )
{
	return std::cos( value );
}

inline double hsp_tan( double value )
{
	return std::tan( value );
}

inline double hsp_atan( double value )
{
	return std::atan( value );
}

inline double hsp_sqrt( double value )
{
	return std::sqrt( value );
}

inline double hsp_expf( double value )
{
	return std::exp( value );
}

inline double hsp_logf( double value )
{
	return std::log( value );
}

inline double hsp_powf( double lhs, double rhs )
{
	return std::pow( lhs, rhs );
}

template <typename T>
inline T hsp_limit( T value, T min_value, T max_value )
{
	return std::min( std::max( value, min_value ), max_value );
}

inline int hsp_limit( int value, int min_value, int max_value )
{
	return hsp_limit<int>( value, min_value, max_value );
}

inline double hsp_limitf( double value, double min_value, double max_value )
{
	return hsp_limit<double>( value, min_value, max_value );
}

} // namespace chsp
