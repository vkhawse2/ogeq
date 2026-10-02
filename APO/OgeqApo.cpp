// OgeqApo.cpp -- modern WDK APO implementation (no CBaseAudioProcessingObject).

#include "OgeqApo.h"
#include "OgeqTrace.h"
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

namespace ogeq {

//--------------------------------------------------------------------
// Construction / IUnknown (aggregation-aware)
//--------------------------------------------------------------------

OgeqApo::OgeqApo(IUnknown* outer, HRESULT* hr)
    : outer_(outer), ref_(1) {
    if (hr) *hr = S_OK;
    // Note: ref_ starts at 1 for the creator's reference.
}

OgeqApo::~OgeqApo() {
    status_.close();
}

// Delegating IUnknown: forward to outer if aggregated.
STDMETHODIMP OgeqApo::QueryInterface(REFIID riid, void** ppv) {
    if (outer_)
        return outer_->QueryInterface(riid, ppv);
    return NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP_(ULONG) OgeqApo::AddRef() {
    if (outer_)
        return outer_->AddRef();
    return NonDelegatingAddRef();
}

STDMETHODIMP_(ULONG) OgeqApo::Release() {
    if (outer_)
        return outer_->Release();
    return NonDelegatingRelease();
}

STDMETHODIMP OgeqApo::NonDelegatingQueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    if (riid == IID_IUnknown) {
        *ppv = static_cast<IAudioProcessingObject*>(this);
    } else if (riid == __uuidof(IAudioProcessingObject)) {
        *ppv = static_cast<IAudioProcessingObject*>(this);
    } else if (riid == __uuidof(IAudioProcessingObjectConfiguration)) {
        *ppv = static_cast<IAudioProcessingObjectConfiguration*>(this);
    } else if (riid == __uuidof(IAudioProcessingObjectRT)) {
        *ppv = static_cast<IAudioProcessingObjectRT*>(this);
    } else if (riid == __uuidof(IAgileObject)) {
        *ppv = static_cast<IAgileObject*>(this);
    } else {
        return E_NOINTERFACE;
    }
    NonDelegatingAddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) OgeqApo::NonDelegatingAddRef() {
    return InterlockedIncrement(&ref_);
}

STDMETHODIMP_(ULONG) OgeqApo::NonDelegatingRelease() {
    ULONG r = InterlockedDecrement(&ref_);
    if (r == 0) delete this;
    return r;
}

//--------------------------------------------------------------------
// IAudioProcessingObject
//--------------------------------------------------------------------

STDMETHODIMP OgeqApo::Initialize(UINT32 cbDataSize, BYTE* pbyData) {
    // Minimal: DSP defaults. No registry, no files, no threads.
    DspSettings defaults;
    defaults.numBands = 5;
    dsp_.applySettings(defaults);

    // Extract the real endpoint ID from APOInitSystemEffects2.
    // The struct has pDeviceCollection + nSoftwareIoDeviceInCollection.
    endpointId_[0] = L'\0';
    if (pbyData && cbDataSize >= sizeof(APOInitSystemEffects2)) {
        APOInitSystemEffects2* init = reinterpret_cast<APOInitSystemEffects2*>(pbyData);
        // Validate: first field is APOInitBaseStruct with cbSize
        if (init->APOInit.cbSize >= sizeof(APOInitSystemEffects2) && init->pDeviceCollection) {
            IMMDevice* pDevice = nullptr;
            if (SUCCEEDED(init->pDeviceCollection->Item(init->nSoftwareIoDeviceInCollection, &pDevice)) && pDevice) {
                LPWSTR devId = nullptr;
                if (SUCCEEDED(pDevice->GetId(&devId)) && devId) {
                    // devId looks like: {0.0.0.00000000}.{acc87fa7-c283-45f6-9ef3-66184fcc668e}
                    // Extract the GUID portion after the last '.'
                    const wchar_t* guidPart = wcsrchr(devId, L'.');
                    if (guidPart && guidPart[1] == L'{') {
                        wcsncpy_s(endpointId_, guidPart + 1, _TRUNCATE);
                    } else {
                        // Fallback: use the whole ID
                        wcsncpy_s(endpointId_, devId, _TRUNCATE);
                    }
                    CoTaskMemFree(devId);
                }
                pDevice->Release();
            }
        }
    }
    // If extraction failed, endpointId_ stays empty and LockForProcess
    // will use a fallback (hash of empty = deterministic but not per-endpoint).
    OGEQ_TRACE(L"Initialize: endpointId='%s'", endpointId_[0] ? endpointId_ : L"<none>");
    return S_OK;
}

STDMETHODIMP OgeqApo::GetInputChannelCount(UINT32* pu32ChannelCount) {
    if (!pu32ChannelCount) return E_POINTER;
    *pu32ChannelCount = 2;  // We support stereo; engine queries this
    return S_OK;
}

STDMETHODIMP OgeqApo::GetLatency(HNSTIME* pTime) {
    if (!pTime) return E_POINTER;
    *pTime = 0;  // No added latency (biquads are sample-by-sample)
    return S_OK;
}

STDMETHODIMP OgeqApo::GetRegistrationProperties(APO_REG_PROPERTIES** ppRegProps) {
    if (!ppRegProps) return E_POINTER;
    *ppRegProps = nullptr;
    return E_NOTIMPL;
}

STDMETHODIMP OgeqApo::Reset() {
    // Reset DSP state (called when the engine wants a clean slate).
    dsp_.configure((float)sampleRate_, (int)channels_);
    return S_OK;
}

// Helper: extract WAVEFORMATEX from IAudioMediaType.
// Returns a heap-allocated copy the caller must delete[] as BYTE*.
HRESULT OgeqApo::GetFloat32Format(IAudioMediaType* pFormat, WAVEFORMATEX** ppWfex) {
    if (!pFormat || !ppWfex) return E_POINTER;
    *ppWfex = nullptr;

    // Modern IAudioMediaType::GetAudioFormat returns const WAVEFORMATEX*
    const WAVEFORMATEX* wfex = pFormat->GetAudioFormat();
    if (!wfex) return E_POINTER;

    // Copy it (we need a mutable copy for our checks; actually just read it)
    UINT32 cbSize = sizeof(WAVEFORMATEX) + wfex->cbSize;
    BYTE* buf = new (std::nothrow) BYTE[cbSize];
    if (!buf) return E_OUTOFMEMORY;
    memcpy(buf, wfex, cbSize);

    *ppWfex = (WAVEFORMATEX*)buf;
    return S_OK;
}

STDMETHODIMP OgeqApo::IsInputFormatSupported(IAudioMediaType* pOutputFormat,
                                             IAudioMediaType* pRequestedInputFormat,
                                             IAudioMediaType** ppSupportedInputFormat) {
    (void)pOutputFormat;
    if (!pRequestedInputFormat) return E_POINTER;

    WAVEFORMATEX* wfex = nullptr;
    HRESULT hr = GetFloat32Format(pRequestedInputFormat, &wfex);
    if (FAILED(hr)) return hr;

    bool isFloat = (wfex->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
    bool validCh = (wfex->nChannels >= 1 && wfex->nChannels <= kMaxChannels);
    bool validRate = (wfex->nSamplesPerSec >= 8000 && wfex->nSamplesPerSec <= 192000);
    delete[] (BYTE*)wfex;

    if (!isFloat || !validCh || !validRate) {
        if (ppSupportedInputFormat) *ppSupportedInputFormat = nullptr;
        return APOERR_FORMAT_NOT_SUPPORTED;
    }

    // We accept the requested format as-is.
    if (ppSupportedInputFormat) {
        *ppSupportedInputFormat = pRequestedInputFormat;
        pRequestedInputFormat->AddRef();
    }
    return S_OK;
}

STDMETHODIMP OgeqApo::IsOutputFormatSupported(IAudioMediaType* pInputFormat,
                                              IAudioMediaType* pRequestedOutputFormat,
                                              IAudioMediaType** ppSupportedOutputFormat) {
    // We require output == input (in-place processing).
    return IsInputFormatSupported(pInputFormat, pRequestedOutputFormat, ppSupportedOutputFormat);
}

//--------------------------------------------------------------------
// IAudioProcessingObjectConfiguration
//--------------------------------------------------------------------

STDMETHODIMP OgeqApo::LockForProcess(UINT32 u32NumInputConnections,
                                     APO_CONNECTION_DESCRIPTOR** ppInputConnections,
                                     UINT32 u32NumOutputConnections,
                                     APO_CONNECTION_DESCRIPTOR** ppOutputConnections) {
    OGEQ_TRACE(L"LockForProcess: in=%u out=%u", u32NumInputConnections, u32NumOutputConnections);
    if (u32NumInputConnections != 1 || u32NumOutputConnections != 1)
        return E_INVALIDARG;
    if (!ppInputConnections || !ppOutputConnections)
        return E_POINTER;

    APO_CONNECTION_DESCRIPTOR* inDesc = ppInputConnections[0];
    APO_CONNECTION_DESCRIPTOR* outDesc = ppOutputConnections[0];
    if (!inDesc || !outDesc) return E_POINTER;

    // Modern API: pFormat is IAudioMediaType*
    WAVEFORMATEX* inWfex = nullptr;
    WAVEFORMATEX* outWfex = nullptr;
    HRESULT hr = GetFloat32Format(inDesc->pFormat, &inWfex);
    if (SUCCEEDED(hr)) hr = GetFloat32Format(outDesc->pFormat, &outWfex);

    if (FAILED(hr)) {
        delete[] (BYTE*)inWfex;
        delete[] (BYTE*)outWfex;
        return hr;
    }

    bool inFloat = (inWfex->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
    bool outFloat = (outWfex->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
    channels_ = inWfex->nChannels;
    sampleRate_ = inWfex->nSamplesPerSec;

    delete[] (BYTE*)inWfex;
    delete[] (BYTE*)outWfex;

    if (!inFloat || !outFloat)
        return APOERR_FORMAT_NOT_SUPPORTED;
    if (channels_ < 1 || channels_ > kMaxChannels)
        return APOERR_FORMAT_NOT_SUPPORTED;

    dsp_.configure((float)sampleRate_, (int)channels_);

    // Open per-endpoint status channel using the real endpoint GUID
    // extracted in Initialize. Falls back to a fixed ID if extraction failed.
    const wchar_t* epId = endpointId_[0] ? endpointId_
                        : L"{00000000-0000-0000-0000-000000000000}";
    status_.open(epId);
    status_.publishFormat(channels_, sampleRate_, S_OK);

    locked_ = true;
    return S_OK;
}

STDMETHODIMP OgeqApo::UnlockForProcess() {
    locked_ = false;
    status_.close();
    return S_OK;
}

//--------------------------------------------------------------------
// IAudioProcessingObjectRT
//--------------------------------------------------------------------

STDMETHODIMP_(void) OgeqApo::APOProcess(UINT32 u32NumInputConnections,
                                        APO_CONNECTION_PROPERTY** ppInputConnections,
                                        UINT32 u32NumOutputConnections,
                                        APO_CONNECTION_PROPERTY** ppOutputConnections) {
    if (!locked_ || u32NumInputConnections < 1 || u32NumOutputConnections < 1)
        return;
    if (!ppInputConnections || !ppOutputConnections)
        return;

    APO_CONNECTION_PROPERTY* inProp = ppInputConnections[0];
    APO_CONNECTION_PROPERTY* outProp = ppOutputConnections[0];
    if (!inProp || !outProp) return;

    float* inBuf = reinterpret_cast<float*>(inProp->pBuffer);
    float* outBuf = reinterpret_cast<float*>(outProp->pBuffer);
    UINT32 frames = inProp->u32ValidFrameCount;

    if (!inBuf || !outBuf || frames == 0) return;

    // Check silent flag (APO_BUFFER_FLAGS is now an enum class, use underlying)
    if ((UINT32)inProp->u32BufferFlags & (UINT32)BUFFER_SILENT) {
        outProp->u32BufferFlags = (APO_BUFFER_FLAGS)((UINT32)outProp->u32BufferFlags | (UINT32)BUFFER_SILENT);
        outProp->u32ValidFrameCount = frames;
        return;
    }

    dsp_.process(inBuf, outBuf, frames);

    outProp->u32ValidFrameCount = frames;
    outProp->u32BufferFlags = (APO_BUFFER_FLAGS)((UINT32)outProp->u32BufferFlags & ~(UINT32)BUFFER_SILENT);

    status_.heartbeat();
}

STDMETHODIMP_(UINT32) OgeqApo::CalcInputFrames(UINT32 u32OutputFrameCount) {
    return u32OutputFrameCount;  // 1:1 (no resampling)
}

STDMETHODIMP_(UINT32) OgeqApo::CalcOutputFrames(UINT32 u32InputFrameCount) {
    return u32InputFrameCount;  // 1:1
}

} // namespace ogeq
