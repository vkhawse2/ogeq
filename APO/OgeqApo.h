// OgeqApo.h -- OGEQ Audio Processing Object.
//
// Design notes carried over from the MiniEQ lineage:
//  - Full COM aggregation support (engine creates us aggregated; a
//    CLASS_E_NOAGGREGATION factory is skipped in total silence).
//  - Static CRT only (no runtime DLL dependency beside audiodg.exe).
//  - u32MaxInstances = 1 for EFX (UINT32_MAX correlated with graph-builder
//    instability on Win11 24H2).
//  - No file I/O, registry, or allocation on the RT path. Ever.
//  - Initialize() does the minimum: endpoint id + settings load.
//    Heavy work is deferred to first LockForProcess().

#pragma once

#include <audioenginebaseapo.h>
#include "../AudioEngine/include/DspProcessor.h"
#include "../Shared/OgeqStatusChannel.h"

// {b8122668-b395-481b-a516-03d4014e4421} -- generated 2026-10-02, never reuse MiniEQ's CLSID.
DEFINE_GUID(CLSID_OgeqApo,
    0xb8122668, 0xb395, 0x481b, 0xa5, 0x16, 0x03, 0xd4, 0x01, 0x4e, 0x44, 0x21);

namespace ogeq {

class OgeqApo : public CBaseAudioProcessingObject {
public:
    OgeqApo(IUnknown* outer, HRESULT* hr);
    virtual ~OgeqApo();

    // IAudioProcessingObject
    STDMETHODIMP Initialize(UINT32 cbDataSize, BYTE* pbyData) override;
    STDMETHODIMP LockForProcess(UINT32 u32NumInputConnections,
                                APO_CONNECTION_DESCRIPTOR** ppInputConnections,
                                UINT32 u32NumOutputConnections,
                                APO_CONNECTION_DESCRIPTOR** ppOutputConnections) override;
    STDMETHODIMP_(void) APOProcess(UINT32 u32NumInputConnections,
                                   APO_CONNECTION_PROPERTY** ppInputConnections,
                                   UINT32 u32NumOutputConnections,
                                   APO_CONNECTION_PROPERTY** ppOutputConnections) override;
    STDMETHODIMP UnlockForProcess() override;

    // IAudioSystemEffects3 (mute/volume hooks if needed later)
    // IAgileObject / FTM via CBaseAudioProcessingObject.

private:
    DspProcessor dsp_;
    StatusChannelWriter status_;
    bool locked_ = false;
    // Endpoint ID for the status channel (set from init data in production).
    // For Phase 2 testing, the UI passes it via APOInitSystemEffects3.
};

} // namespace ogeq
