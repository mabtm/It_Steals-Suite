#pragma once
#include "../hud_stubs/windows.h"
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
using HANDLE=void*;using DWORD_PTR=uintptr_t;using LPSTR=char*;
inline constexpr BOOL FALSE=0,TRUE=1;inline constexpr DWORD INFINITE=0xffffffff,WAIT_OBJECT_0=0,WAIT_TIMEOUT=258;inline constexpr int THREAD_PRIORITY_BELOW_NORMAL=-1;
struct FakeEvent{bool signal=false,manual=false;};inline std::mutex fakeEventGate;inline std::condition_variable fakeEventWake;inline std::atomic<int> fakeHandles{0};
inline HANDLE CreateEventW(void*,BOOL manual,BOOL initial,const wchar_t*){fakeHandles++;return new FakeEvent{initial!=0,manual!=0};}
inline BOOL SetEvent(HANDLE raw){{std::lock_guard lock(fakeEventGate);static_cast<FakeEvent*>(raw)->signal=true;}fakeEventWake.notify_all();return TRUE;}
inline BOOL ResetEvent(HANDLE raw){std::lock_guard lock(fakeEventGate);static_cast<FakeEvent*>(raw)->signal=false;return TRUE;}
inline BOOL CloseHandle(HANDLE raw){fakeHandles--;delete static_cast<FakeEvent*>(raw);return TRUE;}
inline HANDLE GetCurrentThread(){return nullptr;}inline BOOL SetThreadPriority(HANDLE,int){return TRUE;}
inline DWORD WaitForMultipleObjects(DWORD count,HANDLE* events,BOOL all,DWORD timeout){assert(!all);std::unique_lock lock(fakeEventGate);auto ready=[&]{for(DWORD i=0;i<count;i++)if(static_cast<FakeEvent*>(events[i])->signal)return true;return false;};if(timeout==INFINITE)fakeEventWake.wait(lock,ready);else if(!fakeEventWake.wait_for(lock,std::chrono::milliseconds(timeout),ready))return WAIT_TIMEOUT;for(DWORD i=0;i<count;i++){auto* e=static_cast<FakeEvent*>(events[i]);if(e->signal){if(!e->manual)e->signal=false;return WAIT_OBJECT_0+i;}}return WAIT_TIMEOUT;}
inline DWORD WaitForSingleObject(HANDLE event,DWORD timeout){return WaitForMultipleObjects(1,&event,FALSE,timeout);}
