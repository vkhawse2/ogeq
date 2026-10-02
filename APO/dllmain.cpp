// dllmain.cpp -- DLL entry point, class factory, registration.
//
// COM aggregation: the engine creates us with a non-null outer IUnknown.
// Our factory must NOT return CLASS_E_NOAGGREGATION -- that's the silent
// skip that plagued early APO attempts industry-wide.

#include "OgeqApo.h"
#include <audioenginebaseapo.h>

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

    // IClassFactory -- aggregation is REQUIRED (engine always aggregates).
    STDMETHODIMP CreateInstance(IUnknown* pOuter, REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;

        // Only IUnknown can be requested when aggregated.
        if (pOuter && riid != IID_IUnknown)
            return E_NOINTERFACE;

        HRESULT hr = S_OK;
        ogeq::OgeqApo* apo = new (std::nothrow) ogeq::OgeqApo(pOuter, &hr);
        if (!apo) return E_OUTOFMEMORY;
        if (FAILED(hr)) { delete apo; return hr; }

        // If aggregated, return the inner IUnknown; else QI for the requested IID.
        if (pOuter) {
            *ppv = static_cast<IUnknown*>(apo);
            // CBaseAudioProcessingObject already AddRef'd for the inner unknown.
        } else {
            hr = apo->QueryInterface(riid, ppv);
            apo->Release(); // QI AddRef'd; balance the constructor ref.
        }
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
    if (rclsid != CLSID_OgeqApo) return CLASS_E_CLASSNOTAVAILABLE;
    OgeqApoFactory* factory = new (std::nothrow) OgeqApoFactory();
    if (!factory) return E_OUTOFMEMORY;
    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow() {
    return g_refCount == 0 ? S_OK : S_FALSE;
}

// Registration: writes the AudioEngine declaration + COM class.
// The UI (not the DLL) handles per-endpoint FxProperties.
STDAPI DllRegisterServer() {
    // TODO(Phase 2): write
    //   HKCR\CLSID\{b8122668-...}\InprocServer32 = <dll path>, ThreadingModel=Both
    //   HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Audio\AudioEngine\AudioProcessingObjects\{b8122668-...}
    //     with FriendlyName, APOInterface0={...IAudioProcessingObject...}, etc.
    return E_NOTIMPL;
}

STDAPI DllUnregisterServer() {
    // TODO(Phase 2): remove the above keys.
    return E_NOTIMPL;
}

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hInstance = hInstance;
        DisableThreadLibraryCalls(hInstance);
    }
    return TRUE;
}
