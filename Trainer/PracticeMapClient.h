#pragma once
#include "PracticeMapWorker.h"
#include "SeedSearch.h"

// Static maps are polled at 10 Hz. During a search each rendered frame can
// accept one small packet; it is acknowledged only after bounded acceptance.
class PracticeMapClient
{
    HANDLE mapping = nullptr, mutex = nullptr;
    uint8_t *view = nullptr;
    DWORD owner = 0;
    uint32_t revision = 0;
    ULONGLONG nextOpen = 0, nextRead = 0;

  public:
    std::string error;
    PracticeMapClient() = default;
    PracticeMapClient(const PracticeMapClient &) = delete;

    ~PracticeMapClient()
    {
        Close();
    }

    void Close()
    {
        if (view)
            UnmapViewOfFile(view);
        if (mapping)
            CloseHandle(mapping);
        if (mutex)
            CloseHandle(mutex);
        view = nullptr;
        mapping = mutex = nullptr;
        owner = 0;
        revision = 0;
        nextOpen = nextRead = 0;
        error.clear();
    }

    void Poll(DWORD pid, uint32_t session, bool requested, practice::MapWorker &worker,
              practice::SeedSearch &search)
    {
        if (pid != owner)
        {
            Close();
            owner = pid;
        }
        if (!pid || !requested)
            return;
        ULONGLONG now = GetTickCount64();
        if (now < nextRead)
            return;
        nextRead = now + (search.Busy() ? 0 : 100);
        if (!view && now >= nextOpen)
        {
            nextOpen = now + 1000;
            std::string name = "ItStealsPracticeMap.v10." + std::to_string(pid);
            mapping = OpenFileMappingA(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, name.c_str());
            mutex = OpenMutexA(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, (name + ".Lock").c_str());
            if (mapping && mutex)
                view = static_cast<uint8_t *>(
                    MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, practice::MapCapacity));
            if (!view)
            {
                if (mapping)
                    CloseHandle(mapping);
                if (mutex)
                    CloseHandle(mutex);
                mapping = mutex = nullptr;
                return;
            }
        }
        if (!view)
            return;
        DWORD acquired = WaitForSingleObject(mutex, 0);
        if (acquired != WAIT_OBJECT_0 && acquired != WAIT_ABANDONED)
            return;
        std::vector<uint8_t> bytes;
        uint32_t packetRevision = 0;
        if (acquired == WAIT_OBJECT_0)
        {
            int32_t length = 0;
            uint32_t helperSession = 0;
            std::memcpy(&length, view, 4);
            std::memcpy(&packetRevision, view + 4, 4);
            std::memcpy(&helperSession, view + 8, 4);
            if (helperSession == session && packetRevision != revision && length > 0 &&
                length <= static_cast<int32_t>(practice::MapCapacity - 64))
                bytes.assign(view + 64, view + 64 + length);
        }
        ReleaseMutex(mutex);
        if (bytes.empty())
            return;
        int32_t source = 0;
        uint32_t magic = 0;
        if (bytes.size() >= 12)
        {
            std::memcpy(&source, bytes.data() + 8, 4);
            std::memcpy(&magic, bytes.data(), 4);
        }
        if (source != static_cast<int32_t>(pid))
        {
            error = "Practice map process identity mismatch";
            search.Stop(error);
        }
        else if (magic == practice::SeedBatchMagic)
        {
            auto batch = practice::DecodeSeedBatch(bytes, error);
            if (!batch)
                search.Stop("Invalid search publication: " + error);
            else if (!search.Submit(std::move(batch), session))
                return; // Keep this publication unread until there is queue space.
        }
        else
        {
            error.clear();
            worker.Submit(std::move(bytes), session);
        }
        acquired = WaitForSingleObject(mutex, 0);
        if (acquired != WAIT_OBJECT_0 && acquired != WAIT_ABANDONED)
            return;
        uint32_t current = 0, helperSession = 0;
        std::memcpy(&current, view + 4, 4);
        std::memcpy(&helperSession, view + 8, 4);
        if (acquired == WAIT_OBJECT_0 && current == packetRevision && helperSession == session)
        {
            std::memcpy(view + 12, &packetRevision, 4);
            revision = packetRevision;
        }
        ReleaseMutex(mutex);
    }
};
