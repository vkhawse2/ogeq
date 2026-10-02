// dllmain.cpp -- DLL entry point, class factory, registration.
//
// COM aggregation: the engine creates us with a non-null outer IUnknown.
// Our factory must NOT return CLASS_E_NOAGGREGATION -- that's the silent
// skip that plagued early APO attempts industry-wide.

#define INITGUID  // Actually define the GUIDs declared via DEFINE_GUID
#include "OgeqApo.h"
#include "OgeqTrace.h"
#include <audioenginebaseapo.h>
#include <combaseapi.h>  // StringFromGUID2

HINSTANCE g_hInstance = nullptr;
long g_refCount = 0;

//--------------------------------------------------------------------
// Class factory
//--------------------------------------------------------------------

class OgeqApoFactory : public IClassFactory {
public:
    OgeqApoFactory() : ref_(1) { InterlockedIncrement(&g_refCount); }
    virtual ~OgeqApoFactory() { InterlockedDecrement(&g_refCount); }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        wchar_t iidStr[64] = L"?";
        StringFromGUID2(riid, iidStr, 64);
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            OGEQ_TRACE(L"Factory QI(%s) -> S_OK", iidStr);
            return S_OK;
        }
        *ppv = nullptr;
        OGEQ_TRACE(L"Factory QI(%s) -> E_NOINTERFACE", iidStr);
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override {
        ULONG r = InterlockedIncrement(&ref_);
        OGEQ_TRACE(L"Factory AddRef -> %lu", r);
        return r;
    }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG r = InterlockedDecrement(&ref_);
        OGEQ_TRACE(L"Factory Release -> %lu", r);
        if (r == 0) delete this;
        return r;
    }

    // IClassFactory -- aggregation is REQUIRED (engine always aggregates).
    STDMETHODIMP CreateInstance(IUnknown* pOuter, REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;

        wchar_t iidStr[64] = L"?";
        StringFromGUID2(riid, iidStr, 64);
        OGEQ_TRACE(L"CreateInstance: pOuter=%p, riid=%s", pOuter, iidStr);

        // Only IUnknown can be requested when aggregated.
        if (pOuter && riid != IID_IUnknown) {
            OGEQ_TRACE(L"CreateInstance: aggregated but riid!=IUnknown -> E_NOINTERFACE");
            return E_NOINTERFACE;
        }

        HRESULT hr = S_OK;
        ogeq::OgeqApo* apo = new (std::nothrow) ogeq::OgeqApo(pOuter, &hr);
        if (!apo) {
            OGEQ_TRACE(L"CreateInstance: new OgeqApo failed -> E_OUTOFMEMORY");
            return E_OUTOFMEMORY;
        }
        if (FAILED(hr)) {
            OGEQ_TRACE(L"CreateInstance: OgeqApo ctor hr=0x%08X", hr);
            delete apo;
            return hr;
        }

        if (pOuter) {
            // Aggregated: return the non-delegating IUnknown via proper QI.
            OGEQ_TRACE(L"CreateInstance: aggregated, QI(IID_IUnknown)");
            hr = apo->NonDelegatingQueryInterface(IID_IUnknown, ppv);
            apo->NonDelegatingRelease(); // Balance constructor ref; QI AddRef'd.
        } else {
            hr = apo->NonDelegatingQueryInterface(riid, ppv);
            apo->NonDelegatingRelease(); // Balance constructor ref; QI AddRef'd.
        }
        OGEQ_TRACE(L"CreateInstance: returning hr=0x%08X, ppv=%p", hr, *ppv);
        return hr;
    }

    STDMETHODIMP LockServer(BOOL lock) override {
        OGEQ_TRACE(L"Factory LockServer(%d)", lock);
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
    OGEQ_TRACE(L"DllGetClassObject: rclsid=%s, riid=%s", clsidStr, iidStr);
    if (rclsid != CLSID_OgeqApo) {
        OGEQ_TRACE(L"DllGetClassObject: wrong CLSID -> CLASS_E_CLASSNOTAVAILABLE");
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    OgeqApoFactory* factory = new (std::nothrow) OgeqApoFactory();
    if (!factory) {
        OGEQ_TRACE(L"DllGetClassObject: new factory failed -> E_OUTOFMEMORY");
        return E_OUTOFMEMORY;
    }
    HRESULT hr = factory->QueryInterface(riid, ppv);
    OGEQ_TRACE(L"DllGetClassObject: factory=%p, QI hr=0x%08X, ppv=%p", factory, hr, *ppv);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow() {
    return g_refCount == 0 ? S_OK : S_FALSE;
}

// Registration: writes the AudioEngine declaration + COM class.
// The UI (not the DLL) handles per-endpoint FxProperties.
STDAPI DllRegisterServer() {
    wchar_t dllPath[MAX_PATH];
    if (!GetModuleFileNameW(g_hInstance, dllPath, MAX_PATH))
        return HRESULT_FROM_WIN32(GetLastError());

    wchar_t clsidStr[64];
    StringFromGUID2(CLSID_OgeqApo, clsidStr, 64);

    // 1. COM class: HKCR\CLSID\{...}\InprocServer32
    wchar_t clsidKey[128];
    swprintf_s(clsidKey, L"CLSID\\%s", clsidStr);

    HKEY hKey;
    wchar_t inprocKey[160];
    swprintf_s(inprocKey, L"%s\\InprocServer32", clsidKey);
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, inprocKey, 0, nullptr, 0,
                        KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
        return E_FAIL;
    RegSetValueExW(hKey, nullptr, 0, REG_SZ, (BYTE*)dllPath,
                   (DWORD)((wcslen(dllPath) + 1) * sizeof(wchar_t)));
    const wchar_t* threading = L"Both";
    RegSetValueExW(hKey, L"ThreadingModel", 0, REG_SZ, (BYTE*)threading,
                   (DWORD)((wcslen(threading) + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);

    // 2. AudioEngine APO declaration:
    //    HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Audio\AudioEngine\AudioProcessingObjects\{...}
    wchar_t apoKey[256];
    swprintf_s(apoKey, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Audio\\AudioEngine\\AudioProcessingObjects\\%s", clsidStr);

    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, apoKey, 0, nullptr, 0,
                        KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
        return E_FAIL;

    const wchar_t* friendly = L"OGEQ Audio Equalizer";
    RegSetValueExW(hKey, L"FriendlyName", 0, REG_SZ, (BYTE*)friendly,
                   (DWORD)((wcslen(friendly) + 1) * sizeof(wchar_t)));

    const wchar_t* copyright = L"Copyright (c) 2026 OGEQ";
    RegSetValueExW(hKey, L"Copyright", 0, REG_SZ, (BYTE*)copyright,
                   (DWORD)((wcslen(copyright) + 1) * sizeof(wchar_t)));

    DWORD majorVer = 1, minorVer = 0;
    RegSetValueExW(hKey, L"MajorVersion", 0, REG_DWORD, (BYTE*)&majorVer, sizeof(majorVer));
    RegSetValueExW(hKey, L"MinorVersion", 0, REG_DWORD, (BYTE*)&minorVer, sizeof(minorVer));

    // Flags: 0 = default (not in-place specific, engine decides)
    DWORD flags = 0;
    RegSetValueExW(hKey, L"Flags", 0, REG_DWORD, (BYTE*)&flags, sizeof(flags));

    // NumAPOInterfaces must match the count of APOInterfaceN values.
    DWORD numIfaces = 1;
    RegSetValueExW(hKey, L"NumAPOInterfaces", 0, REG_DWORD, (BYTE*)&numIfaces, sizeof(numIfaces));

    // APOInterface0 = IID_IAudioProcessingObject (required)
    wchar_t iidStr[64];
    StringFromGUID2(__uuidof(IAudioProcessingObject), iidStr, 64);
    RegSetValueExW(hKey, L"APOInterface0", 0, REG_SZ, (BYTE*)iidStr,
                   (DWORD)((wcslen(iidStr) + 1) * sizeof(wchar_t)));

    // u32MaxInstances = 1 (24H2 graph-builder stability lesson from MiniEQ)
    DWORD maxInst = 1;
    RegSetValueExW(hKey, L"u32MaxInstances", 0, REG_DWORD, (BYTE*)&maxInst, sizeof(maxInst));

    // u32MinInputConnections / u32MaxInputConnections = 1
    // u32MinOutputConnections / u32MaxOutputConnections = 1
    DWORD one = 1;
    RegSetValueExW(hKey, L"u32MinInputConnections", 0, REG_DWORD, (BYTE*)&one, sizeof(one));
    RegSetValueExW(hKey, L"u32MaxInputConnections", 0, REG_DWORD, (BYTE*)&one, sizeof(one));
    RegSetValueExW(hKey, L"u32MinOutputConnections", 0, REG_DWORD, (BYTE*)&one, sizeof(one));
    RegSetValueExW(hKey, L"u32MaxOutputConnections", 0, REG_DWORD, (BYTE*)&one, sizeof(one));

    RegCloseKey(hKey);
    return S_OK;
}

STDAPI DllUnregisterServer() {
    wchar_t clsidStr[64];
    StringFromGUID2(CLSID_OgeqApo, clsidStr, 64);

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
