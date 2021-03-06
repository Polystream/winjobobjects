// a simple, but hopefully instructive, example of how to use job objects in Windows
//

#include "stdafx.h"
#include <Windows.h>
#include <aclapi.h>
#pragma comment(lib, "advapi32.lib")

#include <iostream>

HANDLE job = nullptr;
PSID creatorSID = nullptr;
SID_IDENTIFIER_AUTHORITY SIDcreator = SECURITY_CREATOR_SID_AUTHORITY;
SECURITY_ATTRIBUTES creatorSa;
PACL acl = nullptr;
PSECURITY_DESCRIPTOR sd = nullptr;

/// create the security attributes we need to be able to control the job and the processes inside it 
bool CreateSA()
{
    if (AllocateAndInitializeSid(&SIDcreator, 1, SECURITY_CREATOR_OWNER_RID, 0, 0, 0, 0, 0, 0, 0, &creatorSID))
    {
        EXPLICIT_ACCESS ea;
        // we want to be able to control every aspect of the job
        ea.grfAccessPermissions = JOB_OBJECT_ALL_ACCESS;
        ea.grfAccessMode = SET_ACCESS;
        // we're not letting this propagate to new jobs created under this one, but we could
        ea.grfInheritance = NO_INHERITANCE;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea.Trustee.ptstrName = LPWSTR(creatorSID);

        auto res = SetEntriesInAcl(1, &ea, nullptr, &acl);
        if (SUCCEEDED(res))
        {
            sd = PSECURITY_DESCRIPTOR(LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH));
            if (sd && InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION))
            {
                if (SetSecurityDescriptorDacl(sd, TRUE, acl, FALSE))
                {
                    creatorSa.nLength = sizeof(creatorSa);
                    creatorSa.bInheritHandle = FALSE;
                    creatorSa.lpSecurityDescriptor = sd;
                    return true;
                }
            }
        }
    }
    return false;
}


/// create a job using our security object and set its properties
bool CreateJob()
{
    job = CreateJobObject(&creatorSa, nullptr);
    if (job)
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobExtInfo;
        ZeroMemory(&jobExtInfo, sizeof(jobExtInfo));
        // we want to terminate all sub processes inside this job when the job itself is closed
        jobExtInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (SetInformationJobObject(job, JobObjectExtendedLimitInformation, &jobExtInfo, sizeof(jobExtInfo)))
        {
            return true;
        }
    }
    return false;
}

/// extend https://msdn.microsoft.com/en-us/library/windows/desktop/ms684150(v=vs.85).aspx 
/// to add some room for the process IDs
struct JobjectProcessIdList : JOBOBJECT_BASIC_PROCESS_ID_LIST
{
    static constexpr int PROCESS_LIST_SiZE = 16;
    ULONG_PTR ProcessIdList2[PROCESS_LIST_SiZE];
};

/// query for, and list, the processes running inside this job and list the names of the exes
/// NOTE: to be able to do this we have had to create the security group with JOB_OBJECT_QUERY access right
void GetProcessesInJob()
{
    JobjectProcessIdList jinfo;
    ZeroMemory(&jinfo, sizeof(jinfo));
    jinfo.NumberOfAssignedProcesses = JobjectProcessIdList::PROCESS_LIST_SiZE+1;
    if ( QueryInformationJobObject(job, JobObjectBasicProcessIdList, &jinfo, sizeof(jinfo), nullptr))
    {
        std::cout << "processes in this job;\n";
        for ( DWORD n = 0; n < jinfo.NumberOfProcessIdsInList; ++n )
        {
            auto ph = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, jinfo.ProcessIdList[n]);
            TCHAR name[1024];
            DWORD length = sizeof(name);
            if (ph && QueryFullProcessImageName(ph,0,name,&length) )
            {
#ifdef UNICODE
                std::wcout << "\t" << name << "\n";
#else
                std::cout << "\t" << name << "\n";
#endif
            }
        }
    }
}

/// query for, and list, the processes running inside this job and wait for them to die
/// NOTE: to be able to do this we have had to create the security group with JOB_OBJECT_QUERY access right
void WaitForProcessesInJob()
{
    std::cout << "waiting for all processes in this job to terminate....please make sure they do" << std::endl;
    WaitForSingleObject(job, INFINITE);
}

/// this app launches notepad.exe to demonstrate how we can control jobs with multiple, nested, processes
auto NOTEPAD_EXE_PATH = TEXT("..\\debug\\testapp.exe");
auto NOTEPAD_WORK_DIR = nullptr;

auto EXE_PATH = NOTEPAD_EXE_PATH;
auto WORK_DIR = NOTEPAD_WORK_DIR;

// lanch out little test exectuable
bool LaunchExe()
{
    PROCESS_INFORMATION pi = { nullptr };
    STARTUPINFO si = { 0 };
    //NOTE: we specify NEW_CONSOLE here just so that we get another window for the testapp, it's not requried for the job to work
    auto handle = CreateProcess(EXE_PATH, nullptr, nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, WORK_DIR, &si, &pi);
    if (handle)
    {
        // assign the newly created (and now running) process to our job
        if (AssignProcessToJobObject(job, pi.hProcess))
        {
            return true;
        }
    }
    return false;
}

void Cleanup()
{
    if (creatorSID)
        FreeSid(creatorSID);
    if (acl)
        LocalFree(acl);
    if (sd)
        LocalFree(sd);
    if (job)
        CloseHandle(job);
}

bool SetupAndLaunchTestExe()
{
    std::cout << "creating security group...\n";
    if (!CreateSA())
    {
        std::cerr << "failed to create security descriptor; 0x" << std::hex << GetLastError() << std::endl;
        Cleanup();
        return false;
    }
    std::cout << "creating job...\n";
    if (!CreateJob())
    {
        std::cerr << "failed to create job; 0x" << std::hex << GetLastError() << std::endl;
        Cleanup();
        return false;
    }
    std::cout << "launching our wee test app...\n";
    if (!LaunchExe())
    {
        std::cerr << "failed to launch " << EXE_PATH << ";  0x" << std::hex << GetLastError() << std::endl;
        Cleanup();
        return false;
    }
    return true;
}

int main()
{
    auto res = -1;
    std::cout << "Test 1: launch processes and kill via job\n";
    if (SetupAndLaunchTestExe())
    {
        std::cout << "...letting the spawned processes run for a little...\n";
        Sleep(1000);
        GetProcessesInJob();
        Sleep(5000);
        //WaitForSingleObject(job, INFINITE);

        std::cout << "terminating all processes inside the job...watch them go\n";
        // kill all the processes in the job with extreme prejudice
        TerminateJobObject(job, 0);

        Cleanup();
        res = 0;
    }

    res = -1;
    std::cout << "Test 2: launch processes and wait for them to die (CLOSE NOTEPAD AND THE TEST CONSOLE APP TO COMPLETE THIS)\n";
    if (SetupAndLaunchTestExe())
    {
        WaitForProcessesInJob();
        Cleanup();
        res = 0;
    }

    return res;
}