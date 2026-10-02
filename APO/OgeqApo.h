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

// {OGEQ-APO-CLSID} -- generate a fresh GUID at first real build:
//   powershell -c "[guid]::NewGuid()"
// and replace the placeholder below. Do NOT reuse MiniEQ's CLSID.
DEFINE_GUID(CLSID_OgeqApo,
    0x00000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

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
    bool locked_ = false;
};

} // namespace ogeq
