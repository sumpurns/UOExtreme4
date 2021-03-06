#include <windows.h>


#define MakePtr( cast, ptr, addValue ) (cast)( (DWORD)(ptr)+(DWORD)(addValue))

PROC WINAPI HookImportedFunction(HMODULE hFromModule, PSTR pszFunctionModule, PSTR pszFunctionName, PROC pfnNewProc) {
    PROC pfnOriginalProc;
    PIMAGE_DOS_HEADER pDosHeader;
    PIMAGE_NT_HEADERS pNTHeader;
    PIMAGE_IMPORT_DESCRIPTOR pImportDesc;
    PIMAGE_THUNK_DATA pThunk;

    if ( IsBadCodePtr(pfnNewProc) ) // Verify that a valid pfn was passed
        return 0;
    
    // First, verify the the module and function names passed to use are valid
    pfnOriginalProc = GetProcAddress( GetModuleHandle(pszFunctionModule),
                                      pszFunctionName );
    if ( !pfnOriginalProc )
        return 0;
    
    
    pDosHeader = (PIMAGE_DOS_HEADER)hFromModule;            // other

    // Tests to make sure we're looking at a module image (the 'MZ' header)
    if ( IsBadReadPtr(pDosHeader, sizeof(IMAGE_DOS_HEADER)) )
        return 0;
    if ( pDosHeader->e_magic != IMAGE_DOS_SIGNATURE )
        return 0;

    // The MZ header has a pointer to the PE header
    pNTHeader = MakePtr(PIMAGE_NT_HEADERS, pDosHeader, pDosHeader->e_lfanew);

    // More tests to make sure we're looking at a "PE" image
    if ( IsBadReadPtr(pNTHeader, sizeof(IMAGE_NT_HEADERS)) )
        return 0;
    if ( pNTHeader->Signature != IMAGE_NT_SIGNATURE )
        return 0;

    // We know have a valid pointer to the module's PE header.  Now go
    // get a pointer to its imports section (.idata)
    pImportDesc = MakePtr(PIMAGE_IMPORT_DESCRIPTOR, pDosHeader, 
                            pNTHeader->OptionalHeader.
                            DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].
                            VirtualAddress);
                        
    // Bail out if the RVA of the imports section is 0 (it doesn't exist)
    if ( pImportDesc == (PIMAGE_IMPORT_DESCRIPTOR)pNTHeader )
        return FALSE;

    // Iterate through the array of imported module descriptors, looking
    // for the module whose name matches the pszFunctionModule parameter
    while ( pImportDesc->Name )
    {
        PSTR pszModName = MakePtr(PSTR, pDosHeader, pImportDesc->Name);
        
        if ( stricmp(pszModName, pszFunctionModule) == 0 )
            break;

        pImportDesc++;  // Advance to next imported module descriptor
    }
    
    // Bail out if we didn't find the import module descriptor for the
    // specified module.  pImportDesc->Name will be non-zero if we found it.
    if ( pImportDesc->Name == 0 )
        return 0;

    // Get a pointer to the found module's import address table (IAT)
    pThunk = MakePtr(PIMAGE_THUNK_DATA, pDosHeader, pImportDesc->FirstThunk);

    // Blast through the table of import addresses, looking for the one
    // that matches the address we got back from GetProcAddress above.
    while ( pThunk->u1.Function )
    {
        if ( pThunk->u1.Function == (PDWORD)pfnOriginalProc )
        {
            // We found it!  Overwrite the original address with the
            // address of the interception function.  Return the original
            // address to the caller so that they can chain on to it.
            pThunk->u1.Function = (PDWORD)pfnNewProc;
            return pfnOriginalProc;
        }
        
        pThunk++;   // Advance to next imported function address
    }
    
    return 0;   // Function not found
}


PROC WINAPI UnhookImportedFunction(HMODULE hFromModule, PSTR pszFunctionModule, PROC pfnHookProc, PROC pfnOriginalProc) {
    PIMAGE_DOS_HEADER pDosHeader;
    PIMAGE_NT_HEADERS pNTHeader;
    PIMAGE_IMPORT_DESCRIPTOR pImportDesc;
    PIMAGE_THUNK_DATA pThunk;

    if ( IsBadCodePtr(pfnOriginalProc) ) // Verify that a valid pfn was passed
        return 0;
       
    pDosHeader = (PIMAGE_DOS_HEADER)hFromModule;            // other

    // Tests to make sure we're looking at a module image (the 'MZ' header)
    if ( IsBadReadPtr(pDosHeader, sizeof(IMAGE_DOS_HEADER)) )
        return 0;
    if ( pDosHeader->e_magic != IMAGE_DOS_SIGNATURE )
        return 0;

    // The MZ header has a pointer to the PE header
    pNTHeader = MakePtr(PIMAGE_NT_HEADERS, pDosHeader, pDosHeader->e_lfanew);

    // More tests to make sure we're looking at a "PE" image
    if ( IsBadReadPtr(pNTHeader, sizeof(IMAGE_NT_HEADERS)) )
        return 0;
    if ( pNTHeader->Signature != IMAGE_NT_SIGNATURE )
        return 0;

    // We know have a valid pointer to the module's PE header.  Now go
    // get a pointer to its imports section (.idata)
    pImportDesc = MakePtr(PIMAGE_IMPORT_DESCRIPTOR, pDosHeader, 
                            pNTHeader->OptionalHeader.
                            DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].
                            VirtualAddress);
                        
    // Bail out if the RVA of the imports section is 0 (it doesn't exist)
    if ( pImportDesc == (PIMAGE_IMPORT_DESCRIPTOR)pNTHeader )
        return FALSE;

    // Iterate through the array of imported module descriptors, looking
    // for the module whose name matches the pszFunctionModule parameter
    while ( pImportDesc->Name )
    {
        PSTR pszModName = MakePtr(PSTR, pDosHeader, pImportDesc->Name);
        
        if ( stricmp(pszModName, pszFunctionModule) == 0 )
            break;

        pImportDesc++;  // Advance to next imported module descriptor
    }
    
    // Bail out if we didn't find the import module descriptor for the
    // specified module.  pImportDesc->Name will be non-zero if we found it.
    if ( pImportDesc->Name == 0 )
        return 0;

    // Get a pointer to the found module's import address table (IAT)
    pThunk = MakePtr(PIMAGE_THUNK_DATA, pDosHeader, pImportDesc->FirstThunk);

    // Blast through the table of import addresses, looking for the one
    // that matches the address we got back from GetProcAddress above.
    while ( pThunk->u1.Function )
    {
        if ( pThunk->u1.Function == (PDWORD)pfnHookProc )
        {
            // We found it!  Overwrite the original address with the
            // address of the interception function.  Return the original
            // address to the caller so that they can chain on to it.
            pThunk->u1.Function = (PDWORD)pfnOriginalProc;
            return pfnOriginalProc;
        }
        
        pThunk++;   // Advance to next imported function address
    }
    
    return 0;   // Function not found
}
