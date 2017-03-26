#ifndef UTILS_H_
#define UTILS_H_

#include <iostream>
#include <stdlib.h>

template <typename T>
void VERIFY( T x, const char* failMsg )
{
   if ( !x )
   {
      char c;
      std::cerr << failMsg << std::endl;
      std::cin >> c;
      exit( EXIT_FAILURE );
   }
}

template <typename T>
T clamp( T val, T min, T max )
{
   const T temp = val < min ? min : val;
   return temp > max ? max : temp;
}

constexpr size_t kilobytes( size_t count )
{
   return count * 1024;
}

constexpr size_t megabytes( size_t count )
{
   return kilobytes( count ) * 1024;
}

constexpr size_t gigabytes( size_t count )
{
   return megabytes( count ) * 1024;
}

#endif  // UTILS_H_