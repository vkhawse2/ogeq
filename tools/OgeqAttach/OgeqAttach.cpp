// OgeqAttach.cpp -- command-line endpoint attach/detach tool.
//
// Usage:
//   ogeq_attach.exe --endpoint {guid} --slot sfx|mfx|efx   (attach OGEQ APO)
//   ogeq_attach.exe --endpoint {guid} --detach             (remove OGEQ APO)
//
// Handles the locked-down FxProperties ACL by taking ownership first
// (SeTakeOwnershipPrivilege + SeRestorePrivilege), then granting
// Administrators full control, then writing the value.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <aclapi.h>

// OGEQ APO CLSID
static const wchar_t* kOgeqClsid = L"{b8122668-b395-481b-a516-03d4014e4421}";

// PKEY_AudioEndpointPlugin_FX_* property set
static const wchar_t* kFxPropSet = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d}";

static bool EnablePrivilege(const char* name) {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return false;
    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    if (!LookupPrivilegeValueA(nullptr, name, &tp.Privileges[0].Luid)) {
        CloseHandle(token);
        return false;
    }
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    BOOL ok = AdjustTokenPrivileges(token, FALSE, &tp, 0, nullptr, nullptr);
    CloseHandle(token);
    return ok && GetLastError() == ERROR_SUCCESS;
}

static std::wstring FxKeyPath(const std::wstring& endpointGuid) {
    return L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\Render\\"
         + endpointGuid + L"\\FxProperties";
}

// Take ownership of the key and grant Administrators full control.
// Returns true on success.
static bool TakeOwnership(const std::wstring& subkey) {
    EnablePrivilege(SE_TAKE_OWNERSHIP_NAME);
    EnablePrivilege(SE_RESTORE_NAME);

    // Open with WRITE_OWNER to change owner
    HKEY hKey = nullptr;
    LONG lr = RegOpenKeyExW(HKEY_LOCAL_MACHINE, subkey.c_str(), 0,
                            WRITE_OWNER | READ_CONTROL, &hKey);
    if (lr != ERROR_SUCCESS) {
        // Try with take-ownership flag
        lr = RegOpenKeyExW(HKEY_LOCAL_MACHINE, subkey.c_str(), 0,
                           WRITE_OWNER, &hKey);
        if (lr != ERROR_SUCCESS) {
            wprintf(L"RegOpenKeyEx(WRITE_OWNER) failed: %ld\n", lr);
            return false;
        }
    }

    // Set owner to Administrators
    PSID adminSid = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;
    if (!AllocateAndInitializeSid(&ntAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminSid)) {
        RegCloseKey(hKey);
        return false;
    }

    lr = RegSetKeySecurity(hKey, OWNER_SECURITY_INFORMATION,
                           (PSECURITY_DESCRIPTOR)&adminSid);  // placeholder, replaced below
    // Proper way: build a security descriptor with the new owner
    {
        SECURITY_DESCRIPTOR sd;
        InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorOwner(&sd, adminSid, FALSE);
        lr = RegSetKeySecurity(hKey, OWNER_SECURITY_INFORMATION, &sd);
    }
    FreeSid(adminSid);
    RegCloseKey(hKey);

    if (lr != ERROR_SUCCESS) {
        wprintf(L"RegSetKeySecurity(OWNER) failed: %ld\n", lr);
        return false;
    }

    // Now open with WRITE_DAC and grant Administrators full control
    lr = RegOpenKeyExW(HKEY_LOCAL_MACHINE, subkey.c_str(), 0,
                       WRITE_DAC | READ_CONTROL, &hKey);
    if (lr != ERROR_SUCCESS) {
        wprintf(L"RegOpenKeyEx(WRITE_DAC) failed: %ld\n", lr);
        return false;
    }

    // Get current DACL
    DWORD sdSize = 0;
    RegGetKeySecurity(hKey, DACL_SECURITY_INFORMATION, nullptr, &sdSize);
    std::string sdBuf(sdSize + 1024, 0);
    PSECURITY_DESCRIPTOR pSd = (PSECURITY_DESCRIPTOR)sdBuf.data();
    DWORD cbNeeded = (DWORD)sdBuf.size();
    lr = RegGetKeySecurity(hKey, DACL_SECURITY_INFORMATION, pSd, &cbNeeded);
    if (lr != ERROR_SUCCESS) {
        wprintf(L"RegGetKeySecurity failed: %ld\n", lr);
        RegCloseKey(hKey);
        return false;
    }

    // Build new DACL: keep existing, add Administrators Full Control
    PACL pOldDacl = nullptr;
    BOOL daclPresent = FALSE, daclDefaulted = FALSE;
    GetSecurityDescriptorDacl(pSd, &daclPresent, &pOldDacl, &daclDefaulted);

    PSID adminSid2 = nullptr;
    AllocateAndInitializeSid(&ntAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
                             DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminSid2);

    DWORD newAclSize = (pOldDacl ? pOldDacl->AclSize : sizeof(ACL)) + 1024;
    std::string aclBuf(newAclSize, 0);
    PACL pNewDacl = (PACL)aclBuf.data();
    InitializeAcl(pNewDacl, newAclSize, ACL_REVISION);

    // Copy existing ACEs
    if (pOldDacl && daclPresent) {
        for (DWORD i = 0; i < pOldDacl->AceCount; i++) {
            void* pAce = nullptr;
            if (GetAce(pOldDacl, i, &pAce))
                AddAce(pNewDacl, ACL_REVISION, MAXDWORD, pAce,
                       ((PACE_HEADER)pAce)->AceSize);
        }
    }
    // Add Administrators Full Control
    AddAccessAllowedAceEx(pNewDacl, ACL_REVISION,
                          CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE,
                          KEY_ALL_ACCESS, adminSid2);
    FreeSid(adminSid2);

    SECURITY_DESCRIPTOR newSd;
    InitializeSecurityDescriptor(&newSd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&newSd, TRUE, pNewDacl, FALSE);
    lr = RegSetKeySecurity(hKey, DACL_SECURITY_INFORMATION, &newSd);
    RegCloseKey(hKey);

    if (lr != ERROR_SUCCESS) {
        wprintf(L"RegSetKeySecurity(DACL) failed: %ld\n", lr);
        return false;
    }

    wprintf(L"Ownership taken, Administrators granted full control.\n");
    return true;
}

static int Attach(const std::wstring& endpointGuid, int slot) {
    std::wstring subkey = FxKeyPath(endpointGuid);
    wchar_t valueName[128];
    swprintf_s(valueName, L"%s},%d", kFxPropSet, slot);
    // kFxPropSet ends with }, so valueName = "{guid},N"
    // Fix: kFxPropSet already has the closing brace
    std::wstring propName = std::wstring(kFxPropSet) + L"," + std::to_wstring(slot);

    // Try direct write first
    HKEY hKey = nullptr;
    LONG lr = RegOpenKeyExW(HKEY_LOCAL_MACHINE, subkey.c_str(), 0, KEY_SET_VALUE, &hKey);
    if (lr == ERROR_ACCESS_DENIED) {
        wprintf(L"Access denied, taking ownership...\n");
        if (!TakeOwnership(subkey)) {
            wprintf(L"Failed to take ownership.\n");
            return 1;
        }
        lr = RegOpenKeyExW(HKEY_LOCAL_MACHINE, subkey.c_str(), 0, KEY_SET_VALUE, &hKey);
    }
    if (lr != ERROR_SUCCESS) {
        wprintf(L"RegOpenKeyEx failed: %ld\n", lr);
        return 1;
    }

    lr = RegSetValueExW(hKey, propName.c_str(), 0, REG_SZ,
                        (const BYTE*)kOgeqClsid,
                        (DWORD)((wcslen(kOgeqClsid) + 1) * sizeof(wchar_t)));

    if (lr != ERROR_SUCCESS) {
        wprintf(L"RegSetValueEx failed: %ld\n", lr);
        RegCloseKey(hKey);
        return 1;
    }

    // Windows 11 also reads the chain form (REG_MULTI_SZ) at slots 13/14/15.
    // Write our CLSID there too so Win11 picks it up.
    // Mapping: SFX 5->13, MFX 6->14, EFX 7->15
    int chainSlot = 0;
    if (slot == 5) chainSlot = 13;
    else if (slot == 6) chainSlot = 14;
    else if (slot == 7) chainSlot = 15;
    if (chainSlot != 0) {
        std::wstring chainProp = std::wstring(kFxPropSet) + L"," + std::to_wstring(chainSlot);
        // REG_MULTI_SZ: our CLSID + double null terminator
        size_t clsidLen = wcslen(kOgeqClsid);
        std::vector<wchar_t> multiSz(clsidLen + 2, L'\0');
        wcscpy_s(multiSz.data(), multiSz.size(), kOgeqClsid);
        // multiSz is now: CLSID\0\0
        lr = RegSetValueExW(hKey, chainProp.c_str(), 0, REG_MULTI_SZ,
                            (const BYTE*)multiSz.data(),
                            (DWORD)(multiSz.size() * sizeof(wchar_t)));
        if (lr == ERROR_SUCCESS) {
            wprintf(L"Also wrote Win11 chain slot %d\n", chainSlot);
        }
    }
    RegCloseKey(hKey);

    wprintf(L"Attached OGEQ APO to %s slot %d\n", endpointGuid.c_str(), slot);
    wprintf(L"Unplug/replug the device for the engine to pick it up.\n");
    return 0;
}

static int Detach(const std::wstring& endpointGuid) {
    std::wstring subkey = FxKeyPath(endpointGuid);
    int removed = 0;
    for (int slot = 5; slot <= 7; slot++) {
        std::wstring propName = std::wstring(kFxPropSet) + L"," + std::to_wstring(slot);
        HKEY hKey = nullptr;
        LONG lr = RegOpenKeyExW(HKEY_LOCAL_MACHINE, subkey.c_str(), 0, KEY_SET_VALUE, &hKey);
        if (lr == ERROR_ACCESS_DENIED) {
            if (!TakeOwnership(subkey)) continue;
            lr = RegOpenKeyExW(HKEY_LOCAL_MACHINE, subkey.c_str(), 0, KEY_SET_VALUE, &hKey);
        }
        if (lr != ERROR_SUCCESS) continue;

        // Only delete if it's ours
        wchar_t val[256]; DWORD cb = sizeof(val), type = 0;
        lr = RegQueryValueExW(hKey, propName.c_str(), nullptr, &type, (BYTE*)val, &cb);
        if (lr == ERROR_SUCCESS && type == REG_SZ && _wcsicmp(val, kOgeqClsid) == 0) {
            if (RegDeleteValueW(hKey, propName.c_str()) == ERROR_SUCCESS) {
                wprintf(L"Removed slot %d\n", slot);
                removed++;
            }
        }
        RegCloseKey(hKey);
    }
    wprintf(L"Detached (%d slot(s) cleared).\n", removed);
    return 0;
}

static void Usage() {
    wprintf(L"Usage:\n");
    wprintf(L"  ogeq_attach --endpoint {guid} --slot sfx|mfx|efx\n");
    wprintf(L"  ogeq_attach --endpoint {guid} --detach\n");
}

int wmain(int argc, wchar_t** argv) {
    std::wstring endpoint;
    std::wstring slotStr;
    bool detach = false;

    for (int i = 1; i < argc; i++) {
        std::wstring a = argv[i];
        if (a == L"--endpoint" && i + 1 < argc) endpoint = argv[++i];
        else if (a == L"--slot" && i + 1 < argc) slotStr = argv[++i];
        else if (a == L"--detach") detach = true;
        else { Usage(); return 1; }
    }

    if (endpoint.empty() || (!detach && slotStr.empty())) {
        Usage();
        return 1;
    }

    if (detach) return Detach(endpoint);

    int slot = 0;
    if (slotStr == L"sfx") slot = 5;
    else if (slotStr == L"mfx") slot = 6;
    else if (slotStr == L"efx") slot = 7;
    else { Usage(); return 1; }

    return Attach(endpoint, slot);
}
