#pragma once
#include "windows.h"
#include <cstring>
using MMRESULT=UINT;inline constexpr MMRESULT MMSYSERR_NOERROR=0;inline constexpr UINT WAVE_FORMAT_PCM=1,WAVE_MAPPER=0xffffffff,CALLBACK_EVENT=1,WHDR_DONE=1;
struct WAVEFORMATEX{uint16_t wFormatTag=0,nChannels=0;DWORD nSamplesPerSec=0,nAvgBytesPerSec=0;uint16_t nBlockAlign=0,wBitsPerSample=0,cbSize=0;};
struct WAVEHDR{LPSTR lpData=nullptr;DWORD dwBufferLength=0,dwFlags=0;};struct FakeWave{HANDLE done;WAVEHDR* queued=nullptr;};using HWAVEOUT=FakeWave*;
inline std::mutex fakeAudioGate;inline std::atomic<bool> fakeHold{false},fakeFailOpen{false},fakeFailPrepare{false},fakeFailWrite{false};inline std::atomic<int> fakeWrites{0},fakeOpens{0},fakeCloses{0},fakeResets{0},fakePrepared{0};inline std::vector<int16_t> fakeLastSamples;
inline FakeWave* fakePlaying=nullptr;
inline void FakeCompletePlayback(){std::lock_guard lock(fakeAudioGate);assert(fakePlaying&&fakePlaying->queued);fakePlaying->queued->dwFlags|=WHDR_DONE;SetEvent(fakePlaying->done);}
inline MMRESULT waveOutOpen(HWAVEOUT* output,UINT device,WAVEFORMATEX* format,DWORD_PTR event,DWORD,UINT flags){assert(device==WAVE_MAPPER&&format->wFormatTag==WAVE_FORMAT_PCM&&format->nChannels==1&&format->nSamplesPerSec==44100&&format->wBitsPerSample==16&&flags==CALLBACK_EVENT);if(fakeFailOpen)return 1;*output=new FakeWave{reinterpret_cast<HANDLE>(event),nullptr};fakeOpens++;return 0;}
inline MMRESULT waveOutPrepareHeader(HWAVEOUT,WAVEHDR*,UINT size){assert(size==sizeof(WAVEHDR));if(fakeFailPrepare)return 1;fakePrepared++;return 0;}
inline MMRESULT waveOutWrite(HWAVEOUT device,WAVEHDR* header,UINT size){assert(size==sizeof(WAVEHDR)&&header->lpData&&header->dwBufferLength>0);if(fakeFailWrite)return 1;std::lock_guard lock(fakeAudioGate);fakeLastSamples.resize(header->dwBufferLength/2);std::memcpy(fakeLastSamples.data(),header->lpData,header->dwBufferLength);fakeWrites++;device->queued=header;fakePlaying=device;if(!fakeHold){header->dwFlags|=WHDR_DONE;SetEvent(device->done);}return 0;}
inline MMRESULT waveOutReset(HWAVEOUT device){std::lock_guard lock(fakeAudioGate);fakeResets++;if(device->queued){device->queued->dwFlags|=WHDR_DONE;SetEvent(device->done);}return 0;}
inline MMRESULT waveOutUnprepareHeader(HWAVEOUT device,WAVEHDR* header,UINT){std::lock_guard lock(fakeAudioGate);if(device->queued==header&&!(header->dwFlags&WHDR_DONE))return 1;if(device->queued==header)device->queued=nullptr;if(fakePlaying==device)fakePlaying=nullptr;fakePrepared--;return 0;}
inline MMRESULT waveOutClose(HWAVEOUT device){assert(!device->queued);fakeCloses++;delete device;return 0;}
