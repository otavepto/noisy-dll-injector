// reference: https://www.codeproject.com/Articles/4610/Three-Ways-to-Inject-Your-Code-into-Another-Proces

#include <Windows.h>
#include <Psapi.h>    // GetProcessImageFileNameW(...)
#include <string>     // std::stoull(...)
#include <cwchar>     // wcsrchr(...)


// exit error codes
#define ERROR_INVALID_ARGS_COUNT             0xD11A45C0
#define ERROR_INVALID_3RD_ARG                0xD11A4530
#define ERROR_INVALID_1ST_ARG                0xD11A4510
#define ERROR_CREATEPROC_FAILED              0xD11C4EA1
#define ERROR_OPENPROC_FAILED                0xD1109E49
#define ERROR_DLL_FULL_PATH_FAILED           0xD119A140
#define ERROR_WRITEPROC_FAILED               0xD11411E0
#define ERROR_VIRTUALALLOC_FAILED            0xD1171410
#define ERROR_CREATEREMOTETH_FAILED          0xD114E301
#define ERROR_RESUMETHREAD_FAILED            0xD114E503
#define ERROR_ALL_SUCCEEDED                  0x00000000

// uncategorized static numerics
#define ARGS_MIN_COUNT                       4
#define MAX_FILE_PATH_FILENAME_COUNT         4096
#define MAX_PROCESS_ARGS_CHARS_COUNT         32767


// buffers used throughout the code
wchar_t dllPathAndFilename[MAX_FILE_PATH_FILENAME_COUNT]     = { 0 }; // contains the full path + filename of the injected .dll
wchar_t processPathAndFilename[MAX_FILE_PATH_FILENAME_COUNT] = { 0 }; // contains the full path + filename of the created/opened process


/*
 * Returns TRUE if the file exists, FALSE otherwise
 *
 * file: [in] Fully qualified path + filename
 */
BOOL File_IsExist(const wchar_t* const file)
{
   DWORD fileAttr;
   
   fileAttr = GetFileAttributesW(file);

   return (fileAttr != INVALID_FILE_ATTRIBUTES) && // file exists
          (fileAttr != FILE_ATTRIBUTE_DIRECTORY);  // and it's not a folder
}

/*
 * Retreives the fully qualified path + filename of a given file
 *
 * filename:                            [in] Relative or fully qualified filename
 * outPathAndFilenameBuffer:            [out] Output buffer holding fully qualified path + filename
 * bufferCharsCount:                    [in] Previous buffer count in characters
 * (optional) outPathAndFilenameCount:  [out] Output buffer holding count of fully qualified path + filename in characters, not including null-terminator
 * (optional) outFilenameCount:         [out] Output buffer holding count of filename in characters, not including null-terminator
 * (optional) outFilenamePos:           [out] Output buffer holding position (address) of the filename part in outPathAndFilenameBuffer
 */
BOOL File_GetFullPathAndFilename(const wchar_t* const filename, wchar_t* outPathAndFilenameBuffer, const DWORD bufferCharsCount, DWORD* outPathAndFilenameCount, DWORD* outFilenameCount, wchar_t** outFilenamePos)
{
   BOOL ret;
   DWORD filenameAndPathCount;
   wchar_t* filenamePos;

   ret = FALSE;

   if (filenameAndPathCount = GetFullPathNameW(filename, bufferCharsCount, outPathAndFilenameBuffer, &filenamePos)) // if getting full path + filename succeeded
   {
      if (File_IsExist(outPathAndFilenameBuffer))
      {
         // set output buffers if they're not null
         if (outPathAndFilenameCount) // if full path + filename count buffer available
         {
            *outPathAndFilenameCount = filenameAndPathCount;
         }

         if (outFilenameCount) // if filename count buffer available
         {
            *outFilenameCount = (DWORD)(filenameAndPathCount - (filenamePos - outPathAndFilenameBuffer));
         }

         if (outFilenamePos) // if filename position buffer available
         {
            *outFilenamePos = filenamePos;
         }

         ret = TRUE;
      }
   }

   return ret;
}

/*
 * Creates/spawns a process from a given fully qualified file path + filename
 *
 * fileFullPathAndFilename:          [in] Fully qualified path + filename
 * (optional) argv:                  [in] Pointer to null-terminated aguments block
 * (optional) argc:                  [in] Count of the above argv block, must be 0 if argv is NULL
 * (optional) outHprocess:           [out] Output buffer holding handle of the created process
 * (optional) outHProcessMainThread: [out] Output buffer holding handle of the main thread of the created process
 * (optional) pid:                   [out] Output buffer holding ID of the created process
 */
BOOL Process_CreateFromFile(const wchar_t* const fileFullPathAndFilename, wchar_t** argv, unsigned int argc, wchar_t* currentWorkingDir, HANDLE* outHprocess, HANDLE* outHProcessMainThread, DWORD* pid)
{
   BOOL ret;
   
   // structures used by CreateProcess(...)
   STARTUPINFO         startupInfo;
   PROCESS_INFORMATION processInfo;

   unsigned char isWorkingDirBufferAllocated; // whether the buffer was provided or we allcoated it
   unsigned char isWorkingDirError;           // did any error occur while looking for current working dir
   wchar_t* fileArgs;                         // contains the additional arguments that are passed to the process, each wrapped in double quotes and separated by a space

   isWorkingDirBufferAllocated = 0;
   isWorkingDirError = 1;
   ret = FALSE;

   // initialize process handle buffer with 0
   if (outHprocess)
   {
      *outHprocess = 0;
   }

   // initialize process main thread handle buffer with 0
   if (outHProcessMainThread)
   {
      *outHProcessMainThread = 0;
   }

   // initialize process ID buffer with 0
   if (pid)
   {
      *pid = 0;
   }

   if (!*currentWorkingDir) // if working dir is not provided
   {
       // the new process will have its full path as a current working directory, otherwise many games will crash
       if ( currentWorkingDir = (wchar_t*)calloc(MAX_FILE_PATH_FILENAME_COUNT, sizeof(wchar_t)) ) // if allocating a buffer for working dir succeeded
       {
           isWorkingDirBufferAllocated = 1;

           // copy full path to process
           if ( !wcsncpy_s(currentWorkingDir, MAX_FILE_PATH_FILENAME_COUNT, fileFullPathAndFilename, wcsrchr(fileFullPathAndFilename, '\\') - fileFullPathAndFilename + 1) )
           {
               // declare that no error occurred while getting the current working directory
               isWorkingDirError = 0;
           }
       }
   }
   else // if working dir is provided
   {
       // declare that no error occurred while getting the current working directory
       isWorkingDirError = 0;
   }

   if (!isWorkingDirError) // if no error occurred when getting the current working directory for the new process
   {
      if ( fileArgs = (wchar_t*)calloc(MAX_PROCESS_ARGS_CHARS_COUNT, sizeof(wchar_t)) )
      {
         // reset structures
         SecureZeroMemory(&startupInfo, sizeof(startupInfo));
         SecureZeroMemory(&processInfo, sizeof(processInfo));

         // this member must = the size of the struct itself
         startupInfo.cb = sizeof(startupInfo);

         // copy full process path + filename as a 1st arg to be passed, surrounded by double quotes
         wcscpy_s(fileArgs, MAX_PROCESS_ARGS_CHARS_COUNT, L"\"");
         wcscat_s(fileArgs, MAX_PROCESS_ARGS_CHARS_COUNT, fileFullPathAndFilename);
         wcscat_s(fileArgs, MAX_PROCESS_ARGS_CHARS_COUNT, L"\"");
         
         // copy all other input args and pass them as one string to the process, each arg is surrounded by double quotes and preceeded by a space
         for (; argc; argv++, argc--) // foreach arg
         {
             wcscat_s(fileArgs, MAX_PROCESS_ARGS_CHARS_COUNT, L" \"");
             wcscat_s(fileArgs, MAX_PROCESS_ARGS_CHARS_COUNT, *argv);
             wcscat_s(fileArgs, MAX_PROCESS_ARGS_CHARS_COUNT, L"\"");
         }
         
         if ( CreateProcessW(fileFullPathAndFilename, // if creating process process succeeded
                             fileArgs,                // pass the rest of the cmd line parameters to the process
                             NULL,                    // no process attributes 
                             NULL,                    // no thread attributes
                             FALSE,                   // the new process won't inherit our handles
         
                                                         // accroding to docs, if 'lpEnvironment' parameter is NULL and the environment block of the parent process contains Unicode characters,
                                                         // you must also ensure that dwCreationFlags includes CREATE_UNICODE_ENVIRONMENT
                             CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
                             NULL,                    // the new process uses the environment of the calling process
                             currentWorkingDir,       // the current working directory of the new process (whether provided or allocated)
                             &startupInfo,
                             &processInfo) )
         {
             // assign retreived handle to output handle buffers
             if (outHprocess)
             {
                 *outHprocess = processInfo.hProcess;
             }
         
             if (outHProcessMainThread)
             {
                 *outHProcessMainThread = processInfo.hThread;
             }
         
             if (pid)
             {
                 *pid = processInfo.dwProcessId;
             }
         
             ret = TRUE;
         }

         // free the allocated buffer for args
         free(fileArgs);
      }
   }

   if (isWorkingDirBufferAllocated) // if the current working dir buffer was allcoated NOT provided
   {
       // free this allcoated buffer for current working dir
       free(currentWorkingDir);
   }

   return ret;
}

/*
 * Opens a process by its ID
 *
 * pid:                            [in] Process ID
 * (optional) outHprocess:         [out] Output buffer holding handle of the opened process
 * (optional) outProcessname:      [out] Output buffer holding the filename of the opened process
 * (optional) bufferCharsCount:    [in] Previous buffer count in characters
 * (optional) outProcessnameCount: [out] Output buffer holding count of filename in characters
 */
BOOL Process_OpenFromPID(const DWORD pid, HANDLE* outHprocess, wchar_t* outProcessname, const DWORD bufferCharsCount, DWORD* outProcessnameCount)
{
   BOOL ret;
   HANDLE hProcess;
   wchar_t* processPathAndFilename;
   DWORD processPathAndFilenamCount;
   wchar_t* processFilenamePos;

   ret = FALSE;

   hProcess = OpenProcess( SYNCHRONIZE               | // enables a thread to wait until the object is in the signaled state, required by WaitForSingleObject(...)
                           PROCESS_CREATE_THREAD     | // required to create a thread, required by CreateRemoteThread(...)
                           PROCESS_QUERY_INFORMATION | // required to retrieve certain information about a process, such as its exit code
                           PROCESS_TERMINATE         | // required to terminate a process using TerminateProcess(...)
                           PROCESS_VM_OPERATION      | // required to perform an operation on the address space of a process, required WriteProcessMemory(...)
                           PROCESS_VM_READ           | // required by CreateRemoteThread(...)
                           PROCESS_VM_WRITE,           // required by CreateRemoteThread(...)

                           FALSE,                      // processes created by this process won't inherit the handle

                           pid );                      // process ID

   // assign retreived handle to output handle buffer
   if (outHprocess)
   {
      *outHprocess = hProcess;
   }

   if (hProcess) // if OpenProcess(...) succeeded
   {
      if (outProcessname && bufferCharsCount) // if process name is required
      {
         if (processPathAndFilename = (wchar_t*)calloc(MAX_FILE_PATH_FILENAME_COUNT, sizeof(wchar_t))) // if allocating a new buffer succeeded
         {
               if (processPathAndFilenamCount = GetProcessImageFileNameW(hProcess, processPathAndFilename, MAX_FILE_PATH_FILENAME_COUNT)) // if getting process full path + filename succeeded
               {
                  // get position of process name (one char after last match of '\')
                  processFilenamePos = wcsrchr(processPathAndFilename, '\\') + 1;

                  if (!wcscpy_s(outProcessname, bufferCharsCount, processFilenamePos)) // if copying process name succeeded
                  {
                     // set output buffer if it's not empty
                     if (outProcessnameCount) // if process name count is required
                     {
                        *outProcessnameCount = (DWORD)(processPathAndFilenamCount - (processFilenamePos - processPathAndFilename));
                     }

                     // since all went ok, return true
                     ret = TRUE;
                  }
               }

            // free allocated buffer
            free(processPathAndFilename);
         }
      }
      else // if process name is not required
      {
         ret = TRUE;
      }
   }

   return ret;
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
   enum
   {
      Injector_Arg1Type_IsInvalid,
      Injector_Arg1Type_IsFilename,
      Injector_Arg1Type_IsPID,
   } arg1Type;

   arg1Type = Injector_Arg1Type_IsInvalid;
   unsigned int argc;
   wchar_t** argv;

   DWORD dllPathAndFilenameCount;
   wchar_t* dllFilenamePos;

   HANDLE hProcess;
   HANDLE hProcessMainThread;

   DWORD processID;

   void* allocatedSpace;

   SIZE_T bytesWritten;

   HANDLE hDllRemoteThread;

   int retErrCode;
   
   // initialize everything
   hProcess                       = 0;
   hProcessMainThread             = 0;
   processID                      = 0;
   allocatedSpace                 = 0;
   bytesWritten                   = 0;
   hDllRemoteThread               = 0;
   retErrCode                     = ERROR_ALL_SUCCEEDED;

   // get argv + argc
   argv = CommandLineToArgvW(pCmdLine, (int*)&argc);

   if (argc >= ARGS_MIN_COUNT) // >= args are required
   {
      if ( ((argv[3][0] == '0') && (argv[3][1] == '\0')) || // if (4th argument = "0")
           ((argv[3][0] == '1') && (argv[3][1] == '\0')) )  // or (4th argument = "1"))
      {
         if (File_GetFullPathAndFilename(argv[1], dllPathAndFilename, _countof(dllPathAndFilename), &dllPathAndFilenameCount, NULL, &dllFilenamePos)) // if getting dll path + filename succeeded
         {
            // try to parse the 1st arg as either a complete path + filename or a process ID
            // assume 1st arg is a filename
            if (File_GetFullPathAndFilename(argv[0], processPathAndFilename, _countof(processPathAndFilename), NULL, NULL, NULL)) // if getting full process path + filename succeeded
            {
               arg1Type = Injector_Arg1Type_IsFilename;
            }
            else // if arg1 was not a valid file
            {
               // try to convert 1st arg to numeric base 10 (decimal)
               try
               {
                  processID = (DWORD)std::stoull(argv[0], NULL, 10);
                  if (processID)
                  {
                     arg1Type = Injector_Arg1Type_IsPID;
                  }
               }
               catch (...) // base 10 conversion failed
               {
                  // try to convert 1st arg to numeric base 16 (hex)
                  try
                  {
                     processID = (DWORD)std::stoull(argv[0], NULL, 16);
                     if (processID)
                     {
                        arg1Type = Injector_Arg1Type_IsPID;
                     }
                  }
                  catch (...) // base 16 conversion failed
                  {

                  }
               }
            }

            if (arg1Type != Injector_Arg1Type_IsInvalid) // if parsing 1st arg was successfull
            {
               if (arg1Type == Injector_Arg1Type_IsFilename) // if arg1 was a valid filename
               {
                  if ( !Process_CreateFromFile(processPathAndFilename, &argv[ARGS_MIN_COUNT], argc - ARGS_MIN_COUNT, argv[2], &hProcess, &hProcessMainThread, &processID) )
                  {
                     retErrCode = ERROR_CREATEPROC_FAILED;
                  }
               }
               else // if arg1 was a process ID
               {
                  if ( !Process_OpenFromPID(processID, &hProcess, NULL, NULL, NULL) )
                  {
                     retErrCode = ERROR_OPENPROC_FAILED;
                  }
               }

               if (retErrCode == ERROR_ALL_SUCCEEDED) // if nothing went wrong above in the above step
               {
                  // allocate a remote page in the child process and write the full dll path + filename i it,
                  // then later pass its address to LoadLibrary -> LoadLibrary(full_dll_path_and_filename)
                  // +1 for extra null terminator
                  allocatedSpace = VirtualAllocEx(hProcess, NULL, (dllPathAndFilenameCount + 1) * sizeof(wchar_t), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE /*0x04*/);
                  if (allocatedSpace) // if remote page allocation succeeded
                  {
                     // TODO: try to allocate a page after child's pages
                     // VirtualAlloc allocates memory at 64K boundaries even though page granularity is 4K
                     // source: https://devblogs.microsoft.com/oldnewthing/20031008-00/?p=42223

                     // if (writing the dll full path + name to the newly allocated page succeeded)
                     // and (bytes written = bytes required)
                     // +1 for null terminator
                     if ( WriteProcessMemory(hProcess, allocatedSpace, dllPathAndFilename, (dllPathAndFilenameCount + 1) * sizeof(wchar_t), &bytesWritten) &&
                           (bytesWritten == ((dllPathAndFilenameCount + 1) * sizeof(wchar_t))) )
                     {
                        // spawn LoadLibraryW(...) as a suspended remote thread in the child process with input arg = allocated page (which has full dll path + filename)
                        hDllRemoteThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryW, allocatedSpace, CREATE_SUSPENDED /*0x00000004*/, NULL);
                        if (hDllRemoteThread) // if remote thread LoadLibraryW(...) was spawned successfully
                        {
                           // resume remote thread LoadLibraryW(...)
                           if (ResumeThread(hDllRemoteThread) != -1) // if resuming remote thread LoadLibraryW(...) succeeded
                           {
                              // source: https://docs.microsoft.com/en-us/windows/win32/sync/synchronization-objects
                              // wait for injected dll to finish its DllMain
                              WaitForSingleObject(hDllRemoteThread, INFINITE);

                              // cleanup allcoated page
                              VirtualFreeEx(hProcess, allocatedSpace, 0, MEM_RELEASE /*0x8000*/); // realease memory allocated for dll path + name
                              allocatedSpace = 0;

                              if (arg1Type == Injector_Arg1Type_IsFilename) // if the process was created by this injector
                              {
                                 // resume child process main thread
                                 ResumeThread(hProcessMainThread);
                              }

                              if (argv[3][0] == '1') // if 3rd argument = "1" (wait until child process exits)
                              {
                                 // wait for process to exit and return its error code
                                 WaitForSingleObject(hProcess, INFINITE);

                                 // return the exit code of the process
                                 GetExitCodeProcess(hProcess, (LPDWORD)&retErrCode);
                              }
                           }
                           else // if resuming remote thread failed
                           {
                              retErrCode = ERROR_RESUMETHREAD_FAILED;
                           }
                        }
                        else // if remote thread was not spawned (LoadLibraryW)
                        {
                           retErrCode = ERROR_CREATEREMOTETH_FAILED;
                        }
                     }
                     else // if either (WriteProcessMemory failed) or (bytes written != bytes required) (dll full path + filename)
                     {
                        retErrCode = ERROR_WRITEPROC_FAILED;
                     }
                  }
                  else // if remote page allocation failed
                  {
                     retErrCode = ERROR_VIRTUALALLOC_FAILED;
                  }
               }
               // else the error code is already assigned by whatever the previous stage (OpenProcess or CreateProcess)
               // hence no need for an else statement
            }
            else // if parsing 1st arg failed
            {
               retErrCode = ERROR_INVALID_1ST_ARG;
            }
         }
         else // if getting dll path + filename failed
         {
            retErrCode = ERROR_DLL_FULL_PATH_FAILED;
         }
      }
      else // if 3rg arg (!= "0") and (!= "1")
      {
         retErrCode = ERROR_INVALID_3RD_ARG;
      }
   }
   else // args count < 3
   {
      retErrCode = ERROR_INVALID_ARGS_COUNT;
   }

   // free memry allocated for argv
   LocalFree(argv);

   // cleanup
   if (hDllRemoteThread) // if remote thread of injected .dll was spawned
   {
      CloseHandle(hDllRemoteThread);
   }

   if (hProcessMainThread) // if process was created by this injector and a handle to its main thread was retrieved
   {
      CloseHandle(hProcessMainThread);
   }

   if (hProcess) // if process was opened successfully
   {
      if (allocatedSpace) // if page allocation in the process was successful
      {
         // realease memory allocated for dll path + name
         VirtualFreeEx(hProcess, allocatedSpace, 0, MEM_RELEASE /*0x00008000*/);
      }

      // close process handle
      CloseHandle(hProcess);
   }

   return retErrCode;
}

