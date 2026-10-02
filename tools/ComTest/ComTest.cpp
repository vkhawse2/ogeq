// Test COM instantiation of OGEQ APO
#include <windows.h>
#include <objbase.h>
#include <iostream>

// {cf5483d7-d830-4fbb-b6c2-1a7753ea9db4}
static const GUID CLSID_OgeqApo =
{ 0xcf5483d7, 0xd830, 0x4fbb, { 0xb6, 0xc2, 0x1a, 0x77, 0x53, 0xea, 0x9d, 0xb4 } };

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
