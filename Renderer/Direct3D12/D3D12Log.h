/****************************************************************************************/
/*  D3D12LOG.H                                                                          */
/*                                                                                      */
/*  Logging system for DirectX 12 Driver                                               */
/*                                                                                      */
/****************************************************************************************/
#ifndef D3D12LOG_H
#define D3D12LOG_H

#include <windows.h>
#include <stdio.h>

class D3D12Log
{
private:
    FILE* m_pLogFile;
    static D3D12Log* s_pInstance;
    
    D3D12Log();
    ~D3D12Log();

public:
    static D3D12Log* GetPtr();
    static void Destroy();
    
    void Printf(const char* format, ...);
    void Close();
};

#endif // D3D12LOG_H
