# noisy-dll-injector
A classical dll injector using createremotethread.  

## Usage:  
```batch
SilentDLLinjector_x<32|64>.exe <process_id | file_path> <dll_file> <current_working_dir> <is_wait> [additional_arg1] [additional_arg2] ...
```
*   process_id | file_path: `<mandatory>`  
      This could be either  
      The name [and full path] of the process to create, this can also be relative to the current working directory.  
      Or  
      The process ID (PID) to open/use if the process is already running  

*   dll_file: `<mandatory>`  
      The name [and full path] of the .dll to inject, this can be relative to the current working directory

*   current_working_dir: `<mandatory>`  
      The directory/folder to use as a working directory for the new process,  
      this may be an empty pair of quotes (empty string) if the working dir is the .exe's path

*   is_wait: `<mandatory>`  
      If set to 1, the injector will wait for the .exe to return/exit, then it'll give back control and return the exit code of the process.  
      If set to 0, the injector will give back control immediately after injection, returning 0 on success.
      Otherwise, the injector fails.

*   additional args: `[optional]`  
      These extra arguments are passed to the .exe, each argument is wrapped in double quotes


## Examples:
* Ex #1: Start the .exe then inject the .dll and immediately return control to cmd.  
  Assuming cmd was spawned in the current directory of the injector  
```batch
start "" /b /wait "SilentDLLinjector_x<32|64>.exe" "D:\myApp.exe" "myDll.dll" "" 0
echo %errorlevel%
```

* Ex #2: Start the .exe then inject the .dll and wait until the .exe returns, then return control to cmd.  
  Also pass some arguments to the .exe process  
  Assuming cmd was spawned in the current directory of the injector  
```batch
start "" /b /wait "SilentDLLinjector_x<32|64>.exe" "D:\myApp.exe" "myDll.dll" "" 1 -adv "arg2 with spaces"
echo %errorlevel%
```

* Ex #3: Open a process whose PID = 3280, then inject the .dll and immediately return control to cmd.  
  Assuming cmd was spawned in the current directory of the injector  
```batch
start "" /b /wait "SilentDLLinjector_x<32|64>.exe" 3280 "myDLL.dll" "" 0
echo %errorlevel%
```
