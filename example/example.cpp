// example.cpp : Defines the entry point for the console application.
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

bool CreateSA()
{
    if (AllocateAndInitializeSid(&SIDcreator, 1, SECURITY_CREATOR_OWNER_RID, 0, 0, 0, 0, 0, 0, 0, &creatorSID))
    {
        EXPLICIT_ACCESS ea;
        ea.grfAccessPermissions = JOB_OBJECT_ALL_ACCESS | JOB_OBJECT_QUERY;
        ea.grfAccessMode = SET_ACCESS;
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

void CleanupSA()
{
    if (creatorSID)
        FreeSid(creatorSID);
    if (acl)
        LocalFree(acl);
    if (sd)
        LocalFree(sd);
}

void CreateJob()
{
    job = CreateJobObject(&creatorSa, nullptr);
    if (job)
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobExtInfo;
        ZeroMemory(&jobExtInfo, sizeof(jobExtInfo));
        jobExtInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE; //TODO: needs different info | JOB_OBJECT_QUERY;
        if (SetInformationJobObject(job, JobObjectExtendedLimitInformation, &jobExtInfo, sizeof(jobExtInfo)))
        {
            std::cout << "ok, we're good\n";
        }
    }
}

auto NOTEPAD_EXE_PATH = TEXT("..\\..\\testapp\\debug\\testapp.exe");
auto NOTEPAD_WORK_DIR = nullptr;

auto EXE_PATH = NOTEPAD_EXE_PATH;
auto WORK_DIR = NOTEPAD_WORK_DIR;

void LaunchExe()
{
    PROCESS_INFORMATION pi = { 0 };
    STARTUPINFO si = { 0 };
    auto handle = CreateProcess(EXE_PATH, nullptr, nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, WORK_DIR, &si, &pi);
    if (handle)
    {
        if (AssignProcessToJobObject(job, pi.hProcess))
        {
            Sleep(5000);
        }
    }
}

int main()
{
    CreateSA();
    CreateJob();
    LaunchExe();
    //WaitForSingleObject(job, INFINITE);
    if (job)
    {
        std::cout << "terminating all processes inside the job...watch them go\n";
        // kill all the processes in the job with extreme prejudice
        TerminateJobObject(job, 0);
        CloseHandle(job);
    }

    CleanupSA();
    return 0;
}