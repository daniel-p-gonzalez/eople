#pragma once
//
// -http://stackoverflow.com/questions/16299029/resolution-of-stdchronohigh-resolution-clock-doesnt-correspond-to-measureme
//

namespace Eople
{

#ifdef WIN32
struct HighResClock
{
  typedef long long                               rep;
  typedef std::nano                               period;
  typedef std::chrono::duration<rep, period>      duration;
  typedef std::chrono::time_point<HighResClock>   time_point;
  static const bool is_steady = true;
  static const long long frequency;

  static time_point now();
};
#else
typedef std::chrono::high_resolution_clock HighResClock;
#endif

} // namespace Eople
