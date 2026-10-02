// OgeqApo.h -- OGEQ Audio Processing Object (modern WDK API).
//
// Targets WDK 10.0.26100+ which removed CBaseAudioProcessingObject.
// Implements IAudioProcessingObject* directly with full COM aggregation.
//
// Design notes from MiniEQ lineage:
//  - Engine always creates us aggregated; factory must NOT return
//    CLASS_E_NOAGGREGATION.
//  - Static CRT only.
//  - u32MaxInstances = 1 (24H2 graph-builder stability).
//  - No file I/O, registry, or allocation on the RT path.

#pragma once

#include <audioenginebaseapo.h>
#include <audioclient.h>
#include "../AudioEngine/include/DspProcessor.h"
#include "../Shared/OgeqStatusChannel.h"

// {b8122668-b395-481b-a516-03d4014e4421}
DEFINE_GUID(CLSID_OgeqApo,
    0xb8122668, 0xb395, 0x481b, 0xa5, 0x16, 0x03, 0xd4, 0x01, 0x4e, 0x44, 0x21);

namespace ogeq {

class OgeqApo : public IAudioProcessingObject,
                public IAudioProcessingObjectConfiguration,
                public IAudioProcessingObjectRT,
                public IAgileObject {
public:
    OgeqApo(IUnknown* outer, HRESULT* hr);
    virtual ~OgeqApo();

    // IUnknown (delegating for aggregation)
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IAudioProcessingObject
    STDMETHODIMP Initialize(UINT32 cbDataSize, BYTE* pbyData) override;
    STDMETHODIMP GetInputChannelCount(UINT32* pu32ChannelCount) override;
    STDMETHODIMP GetLatency(HNSTIME* pTime) override;
    STDMETHODIMP GetRegistrationProperties(APO_REG_PROPERTIES** ppRegProps) override;
    STDMETHODIMP Reset() override;
    STDMETHODIMP IsInputFormatSupported(IAudioMediaType* pOutputFormat,
                                        IAudioMediaType* pRequestedInputFormat,
                                        IAudioMediaType** ppSupportedInputFormat) override;
    STDMETHODIMP IsOutputFormatSupported(IAudioMediaType* pInputFormat,
                                         IAudioMediaType* pRequestedOutputFormat,
                                         IAudioMediaType** ppSupportedOutputFormat) override;

    // IAudioProcessingObjectConfiguration
    STDMETHODIMP LockForProcess(UINT32 u32NumInputConnections,
                                APO_CONNECTION_DESCRIPTOR** ppInputConnections,
                                UINT32 u32NumOutputConnections,
                                APO_CONNECTION_DESCRIPTOR** ppOutputConnections) override;
    STDMETHODIMP UnlockForProcess() override;

    // IAudioProcessingObjectRT
    STDMETHODIMP_(void) APOProcess(UINT32 u32NumInputConnections,
                                   APO_CONNECTION_PROPERTY** ppInputConnections,
                                   UINT32 u32NumOutputConnections,
                                   APO_CONNECTION_PROPERTY** ppOutputConnections) override;
    STDMETHODIMP_(UINT32) CalcInputFrames(UINT32 u32OutputFrameCount) override;
    STDMETHODIMP_(UINT32) CalcOutputFrames(UINT32 u32InputFrameCount) override;

    // Non-delegating IUnknown for aggregation
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);
    STDMETHODIMP_(ULONG) NonDelegatingAddRef();
    STDMETHODIMP_(ULONG) NonDelegatingRelease();

private:
    HRESULT GetFloat32Format(IAudioMediaType* pFormat, WAVEFORMATEX** ppWfex);

    IUnknown* outer_;           // Controlling unknown (for aggregation)
    long ref_;                  // Non-delegating refcount
    long nonDelegatingRef_;     // Actually use ref_ for both; outer_ for delegating

    DspProcessor dsp_;
    StatusChannelWriter status_;
    bool locked_ = false;
    UINT32 channels_ = 2;
    UINT32 sampleRate_ = 48000;
    wchar_t endpointId_[64] = L"";  // GUID portion of MMDevice ID, e.g. {acc87fa7-...}
};

} // namespace ogeq
