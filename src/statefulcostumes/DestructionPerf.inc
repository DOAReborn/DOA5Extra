struct DestructionPerfScope {
 const char* label; LARGE_INTEGER start{}; unsigned slot;
 DestructionPerfScope(const char* name,unsigned index):label(name),slot(index){QueryPerformanceCounter(&start);}
 ~DestructionPerfScope(){
  LARGE_INTEGER end{},frequency{};QueryPerformanceCounter(&end);QueryPerformanceFrequency(&frequency);
  if(frequency.QuadPart<=0)return;
  struct Stats {ULONGLONG since{};double total{},maximum{};unsigned calls{};};
  static thread_local Stats stats[3]{};
  auto& s=stats[slot];const ULONGLONG now=GetTickCount64();
  if(!s.since)s.since=now;
  const double ms=1000.0*double(end.QuadPart-start.QuadPart)/double(frequency.QuadPart);
  s.total+=ms;if(ms>s.maximum)s.maximum=ms;++s.calls;
  if(slot==0||now-s.since>=1000){
   char line[200];std::snprintf(line,sizeof line,"Destruction CPU timing %s thread=%lu calls=%u total_ms=%.3f max_ms=%.3f window_ms=%llu",label,GetCurrentThreadId(),s.calls,s.total,s.maximum,static_cast<unsigned long long>(now-s.since));Queue(line);
   s={};
  }
 }
};
