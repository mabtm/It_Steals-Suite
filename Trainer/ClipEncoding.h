#pragma once
#include "ClipAudio.h"
#include "GifWriter.h"
#include "PngSequence.h"
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#ifdef _WIN32
#include "LosslessProcess.h"
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <windows.h>
#endif
namespace suite_clip
{
inline std::vector<uint8_t> NV12(const std::vector<uint8_t> &source, int width, int height)
{
    if (width % 2 || height % 2 || source.size() != size_t(width) * height * 4)
        throw std::runtime_error("Invalid encoder frame dimensions");
    std::vector<uint8_t> dest(size_t(width) * height * 3 / 2);
    auto clamp = [](int v) { return uint8_t(std::clamp(v, 0, 255)); };
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
            auto p = source.data() + (size_t(y) * width + x) * 4;
            dest[size_t(y) * width + x] = clamp(((47 * p[2] + 157 * p[1] + 16 * p[0] + 128) >> 8) + 16);
        }
    size_t uv = size_t(width) * height;
    for (int y = 0; y < height; y += 2)
        for (int x = 0; x < width; x += 2)
        {
            int r = 0, g = 0, b = 0;
            for (int j = 0; j < 2; j++)
                for (int i = 0; i < 2; i++)
                {
                    auto p = source.data() + (size_t(y + j) * width + x + i) * 4;
                    b += p[0];
                    g += p[1];
                    r += p[2];
                }
            r /= 4;
            g /= 4;
            b /= 4;
            dest[uv + size_t(y / 2) * width + x] = clamp(((-26 * r - 87 * g + 112 * b + 128) >> 8) + 128);
            dest[uv + size_t(y / 2) * width + x + 1] = clamp(((112 * r - 102 * g - 10 * b + 128) >> 8) + 128);
        }
    return dest;
}
#ifdef _WIN32
inline void MediaCheck(HRESULT hr)
{
    if (FAILED(hr))
    {
        std::ostringstream s;
        s << "Windows video encoder failed (0x" << std::hex << uint32_t(hr)
          << "). Check disk space and Windows Media Foundation / H.264 "
             "availability; GIF remains available.";
        throw std::runtime_error(s.str());
    }
}

template <class T> struct MediaPtr
{
    T *p = nullptr;

    ~MediaPtr()
    {
        if (p)
            p->Release();
    }

    T **Out()
    {
        if (p)
        {
            p->Release();
            p = nullptr;
        }
        return &p;
    }

    T *operator->()
    {
        return p;
    }
};

struct MediaSession
{
    bool com = false, mf = false;

    MediaSession()
    {
        MediaCheck(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
        com = true;
        HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
        if (FAILED(hr))
        {
            CoUninitialize();
            com = false;
            MediaCheck(hr);
        }
        mf = true;
    }

    ~MediaSession()
    {
        if (mf)
            MFShutdown();
        if (com)
            CoUninitialize();
    }
};

class Mp4Writer
{
    MediaSession session;
    MediaPtr<IMFByteStream> fileStream;
    MediaPtr<IMFSinkWriter> sink;
    DWORD stream = 0;
    Options options;
    uint64_t index = 0;

  public:
    Mp4Writer(const std::filesystem::path &file, const Options &o) : options(o)
    {
        // The encoder owns a previously reserved empty file. Open it explicitly;
        // do not rely on URL-only sink writer overwrite behavior.
        MediaCheck(MFCreateFile(MF_ACCESSMODE_WRITE, MF_OPENMODE_FAIL_IF_NOT_EXIST, MF_FILEFLAGS_NONE,
                                file.c_str(), fileStream.Out()));
        MediaPtr<IMFAttributes> attrs;
        MediaCheck(MFCreateAttributes(attrs.Out(), 3));
        MediaCheck(attrs->SetGUID(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4));
        MediaCheck(attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE));
        MediaCheck(MFCreateSinkWriterFromURL(file.c_str(), fileStream.p, attrs.p, sink.Out()));
        MediaPtr<IMFMediaType> output;
        MediaCheck(MFCreateMediaType(output.Out()));
        MediaCheck(output->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
        MediaCheck(output->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264));
        MediaCheck(
            output->SetUINT32(MF_MT_AVG_BITRATE, UINT32(std::clamp(int64_t(o.width) * o.height * o.fps / 8,
                                                                   int64_t(2000000), int64_t(40000000)))));
        MediaCheck(output->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
        MediaCheck(MFSetAttributeSize(output.p, MF_MT_FRAME_SIZE, o.width, o.height));
        MediaCheck(MFSetAttributeRatio(output.p, MF_MT_FRAME_RATE, o.fps, 1));
        MediaCheck(MFSetAttributeRatio(output.p, MF_MT_PIXEL_ASPECT_RATIO, 1, 1));
        MediaCheck(sink->AddStream(output.p, &stream));
        MediaPtr<IMFMediaType> input;
        MediaCheck(MFCreateMediaType(input.Out()));
        MediaCheck(input->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
        MediaCheck(input->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12));
        MediaCheck(input->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
        MediaCheck(input->SetUINT32(MF_MT_DEFAULT_STRIDE, o.width));
        MediaCheck(input->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_16_235));
        MediaCheck(input->SetUINT32(MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT709));
        MediaCheck(MFSetAttributeSize(input.p, MF_MT_FRAME_SIZE, o.width, o.height));
        MediaCheck(MFSetAttributeRatio(input.p, MF_MT_FRAME_RATE, o.fps, 1));
        MediaCheck(MFSetAttributeRatio(input.p, MF_MT_PIXEL_ASPECT_RATIO, 1, 1));
        MediaCheck(sink->SetInputMediaType(stream, input.p, nullptr));
        MediaCheck(sink->BeginWriting());
    }

    void Write(const std::vector<uint8_t> &bgra)
    {
        auto bytes = NV12(bgra, options.width, options.height);
        MediaPtr<IMFMediaBuffer> buffer;
        MediaCheck(MFCreateMemoryBuffer(DWORD(bytes.size()), buffer.Out()));
        BYTE *dest = nullptr;
        MediaCheck(buffer->Lock(&dest, nullptr, nullptr));
        std::memcpy(dest, bytes.data(), bytes.size());
        MediaCheck(buffer->Unlock());
        MediaCheck(buffer->SetCurrentLength(DWORD(bytes.size())));
        MediaPtr<IMFSample> sample;
        MediaCheck(MFCreateSample(sample.Out()));
        MediaCheck(sample->AddBuffer(buffer.p));
        MediaCheck(sample->SetSampleTime(options.Stamp(index)));
        MediaCheck(sample->SetSampleDuration(options.Stamp(index + 1) - options.Stamp(index)));
        MediaCheck(sink->WriteSample(stream, sample.p));
        index++;
    }

    void Finish()
    {
        MediaCheck(sink->Finalize());
    }
};
#endif
class Encoder
{
    std::thread thread;
    mutable std::mutex gate;
    std::condition_variable wake;
    std::deque<std::vector<uint8_t>> queue;
    std::vector<std::vector<uint8_t>> buffers;
    std::atomic<int> phase{0};
    bool finish = false;
    std::atomic<bool> abort{false};
    std::string error;
    std::filesystem::path path;
    std::atomic<int> state{0};
    std::atomic<uint64_t> written{0};

  public:
    ~Encoder()
    {
        Cancel();
        if (thread.joinable())
            thread.join();
    }

    int State() const
    {
        return state.load();
    }

    uint64_t Written() const
    {
        return written.load();
    }

    std::string Error() const
    {
        std::lock_guard l(gate);
        return error;
    }

    std::filesystem::path Path() const
    {
        std::lock_guard l(gate);
        return path;
    }

    int Phase() const
    {
        return phase.load();
    }

    const char *PhaseName() const
    {
        switch (phase.load())
        {
        case 1:
            return "Rendering / encoding frames";
        case 2:
            return "Analysing compression / pass 1";
        case 3:
            return "Compressing video / pass 2";
        case 4:
            return "Verifying file size";
        case 5:
            return "Rendering recorded game audio";
        case 6:
            return "Finalizing video and audio";
        default:
            return "Finalizing video";
        }
    }

    std::vector<uint8_t> AcquireFrame(size_t bytes)
    {
        std::vector<uint8_t> result;
        {
            std::lock_guard l(gate);
            if (!buffers.empty())
            {
                result = std::move(buffers.back());
                buffers.pop_back();
            }
        }
        result.resize(bytes);
        return result;
    }

    bool Ready() const
    {
        std::lock_guard l(gate);
        return state == 1 && !abort && !finish && queue.size() < 2;
    }

    bool Push(std::vector<uint8_t> bytes)
    {
        std::lock_guard l(gate);
        if (state != 1 || abort || finish || queue.size() >= 2)
            return false;
        queue.push_back(std::move(bytes));
        wake.notify_one();
        return true;
    }

    void Finish()
    {
        std::lock_guard l(gate);
        finish = true;
        wake.notify_one();
    }

    void Cancel()
    {
        std::lock_guard l(gate);
        abort = true;
        wake.notify_one();
    }

    bool Start(const Options &options, const std::filesystem::path &destination, ExportSource source = {})
    {
        if (state == 1)
            return false;
        if (thread.joinable())
            thread.join();
        try
        {
            auto validated = options;
            validated.Validate(options.b);
        }
        catch (const std::exception &e)
        {
            std::lock_guard l(gate);
            error = e.what();
            state = 3;
            return false;
        }
        {
            std::lock_guard l(gate);
            queue.clear();
            buffers.clear();
            phase = 1;
            finish = abort = false;
            error.clear();
            path = destination;
            written = 0;
            state = 1;
        }
        try
        {
            thread = std::thread([this, options, destination, source] {
                struct FrameCleanup
                {
                    Encoder &owner;
                    ~FrameCleanup()
                    {
                        std::lock_guard l(owner.gate);
                        owner.queue.clear();
                        owner.buffers.clear();
                    }
                } cleanup{*this};
                auto started = std::chrono::steady_clock::now();
                auto temporary = destination;
                temporary += std::wstring(L".partial") + Extension(options.format);
                bool temporaryOwned = false;
                struct WorkFiles
                {
                    std::filesystem::path path;
                    ~WorkFiles()
                    {
                        if (!path.empty())
                        {
                            std::error_code ec;
                            std::filesystem::remove_all(path, ec);
                        }
                    }
                } work;

                try
                {
                    std::filesystem::create_directories(destination.parent_path());
                    if (std::filesystem::exists(destination) || std::filesystem::exists(temporary))
                        throw std::runtime_error("Export name already exists; choose a new filename");
                    if (options.format == 3)
                    {
                        if (!std::filesystem::create_directory(temporary))
                            throw std::runtime_error("Cannot reserve PNG sequence folder");
                        temporaryOwned = true;
                    }
                    else
                    {
#ifdef _WIN32
                        HANDLE reservation = CreateFileW(temporary.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                                         nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
                        if (reservation == INVALID_HANDLE_VALUE)
                            throw std::runtime_error("Cannot reserve clip output file");
                        CloseHandle(reservation);
                        temporaryOwned = true;
#else
                        {
                            std::ofstream reserve(temporary, std::ios::binary);
                            if (!reserve)
                                throw std::runtime_error("Cannot reserve clip output file");
                            temporaryOwned = true;
                        }
#endif
                    }
                    bool withAudio = options.AudioEnabled();
                    if (withAudio && (!source.audio.recorded || source.audioFile.empty()))
                        throw std::runtime_error("Recorded audio is missing or unavailable");
                    if (options.maxFileBytes || withAudio)
                    {
                        auto folder = temporary;
                        folder += L".work";
                        if (!std::filesystem::create_directory(folder))
                            throw std::runtime_error("Cannot reserve temporary media folder");
                        work.path = folder;
                    }
                    auto audioPath = work.path / L"audio.wav";
                    if (withAudio)
                    {
                        phase = 5;
                        RenderRecordedAudio(audioPath, options, source, abort);
                        phase = 1;
                    }
                    {
                        std::unique_ptr<GifWriter> gif;
                        std::unique_ptr<PngSequence> png;
#ifdef _WIN32
                        std::unique_ptr<Mp4Writer> mp4;
                        std::unique_ptr<LosslessWriter> lossless;
                        if (options.format == 0)
                            mp4 = std::make_unique<Mp4Writer>(temporary, options);
                        std::filesystem::path videoSource;
                        if (ExternalFormat(options.format))
                        {
                            auto streamOptions = options;
                            auto target = temporary;
                            if (options.maxFileBytes)
                            {
                                SizeBitrate(options);
                                videoSource = work.path / L"source.avi";
                                target = videoSource;
                                streamOptions.format = 4;
                                streamOptions.maxFileBytes = 0;
                            }
                            if (withAudio && !options.maxFileBytes)
                            {
                                auto input = StreamInputArguments(streamOptions, true);
                                auto codec = VideoCodecArguments(streamOptions);
                                auto duration = AudioDurationArguments(streamOptions);
                                std::wstring command = std::wstring(input.begin(), input.end()) + L"-i " +
                                                       QuoteArgument(audioPath.wstring()) +
                                                       L" -map 0:v:0 -map 1:a:0 " +
                                                       std::wstring(codec.begin(), codec.end()) +
                                                       AudioCodecArguments(streamOptions.format) +
                                                       std::wstring(duration.begin(), duration.end()) +
                                                       QuoteArgument(target.wstring());
                                lossless = std::make_unique<LosslessWriter>(target, streamOptions, abort,
                                                                            command, true);
                            }
                            else
                                lossless = std::make_unique<LosslessWriter>(target, streamOptions, abort);
                        }
#else
                        if (ExternalFormat(options.format))
                            throw std::runtime_error("Video export uses the bundled Windows encoder");
                        if (options.format == 0)
                            throw std::runtime_error("MP4 encoding requires Windows Media Foundation");
#endif
                        if (options.format == 3)
                            png = std::make_unique<PngSequence>(temporary, options);
                        if (options.format == 1)
                            gif = std::make_unique<GifWriter>(temporary, options.width, options.height,
                                                              options.fps);
                        double encodeMs = 0, inputWaitMs = 0;
                        while (true)
                        {
                            std::vector<uint8_t> bytes;
                            {
                                std::unique_lock l(gate);
                                auto waitStart = std::chrono::steady_clock::now();
                                wake.wait(l, [&] { return abort || finish || !queue.empty(); });
                                inputWaitMs += std::chrono::duration<double, std::milli>(
                                                   std::chrono::steady_clock::now() - waitStart)
                                                   .count();
                                if (abort)
                                    break;
                                if (queue.empty())
                                {
                                    if (finish)
                                        break;
                                    continue;
                                }
                                bytes = std::move(queue.front());
                                queue.pop_front();
                            }
                            if (bytes.size() != size_t(options.width) * options.height * 4)
                                throw std::runtime_error("Encoder frame size mismatch");
                            auto encodeStart = std::chrono::steady_clock::now();
                            if (gif)
                                gif->Write(bytes);
                            if (png)
                                png->Write(bytes);
#ifdef _WIN32
                            if (mp4)
                            {
                                mp4->Write(bytes);
                            }
                            if (lossless)
                                lossless->Write(bytes);
#endif
                            {
                                std::lock_guard l(gate);
                                if (buffers.size() < 2)
                                    buffers.push_back(std::move(bytes));
                            }
                            encodeMs += std::chrono::duration<double, std::milli>(
                                            std::chrono::steady_clock::now() - encodeStart)
                                            .count();
                            uint64_t count = ++written;
                            if (source.pipeline && (count % 500 == 0 || count == options.Frames()))
                            {
                                ExportProgress point;
                                point.frames = count;
                                point.wallSeconds =
                                    std::chrono::duration<double>(std::chrono::steady_clock::now() - started)
                                        .count();
                                point.encoderMilliseconds = encodeMs;
                                point.inputWaitMilliseconds = inputWaitMs;
                                std::error_code ec;
                                point.outputBytes =
                                    options.format == 3 ? 0 : std::filesystem::file_size(temporary, ec);
                                if (ec)
                                    point.outputBytes = 0;
#ifdef _WIN32
                                PROCESS_MEMORY_COUNTERS counters{};
                                if (K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
                                    point.trainerWorkingSet = uint64_t(counters.WorkingSetSize);
                                if (lossless)
                                    point.encoderWorkingSet = lossless->WorkingSetBytes();
#endif
                                source.pipeline->Mark(point);
                            }
                        }
                        bool cancelled;
                        {
                            std::lock_guard l(gate);
                            cancelled = abort;
                        }
                        if (!cancelled)
                        {
                            if (written != options.Frames())
                                throw std::runtime_error("Export ended before all frames were encoded");
                            if (gif)
                                gif->Finish();
                            if (png)
                                png->Finish();
#ifdef _WIN32
                            if (mp4)
                            {
                                mp4->Finish();
                                mp4.reset();
                            }
                            if (lossless)
                            {
                                lossless->Finish();
                                lossless.reset();
                                if (options.maxFileBytes)
                                {
                                    uint64_t bitrate = SizeBitrate(options);
                                    bool fits = false;
                                    for (int attempt = 0; attempt < 3 && !fits; attempt++)
                                    {
                                        for (int pass = 1; pass <= 2; pass++)
                                        {
                                            phase = pass == 1 ? 2 : 3;
                                            auto args = BudgetCodecArguments(options, bitrate, pass);
                                            auto stats = work.path / L"rate";
                                            std::wstring command =
                                                L"-hide_banner -nostdin -loglevel error -y "
                                                L"-filter_threads 1 "
                                                L"-xerror -i " +
                                                QuoteArgument(videoSource.wstring()) +
                                                (withAudio && pass == 2
                                                     ? L" -i " + QuoteArgument(audioPath.wstring()) +
                                                           L" -map 0:v:0 -map 1:a:0 " +
                                                           AudioCodecArguments(options.format)
                                                     : L" -map 0:v:0 -an ") +
                                                L"-passlogfile " + QuoteArgument(stats.wstring()) + L" " +
                                                std::wstring(args.begin(), args.end()) +
                                                (pass == 1 ? L"NUL" : QuoteArgument(temporary.wstring()));
                                            auto diagnostic =
                                                work.path / (L"pass-" + std::to_wstring(attempt) + L"-" +
                                                             std::to_wstring(pass));
                                            LosslessWriter process(diagnostic, options, abort, command,
                                                                   false);
                                            process.Finish();
                                        }
                                        phase = 4;
                                        auto actual = std::filesystem::file_size(temporary);
                                        fits = actual > 128 && actual <= options.maxFileBytes;
                                        if (!fits)
                                        {
                                            bitrate = SmallerBitrate(bitrate, actual, options.maxFileBytes);
                                            if (bitrate < 24000)
                                                break;
                                        }
                                    }
                                    if (!fits)
                                        throw std::runtime_error(
                                            "The requested file size could not contain this "
                                            "complete clip. "
                                            "Increase the limit or reduce resolution / FPS.");
                                }
                            }
#endif
                        }
                    }
                    {
                        std::lock_guard l(gate);
                        if (abort)
                        {
                            std::error_code ec;
                            std::filesystem::remove_all(temporary, ec);
                            state = 4;
                            return;
                        }
                    }
#ifdef _WIN32
                    if (withAudio && !options.maxFileBytes && !ExternalFormat(options.format))
                    {
                        phase = 6;
                        auto video = work.path / (std::wstring(L"video") + Extension(options.format));
                        std::filesystem::rename(temporary, video);
                        std::wstring command =
                            L"-hide_banner -nostdin -loglevel error -y -xerror -i " +
                            QuoteArgument(video.wstring()) + L" -i " + QuoteArgument(audioPath.wstring()) +
                            L" -map 0:v:0 -map 1:a:0 -c:v copy " + AudioCodecArguments(options.format) +
                            (options.format == 0 || options.format == 6 ? L"-movflags +faststart " : L"") +
                            QuoteArgument(temporary.wstring());
                        LosslessWriter mux(work.path / L"mux", options, abort, command, false);
                        mux.Finish();
                    }
#endif
                    if (abort)
                        throw std::runtime_error("Export cancelled");
                    if (options.format == 4)
                        VerifyAviTiming(temporary, options);
                    if (options.maxFileBytes && std::filesystem::file_size(temporary) > options.maxFileBytes)
                        throw std::runtime_error("Encoded file exceeds the size limit; "
                                                 "output was not published");
#ifdef _WIN32
                    if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH))
                        throw std::runtime_error(
                            "Cannot commit exported clip; existing files were preserved");
#else
                    if (std::filesystem::exists(destination))
                        throw std::runtime_error("Destination exists");
                    std::filesystem::rename(temporary, destination);
#endif
                    try
                    {
                        WriteExportReport(
                            destination, options, source,
                            std::chrono::duration<double>(std::chrono::steady_clock::now() - started)
                                .count());
                    }
                    catch (...)
                    { /* The completed video remains usable if diagnostics
                         cannot be written. */
                    }
                    state = 2;
                }
                catch (const std::exception &e)
                {
                    std::error_code ec;
                    if (temporaryOwned)
                        std::filesystem::remove_all(temporary, ec);
                    std::lock_guard l(gate);
                    error = e.what();
                    state = abort ? 4 : 3;
                }
            });
            return true;
        }
        catch (const std::exception &e)
        {
            std::lock_guard l(gate);
            error = e.what();
            state = 3;
            return false;
        }
    }
};
} // namespace suite_clip
