#pragma once
#include <algorithm>
#include <cmath>
namespace ZoneElevator
{
inline constexpr int kMetalworksZone = 237;
inline constexpr float kPeriod = 18.25f, kTravelTime = 8.0f;
inline constexpr float kBottomY = -13.10f, kTopY = 2.00f;
inline constexpr float kPlatformX = -56.0f, kPlatformZ = 0.0f;
inline constexpr float kPlatformHalfWidth = 3.5f, kPlatformHalfDepth = 15.0f;
struct State { float elapsed=0.0f, lastY=kBottomY; };
inline bool Update(State& s,float dt,float p[3],bool& ground){if(!p||dt<=0)return false;s.elapsed+=dt;while(s.elapsed>=kPeriod)s.elapsed-=kPeriod;float h=kPeriod*.5f,t=s.elapsed<h?s.elapsed:s.elapsed-h,f=std::min(1.0f,t/kTravelTime),y=s.elapsed<h?kBottomY+(kTopY-kBottomY)*f:kTopY-(kTopY-kBottomY)*f,dy=y-s.lastY;bool r=p[0]>=kPlatformX-kPlatformHalfWidth&&p[0]<=kPlatformX+kPlatformHalfWidth&&p[2]>=kPlatformZ-kPlatformHalfDepth&&p[2]<=kPlatformZ+kPlatformHalfDepth&&std::fabs(p[1]-s.lastY)<1.5f;if(r){p[1]+=dy;ground=true;}s.lastY=y;return r&&dy!=0;}
}
