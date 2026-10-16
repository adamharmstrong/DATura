#include "zone_environment_state.h"
#include "d3d_model_render_state.h"
#include <cmath>
#include <iostream>
#include <limits>
int main() {
 int failures=0,checks=0;
 auto check=[&](bool value,const char* text){++checks;if(!value){++failures;std::cerr<<text<<'\n';}};
 ZoneEnvironmentState::SetTimeOverride(721);
 check(ZoneEnvironmentState::CurrentMinuteOfDay()==721,"integer override");
 check(ZoneEnvironmentState::CurrentMinuteOfDayPrecise()==721.0,"fractional override exact");
 ZoneEnvironmentState::SetTimeOverride(-1);
 check(ZoneEnvironmentState::CurrentMinuteOfDay()==1439,"integer negative override wraps");
 check(ZoneEnvironmentState::CurrentMinuteOfDayPrecise()==1439.0,"fractional negative override wraps");
 ZoneEnvironmentState::ClearTimeOverride();
 for(bool local:{false,true}) {
  ZoneEnvironmentState::SetUseLocalSystemTime(local);
  const int before=ZoneEnvironmentState::CurrentMinuteOfDay();
  const double precise=ZoneEnvironmentState::CurrentMinuteOfDayPrecise();
  const int after=ZoneEnvironmentState::CurrentMinuteOfDay();
  check(precise>=0 && precise<1440,"clock stays within day");
  check(static_cast<int>(precise)==before || static_cast<int>(precise)==after,"fractional clock matches integer epoch/timezone");
 }
 ZoneEnvironmentState::SetUseLocalSystemTime(false);
 auto direction=[](float minute,float out[3]){D3DModelRenderState::CalculateDirectionalLight(minute,true,0,0,out);};
 float a[3]={},b[3]={},c[3]={};
 direction(720,a);direction(720.25f,b);direction(720.5f,c);
 float change1=0,change2=0;
 for(int i=0;i<3;++i){change1+=std::abs(a[i]-b[i]);change2+=std::abs(b[i]-c[i]);}
 check(change1>0 && change1<.002f,"subminute light advances gradually");
 check(change2>0 && change2<.002f,"consecutive subminute light advances gradually");
 check(std::abs(std::sqrt(b[0]*b[0]+b[1]*b[1]+b[2]*b[2])-1)<1e-6f,"subminute direction normalized");
 direction(-.25f,a);direction(1439.75f,b);
 for(int i=0;i<3;++i)check(std::abs(a[i]-b[i])<1e-6f,"negative fractional minute wraps");
 direction(1440.25f,a);direction(.25f,b);
 for(int i=0;i<3;++i)check(std::abs(a[i]-b[i])<1e-6f,"overflow fractional minute wraps");
 direction(std::numeric_limits<float>::quiet_NaN(),a);
 for(float v:a)check(std::isfinite(v),"invalid minute finite fallback");
 std::cout<<checks<<" fractional clock/direction checks, "<<failures<<" failures\n";
 return failures?1:0;
}