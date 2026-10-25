#include <cstdint>
#include <stdexcept>

// Minimal native COM fixture. The production C# code calls these through the
// actual vtables; no Unity or platform graphics library is loaded on Linux.
struct Object
{
    void **table;
    int refs = 1;
};

static void *textureTable[4]{}, *deviceTable[41]{}, *contextTable[112]{}, *sharedTable[3]{},
    *mutexTable[10]{};
static Object texture{textureTable}, device{deviceTable}, context{contextTable}, shared{sharedTable},
    mutex{mutexTable};
static bool ready = true;
static bool dirty = false;
static int copies = 0, fault = 0;

static uint32_t AddRef(Object *self)
{
    return ++self->refs;
}

static uint32_t Release(Object *self)
{
    return --self->refs;
}

static void GetDevice(Object *, Object **out)
{
    *out = &device;
    AddRef(*out);
}

static void GetContext(Object *, Object **out)
{
    *out = &context;
    AddRef(*out);
}

static int Open(Object *, intptr_t handle, const void *, Object **out)
{
    if (handle != 123)
        return -1;
    *out = &shared;
    AddRef(*out);
    return 0;
}

static int Query(Object *, const void *, Object **out)
{
    *out = &mutex;
    AddRef(*out);
    return 0;
}

static int Acquire(Object *, uint64_t key, uint32_t milliseconds)
{
    if (key != 1 || milliseconds != 0)
        return -1;
    return fault ? fault : ready ? 0 : 258;
}

static int Unlock(Object *, uint64_t key)
{
    ready = false;
    return key == 0 && !dirty ? 0 : -1;
}

static void Copy(Object *, Object *destination, Object *source)
{
    if (destination == &texture && source == &shared && ready)
    {
        copies++;
        dirty = true;
    }
}

static void Flush(Object *)
{
    dirty = false;
}

extern "C" void *CaptureTexture()
{
    for (auto *object : {&texture, &device, &context, &shared, &mutex})
    {
        object->table[1] = reinterpret_cast<void *>(AddRef);
        object->table[2] = reinterpret_cast<void *>(Release);
    }
    textureTable[3] = reinterpret_cast<void *>(GetDevice);
    deviceTable[28] = reinterpret_cast<void *>(Open);
    deviceTable[40] = reinterpret_cast<void *>(GetContext);
    sharedTable[0] = reinterpret_cast<void *>(Query);
    mutexTable[8] = reinterpret_cast<void *>(Acquire);
    mutexTable[9] = reinterpret_cast<void *>(Unlock);
    contextTable[47] = reinterpret_cast<void *>(Copy);
    contextTable[111] = reinterpret_cast<void *>(Flush);
    return &texture;
}

extern "C" void CaptureReady(int value)
{
    ready = value != 0;
}

extern "C" void CaptureFault(int value)
{
    fault = value;
}

extern "C" int CaptureCopies()
{
    return copies;
}

extern "C" int CaptureReferences()
{
    return texture.refs + device.refs + context.refs + shared.refs + mutex.refs;
}

extern "C" void CaptureInvoke(void (*callback)(int), int id)
{
    callback(id);
}
