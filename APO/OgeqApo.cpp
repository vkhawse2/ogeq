// OgeqApo.cpp -- OGEQ Audio Processing Object implementation.
//
// Based on the MiniEQ APO lineage (which proved COM loading works) with:
//  - u32MaxInstances = 1 (24H2 graph-builder stability)
//  - Per-endpoint status channel (not one global mapping)
//  - No file I/O, registry, or allocation on any APO thread
//  - Initialize() is minimal; heavy work deferred to LockForProcess()

#include "OgeqApo.h"
#include "../Shared/OgeqIpc.h"

#include <audioenginebaseapo.h>
#include <mmdeviceapi.h>

namespace ogeq {

//--------------------------------------------------------------------
// OgeqApo
//--------------------------------------------------------------------

OgeqApo::OgeqApo(IUnknown* outer, HRESULT* hr)
    : CBaseAudioProcessingObject(outer, hr) {
    // CBaseAudioProcessingObject handles aggregation, FTM, IAgileObject.
    // We add IAudioSystemEffects3 via QueryInterface override if needed.
}

OgeqApo::~OgeqApo() {
    // Status channel cleanup happens in UnlockForProcess / destructor.
    // No blocking calls here.
}

STDMETHODIMP OgeqApo::Initialize(UINT32 cbDataSize, BYTE* pbyData) {
    // Minimal: store init data (endpoint GUID comes via APOInitSystemEffects3
    // or property store in production; for now, accept empty).
    //
    // DO NOT: touch the registry, filesystem, or create threads here.
    // The engine calls this during graph building -- keep it fast.

    HRESULT hr = CBaseAudioProcessingObject::Initialize(cbDataSize, pbyData);
    if (FAILED(hr)) return hr;

    // Default DSP settings: flat EQ, 5 bands. The UI pushes real settings
    // via the shared-memory channel after LockForProcess.
    DspSettings defaults;
    defaults.numBands = 5;
    dsp_.applySettings(defaults);

    return S_OK;
}

STDMETHODIMP OgeqApo::LockForProcess(
    UINT32 u32NumInputConnections,
    APO_CONNECTION_DESCRIPTOR** ppInputConnections,
    UINT32 u32NumOutputConnections,
    APO_CONNECTION_DESCRIPTOR** ppOutputConnections) {

    // Validate: we require exactly 1 input and 1 output (EFX/SFX sysfx).
    if (u32NumInputConnections != 1 || u32NumOutputConnections != 1)
        return APOERR_INVALID_CONNECTION_COUNT;

    APO_CONNECTION_DESCRIPTOR* inDesc = ppInputConnections[0];
    APO_CONNECTION_DESCRIPTOR* outDesc = ppOutputConnections[0];

    // We only process float32. The engine on Win11 always offers float32
    // for sysfx; if not, reject with a clear HRESULT (telemetry, not silent).
    if (inDesc->u32Type != APO_CONNECTION_BUFFER_TYPE_EXTERNAL ||
        outDesc->u32Type != APO_CONNECTION_BUFFER_TYPE_EXTERNAL)
        return APOERR_FORMAT_NOT_SUPPORTED;

    WAVEFORMATEX* inFmt = inDesc->pFormat;
    WAVEFORMATEX* outFmt = outDesc->pFormat;

    if (!inFmt || !outFmt)
        return APOERR_FORMAT_NOT_SUPPORTED;

    bool inFloat = (inFmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                   (inFmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                    reinterpret_cast<WAVEFORMATEXTENSIBLE*>(inFmt)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    bool outFloat = (outFmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                    (outFmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                     reinterpret_cast<WAVEFORMATEXTENSIBLE*>(outFmt)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);

    if (!inFloat || !outFloat)
        return APOERR_FORMAT_NOT_SUPPORTED;

    // Configure DSP for the stream format.
    UINT32 channels = inFmt->nChannels;
    UINT32 sampleRate = inFmt->nSamplesPerSec;

    if (channels < 1 || channels > kMaxChannels)
        return APOERR_FORMAT_NOT_SUPPORTED;
    if (sampleRate < 8000 || sampleRate > 192000)
        return APOERR_FORMAT_NOT_SUPPORTED;

    dsp_.configure((float)sampleRate, (int)channels);

    // Open per-endpoint status channel for heartbeat telemetry.
    // The endpoint ID comes from init data in production; for Phase 2
    // bring-up we use a placeholder until the UI passes the real ID.
    // TODO(Phase 2): extract endpoint GUID from APOInitSystemEffects3.
    status_.open(L"{00000000-0000-0000-0000-000000000000}");
    status_.publishFormat(channels, sampleRate, S_OK);

    HRESULT hr = CBaseAudioProcessingObject::LockForProcess(
        u32NumInputConnections, ppInputConnections,
        u32NumOutputConnections, ppOutputConnections);
    if (SUCCEEDED(hr)) {
        locked_ = true;
    }
    return hr;
}

STDMETHODIMP_(void) OgeqApo::APOProcess(
    UINT32 u32NumInputConnections,
    APO_CONNECTION_PROPERTY** ppInputConnections,
    UINT32 u32NumOutputConnections,
    APO_CONNECTION_PROPERTY** ppOutputConnections) {

    // Real-time: no allocation, no locks, no syscalls.
    // In-place processing (input and output may alias).

    if (!locked_ || u32NumInputConnections < 1 || u32NumOutputConnections < 1)
        return;

    APO_CONNECTION_PROPERTY* inProp = ppInputConnections[0];
    APO_CONNECTION_PROPERTY* outProp = ppOutputConnections[0];

    if (!inProp || !outProp)
        return;

    // Silence flags: if input is silent, output silence.
    if (inProp->u32Signature != APO_CONNECTION_PROPERTY_V1_SIGNATURE ||
        outProp->u32Signature != APO_CONNECTION_PROPERTY_V1_SIGNATURE)
        return;

    float* inBuf = reinterpret_cast<float*>(inProp->pBuffer);
    float* outBuf = reinterpret_cast<float*>(outProp->pBuffer);
    UINT32 frames = inProp->u32ValidFrameCount;

    if (!inBuf || !outBuf || frames == 0)
        return;

    if (inProp->u32BufferFlags & BUFFER_SILENT) {
        outProp->u32BufferFlags |= BUFFER_SILENT;
        outProp->u32ValidFrameCount = frames;
        return;
    }

    // Process through DSP.
    dsp_.process(inBuf, outBuf, frames);

    outProp->u32ValidFrameCount = frames;
    outProp->u32BufferFlags &= ~BUFFER_SILENT;

    // Heartbeat: RT-safe seqlock bump.
    status_.heartbeat();
}

STDMETHODIMP OgeqApo::UnlockForProcess() {
    locked_ = false;
    status_.close();
    return CBaseAudioProcessingObject::UnlockForProcess();
}

} // namespace ogeq
