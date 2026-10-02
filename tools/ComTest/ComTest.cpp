// Test COM instantiation of OGEQ APO
#include <windows.h>
#include <objbase.h>
#include <iostream>

// {b8122668-b395-481b-a516-03d4014e4421}
static const GUID CLSID_OgeqApo =
{ 0xb8122668, 0xb395, 0x481b, { 0xa5, 0x16, 0x03, 0xd4, 0x01, 0x4e, 0x44, 0x21 } };

int main() {
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        std::cout << "CoInitialize failed: 0x" << std::hex << hr << std::endl;
        return 1;
    }

    IUnknown* pUnk = nullptr;
    // Try non-aggregated first
    hr = CoCreateInstance(CLSID_OgeqApo, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IUnknown, (void**)&pUnk);
    if (SUCCEEDED(hr)) {
        std::cout << "CoCreateInstance (non-aggregated): SUCCESS" << std::endl;
        pUnk->Release();
    } else {
        std::cout << "CoCreateInstance (non-aggregated): FAILED 0x"
                  << std::hex << hr << std::endl;
    }

    CoUninitialize();
    return 0;
}
