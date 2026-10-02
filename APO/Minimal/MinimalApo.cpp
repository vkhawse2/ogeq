// MinimalApo.cpp -- Bare-minimum APO for engine loading test.
//
// Purpose: isolate whether the Windows audio engine will load ANY third-party
// APO on this machine. If this loads, OGEQ's problem is in its implementation.
// If this doesn't load, the problem is systemic (policy, endpoint, OS).
//
// Implements only the required interfaces with pass-through processing.
// Logs to C:\ProgramData\OGEQ\minimal-trace.log

#define INITGUID
#include <windows.h>
#include <audioenginebaseapo.h>
#include <combaseapi.h>
#include <cstdio>
#include <cstdarg>

// {d4e5f6a7-b8c9-4d0e-af12-3456789abcde}
DEFINE_GUID(CLSID_MinimalApo,
    0xd4e5f6a7, 0xb8c9, 0x4d0e, 0xaf, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde);

HINSTANCE g_hInstance = nullptr;
long g_refCount = 0;

static void MinimalTrace(const wchar_t* fmt, ...) {
    wchar_t buf[512];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _TRUNCATE, fmt, ap);
    va_end(ap);

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t line[640];
    _snwprintf_s(line, _TRUNCATE, L"[%02d:%02d:%02d.%03d] %s\r\n",
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buf);

    CreateDirectoryW(L"C:\\ProgramData\\OGEQ", nullptr);
    HANDLE h = CreateFileW(L"C:\\ProgramData\\OGEQ\\minimal-trace.log",
                           FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        LARGE_INTEGER size{};
        if (GetFileSizeEx(h, &size) && size.QuadPart == 0) {
            const BYTE bom[] = { 0xFF, 0xFE };
            WriteFile(h, bom, 2, &written, nullptr);
        }
        WriteFile(h, line, (DWORD)(wcslen(line) * sizeof(wchar_t)), &written, nullptr);
        CloseHandle(h);
    }
}

//--------------------------------------------------------------------
// Minimal APO class
//--------------------------------------------------------------------

class MinimalApo : public IAudioProcessingObject,
                   public IAudioProcessingObjectConfiguration,
                   public IAudioProcessingObjectRT,
                   public IAudioSystemEffects {
public:
    MinimalApo(IUnknown* outer) : outer_(outer), ref_(1) {
        MinimalTrace(L"MinimalApo constructed, outer=%p", outer);
    }
    virtual ~MinimalApo() {
        MinimalTrace(L"MinimalApo destroyed");
    }

    // IUnknown (delegating)
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (outer_) return outer_->QueryInterface(riid, ppv);
        return NonDelegatingQueryInterface(riid, ppv);
    }
    STDMETHODIMP_(ULONG) AddRef() override {
        if (outer_) return outer_->AddRef();
        return NonDelegatingAddRef();
    }
    STDMETHODIMP_(ULONG) Release() override {
        if (outer_) return outer_->Release();
        return NonDelegatingRelease();
    }

    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        wchar_t iidStr[64] = L"?";
        StringFromGUID2(riid, iidStr, 64);
        MinimalTrace(L"MinimalApo QI(%s)", iidStr);

        if (riid == IID_IUnknown) {
            *ppv = static_cast<IAudioProcessingObject*>(this);
        } else if (riid == __uuidof(IAudioProcessingObject)) {
            *ppv = static_cast<IAudioProcessingObject*>(this);
        } else if (riid == __uuidof(IAudioProcessingObjectConfiguration)) {
            *ppv = static_cast<IAudioProcessingObjectConfiguration*>(this);
        } else if (riid == __uuidof(IAudioProcessingObjectRT)) {
            *ppv = static_cast<IAudioProcessingObjectRT*>(this);
        } else if (riid == __uuidof(IAudioSystemEffects)) {
            *ppv = static_cast<IAudioSystemEffects*>(this);
        } else {
            MinimalTrace(L"MinimalApo QI(%s) -> E_NOINTERFACE", iidStr);
            return E_NOINTERFACE;
        }
        NonDelegatingAddRef();
        MinimalTrace(L"MinimalApo QI(%s) -> S_OK", iidStr);
        return S_OK;
    }
    STDMETHODIMP_(ULONG) NonDelegatingAddRef() {
        return InterlockedIncrement(&ref_);
    }
    STDMETHODIMP_(ULONG) NonDelegatingRelease() {
        ULONG r = InterlockedDecrement(&ref_);
        if (r == 0) delete this;
        return r;
    }

    // IAudioProcessingObject
    STDMETHODIMP Initialize(UINT32 cbDataSize, BYTE* pbyData) override {
        MinimalTrace(L"MinimalApo Initialize(cb=%u)", cbDataSize);
        return S_OK;
    }
    STDMETHODIMP GetInputChannelCount(UINT32* pu32ChannelCount) override {
        if (!pu32ChannelCount) return E_POINTER;
        *pu32ChannelCount = 2;
        return S_OK;
    }
    STDMETHODIMP GetLatency(HNSTIME* pTime) override {
        if (!pTime) return E_POINTER;
        *pTime = 0;
        return S_OK;
    }
    STDMETHODIMP GetRegistrationProperties(APO_REG_PROPERTIES** ppRegProps) override {
        if (!ppRegProps) return E_POINTER;
        *ppRegProps = nullptr;
        return E_NOTIMPL;
    }
    STDMETHODIMP Reset() override { return S_OK; }
    STDMETHODIMP IsInputFormatSupported(IAudioMediaType* pOutputFormat,
                                        IAudioMediaType* pRequestedInputFormat,
                                        IAudioMediaType** ppSupportedInputFormat) override {
        // Accept anything, request float32
        if (ppSupportedInputFormat) *ppSupportedInputFormat = nullptr;
        MinimalTrace(L"MinimalApo IsInputFormatSupported -> S_OK");
        return S_OK;
    }
    STDMETHODIMP IsOutputFormatSupported(IAudioMediaType* pInputFormat,
                                         IAudioMediaType* pRequestedOutputFormat,
                                         IAudioMediaType** ppSupportedOutputFormat) override {
        if (ppSupportedOutputFormat) *ppSupportedOutputFormat = nullptr;
        return S_OK;
    }

    // IAudioProcessingObjectConfiguration
    STDMETHODIMP LockForProcess(UINT32 u32NumInputConnections,
                                APO_CONNECTION_DESCRIPTOR** ppInputConnections,
                                UINT32 u32NumOutputConnections,
                                APO_CONNECTION_DESCRIPTOR** ppOutputConnections) override {
        MinimalTrace(L"MinimalApo LockForProcess(in=%u, out=%u)", u32NumInputConnections, u32NumOutputConnections);
        return S_OK;
    }
    STDMETHODIMP UnlockForProcess() override {
        MinimalTrace(L"MinimalApo UnlockForProcess");
        return S_OK;
    }

    // IAudioProcessingObjectRT
    STDMETHODIMP_(void) APOProcess(UINT32 u32NumInputConnections,
                                   APO_CONNECTION_PROPERTY** ppInputConnections,
                                   UINT32 u32NumOutputConnections,
                                   APO_CONNECTION_PROPERTY** ppOutputConnections) override {
        // Pass-through: copy input to output
        static long callCount = 0;
        long n = InterlockedIncrement(&callCount);
        if (n == 1) {
            MinimalTrace(L"MinimalApo APOProcess FIRST CALL");
        }
        if (u32NumInputConnections > 0 && u32NumOutputConnections > 0 &&
            ppInputConnections && ppOutputConnections &&
            ppInputConnections[0] && ppOutputConnections[0]) {
            UINT32 frames = ppInputConnections[0]->u32ValidFrameCount;
            if (frames > ppOutputConnections[0]->u32ValidFrameCount)
                frames = ppOutputConnections[0]->u32ValidFrameCount;
            // Assume float32 stereo (simplified for test)
            memcpy(ppOutputConnections[0]->pBuffer,
                   ppInputConnections[0]->pBuffer,
                   frames * 2 * sizeof(float));
            ppOutputConnections[0]->u32ValidFrameCount = frames;
            ppOutputConnections[0]->u32BufferFlags = ppInputConnections[0]->u32BufferFlags;
        }
    }
    STDMETHODIMP_(UINT32) CalcInputFrames(UINT32 u32OutputFrameCount) override {
        return u32OutputFrameCount;
    }
    STDMETHODIMP_(UINT32) CalcOutputFrames(UINT32 u32InputFrameCount) override {
        return u32InputFrameCount;
    }

private:
    IUnknown* outer_;
    long ref_;
};

//--------------------------------------------------------------------
// Class factory
//--------------------------------------------------------------------

class MinimalApoFactory : public IClassFactory {
public:
    MinimalApoFactory() : ref_(1) { InterlockedIncrement(&g_refCount); }
    virtual ~MinimalApoFactory() { InterlockedDecrement(&g_refCount); }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&ref_);
    }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG r = InterlockedDecrement(&ref_);
        if (r == 0) delete this;
        return r;
    }

    STDMETHODIMP CreateInstance(IUnknown* pOuter, REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        wchar_t iidStr[64] = L"?";
        StringFromGUID2(riid, iidStr, 64);
        MinimalTrace(L"MinimalApoFactory CreateInstance(outer=%p, riid=%s)", pOuter, iidStr);

        if (pOuter && riid != IID_IUnknown)
            return E_NOINTERFACE;

        MinimalApo* apo = new (std::nothrow) MinimalApo(pOuter);
        if (!apo) return E_OUTOFMEMORY;

        HRESULT hr = apo->NonDelegatingQueryInterface(
            pOuter ? IID_IUnknown : riid, ppv);
        apo->NonDelegatingRelease();
        MinimalTrace(L"MinimalApoFactory CreateInstance -> hr=0x%08X", hr);
        return hr;
    }

    STDMETHODIMP LockServer(BOOL lock) override {
        if (lock) InterlockedIncrement(&g_refCount);
        else InterlockedDecrement(&g_refCount);
        return S_OK;
    }

private:
    long ref_;
};

//--------------------------------------------------------------------
// DLL exports
//--------------------------------------------------------------------

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    wchar_t clsidStr[64] = L"?", iidStr[64] = L"?";
    StringFromGUID2(rclsid, clsidStr, 64);
    StringFromGUID2(riid, iidStr, 64);
    wchar_t dllPath[MAX_PATH] = L"?";
    GetModuleFileNameW(g_hInstance, dllPath, MAX_PATH);
    DWORD pid = GetCurrentProcessId();

    MinimalTrace(L"MINIMAL APO LOADED: PID=%lu, PATH=%s", pid, dllPath);
    MinimalTrace(L"DllGetClassObject: rclsid=%s, riid=%s", clsidStr, iidStr);

    if (rclsid != CLSID_MinimalApo) {
        MinimalTrace(L"DllGetClassObject: wrong CLSID -> CLASS_E_CLASSNOTAVAILABLE");
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    MinimalApoFactory* factory = new (std::nothrow) MinimalApoFactory();
    if (!factory) return E_OUTOFMEMORY;
    HRESULT hr = factory->QueryInterface(riid, ppv);
    MinimalTrace(L"DllGetClassObject: hr=0x%08X", hr);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow() {
    return g_refCount == 0 ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer() {
    wchar_t dllPath[MAX_PATH];
    if (!GetModuleFileNameW(g_hInstance, dllPath, MAX_PATH))
        return HRESULT_FROM_WIN32(GetLastError());

    wchar_t clsidStr[64];
    StringFromGUID2(CLSID_MinimalApo, clsidStr, 64);

    // COM class
    wchar_t inprocKey[160];
    swprintf_s(inprocKey, L"CLSID\\%s\\InprocServer32", clsidStr);
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, inprocKey, 0, nullptr, 0,
                        KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
        return E_FAIL;
    RegSetValueExW(hKey, nullptr, 0, REG_SZ, (BYTE*)dllPath,
                   (DWORD)((wcslen(dllPath) + 1) * sizeof(wchar_t)));
    const wchar_t* threading = L"Both";
    RegSetValueExW(hKey, L"ThreadingModel", 0, REG_SZ, (BYTE*)threading,
                   (DWORD)((wcslen(threading) + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);

    // APO declaration
    wchar_t apoKey[256];
    swprintf_s(apoKey, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Audio\\AudioEngine\\AudioProcessingObjects\\%s", clsidStr);
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, apoKey, 0, nullptr, 0,
                        KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
        return E_FAIL;

    const wchar_t* friendly = L"Minimal Test APO";
    RegSetValueExW(hKey, L"FriendlyName", 0, REG_SZ, (BYTE*)friendly,
                   (DWORD)((wcslen(friendly) + 1) * sizeof(wchar_t)));
    const wchar_t* copyright = L"Copyright (c) 2026";
    RegSetValueExW(hKey, L"Copyright", 0, REG_SZ, (BYTE*)copyright,
                   (DWORD)((wcslen(copyright) + 1) * sizeof(wchar_t)));
    DWORD majorVer = 1, minorVer = 0, flags = 0, numIfaces = 2;
    RegSetValueExW(hKey, L"MajorVersion", 0, REG_DWORD, (BYTE*)&majorVer, sizeof(majorVer));
    RegSetValueExW(hKey, L"MinorVersion", 0, REG_DWORD, (BYTE*)&minorVer, sizeof(minorVer));
    RegSetValueExW(hKey, L"Flags", 0, REG_DWORD, (BYTE*)&flags, sizeof(flags));
    RegSetValueExW(hKey, L"NumAPOInterfaces", 0, REG_DWORD, (BYTE*)&numIfaces, sizeof(numIfaces));

    wchar_t iidStr[64];
    StringFromGUID2(__uuidof(IAudioProcessingObject), iidStr, 64);
    RegSetValueExW(hKey, L"APOInterface0", 0, REG_SZ, (BYTE*)iidStr,
                   (DWORD)((wcslen(iidStr) + 1) * sizeof(wchar_t)));
    wchar_t iidStr2[64];
    StringFromGUID2(__uuidof(IAudioSystemEffects), iidStr2, 64);
    RegSetValueExW(hKey, L"APOInterface1", 0, REG_SZ, (BYTE*)iidStr2,
                   (DWORD)((wcslen(iidStr2) + 1) * sizeof(wchar_t)));

    DWORD one = 1;
    RegSetValueExW(hKey, L"u32MaxInstances", 0, REG_DWORD, (BYTE*)&one, sizeof(one));
    RegSetValueExW(hKey, L"u32MinInputConnections", 0, REG_DWORD, (BYTE*)&one, sizeof(one));
    RegSetValueExW(hKey, L"u32MaxInputConnections", 0, REG_DWORD, (BYTE*)&one, sizeof(one));
    RegSetValueExW(hKey, L"u32MinOutputConnections", 0, REG_DWORD, (BYTE*)&one, sizeof(one));
    RegSetValueExW(hKey, L"u32MaxOutputConnections", 0, REG_DWORD, (BYTE*)&one, sizeof(one));

    RegCloseKey(hKey);
    return S_OK;
}

STDAPI DllUnregisterServer() {
    wchar_t clsidStr[64];
    StringFromGUID2(CLSID_MinimalApo, clsidStr, 64);
    wchar_t clsidKey[128];
    swprintf_s(clsidKey, L"CLSID\\%s", clsidStr);
    RegDeleteTreeW(HKEY_CLASSES_ROOT, clsidKey);
    wchar_t apoKey[256];
    swprintf_s(apoKey, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Audio\\AudioEngine\\AudioProcessingObjects\\%s", clsidStr);
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, apoKey);
    return S_OK;
}

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hInstance = hInstance;
        DisableThreadLibraryCalls(hInstance);
    }
    return TRUE;
}
