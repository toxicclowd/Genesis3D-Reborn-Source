/****************************************************************************************/
/*  D3D12LOG.CPP                                                                        */
/*                                                                                      */
/*  Logging system implementation for DirectX 12 Driver                                */
/*                                                                                      */
/****************************************************************************************/
#include "D3D12Log.h"
#include <time.h>
#include <stdarg.h>

D3D12Log* D3D12Log::s_pInstance = nullptr;

D3D12Log::D3D12Log()
    : m_pLogFile(nullptr)
{
    fopen_s(&m_pLogFile, "Direct3D12Driver.log", "w");
    if (m_pLogFile)
    {
        time_t rawtime;
        struct tm timeinfo;
        char buffer[256];
        
        time(&rawtime);
        localtime_s(&timeinfo, &rawtime);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
        
        fprintf(m_pLogFile, "===========================================\n");
        fprintf(m_pLogFile, "DirectX 12 Driver Log\n");
        fprintf(m_pLogFile, "Started: %s\n", buffer);
        fprintf(m_pLogFile, "===========================================\n\n");
        fflush(m_pLogFile);
    }
}

D3D12Log::~D3D12Log()
{
    Close();
}

D3D12Log* D3D12Log::GetPtr()
{
    if (!s_pInstance)
    {
        s_pInstance = new D3D12Log();
    }
    return s_pInstance;
}

void D3D12Log::Destroy()
{
    if (s_pInstance)
    {
        delete s_pInstance;
        s_pInstance = nullptr;
    }
}

void D3D12Log::Printf(const char* format, ...)
{
    if (!m_pLogFile)
        return;
    
    va_list args;
    va_start(args, format);
    vfprintf(m_pLogFile, format, args);
    fprintf(m_pLogFile, "\n");
    fflush(m_pLogFile);
    va_end(args);
    
    // Also output to debugger
#ifdef _DEBUG
    char buffer[1024];
    va_start(args, format);
    vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
    va_end(args);
    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
#endif
}

void D3D12Log::Close()
{
    if (m_pLogFile)
    {
        fprintf(m_pLogFile, "\n===========================================\n");
        fprintf(m_pLogFile, "Log Closed\n");
        fprintf(m_pLogFile, "===========================================\n");
        fclose(m_pLogFile);
        m_pLogFile = nullptr;
    }
}
