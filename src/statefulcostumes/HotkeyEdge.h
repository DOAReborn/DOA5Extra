#pragma once
struct HotkeyEdge {
 bool armed=false;
 bool Update(bool focused,bool valid,bool down) noexcept {
  if(!focused||!valid){armed=false;return false;}
  if(!down){armed=true;return false;}
  if(!armed)return false;
  armed=false;return true;
 }
};
