import ctypes
from ctypes import wintypes
import sys

DLL_PATH = r"D:\projects\neamaz\dll.dll"
PID = 10824

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

OpenProcess = kernel32.OpenProcess
OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
OpenProcess.restype = wintypes.HANDLE

VirtualAllocEx = kernel32.VirtualAllocEx
VirtualAllocEx.argtypes = [wintypes.HANDLE, wintypes.LPVOID, ctypes.c_size_t, wintypes.DWORD, wintypes.DWORD]
VirtualAllocEx.restype = wintypes.LPVOID

WriteProcessMemory = kernel32.WriteProcessMemory
WriteProcessMemory.argtypes = [wintypes.HANDLE, wintypes.LPVOID, wintypes.LPCVOID, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
WriteProcessMemory.restype = wintypes.BOOL

GetModuleHandleW = kernel32.GetModuleHandleW
GetModuleHandleW.argtypes = [wintypes.LPCWSTR]
GetModuleHandleW.restype = wintypes.HMODULE

GetProcAddress = kernel32.GetProcAddress
GetProcAddress.argtypes = [wintypes.HMODULE, wintypes.LPCSTR]
GetProcAddress.restype = wintypes.LPVOID

CreateRemoteThread = kernel32.CreateRemoteThread
CreateRemoteThread.argtypes = [wintypes.HANDLE, wintypes.LPVOID, ctypes.c_size_t, wintypes.LPVOID, wintypes.LPVOID, wintypes.DWORD, wintypes.LPDWORD]
CreateRemoteThread.restype = wintypes.HANDLE

WaitForSingleObject = kernel32.WaitForSingleObject
WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
WaitForSingleObject.restype = wintypes.DWORD

GetExitCodeThread = kernel32.GetExitCodeThread
GetExitCodeThread.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
GetExitCodeThread.restype = wintypes.BOOL

VirtualFreeEx = kernel32.VirtualFreeEx
VirtualFreeEx.argtypes = [wintypes.HANDLE, wintypes.LPVOID, ctypes.c_size_t, wintypes.DWORD]
VirtualFreeEx.restype = wintypes.BOOL

CloseHandle = kernel32.CloseHandle
CloseHandle.argtypes = [wintypes.HANDLE]
CloseHandle.restype = wintypes.BOOL

PROCESS_ALL_ACCESS = 0x1F0FFF
MEM_COMMIT = 0x1000
MEM_RESERVE = 0x2000
PAGE_READWRITE = 0x04
MEM_RELEASE = 0x8000
INFINITE = 0xFFFFFFFF

path_bytes = (DLL_PATH + "\x00").encode("utf-16-le")
size = len(path_bytes)

hProcess = OpenProcess(PROCESS_ALL_ACCESS, False, PID)
if not hProcess:
    raise ctypes.WinError(ctypes.get_last_error())
print("Opened process", PID)

remote_mem = VirtualAllocEx(hProcess, None, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
if not remote_mem:
    raise ctypes.WinError(ctypes.get_last_error())
print("Allocated remote memory", hex(remote_mem))

written = ctypes.c_size_t(0)
ok = WriteProcessMemory(hProcess, remote_mem, path_bytes, size, ctypes.byref(written))
if not ok:
    raise ctypes.WinError(ctypes.get_last_error())
print("Written", written.value, "bytes")

k32 = GetModuleHandleW("kernel32.dll")
loadlib = GetProcAddress(k32, b"LoadLibraryW")
if not loadlib:
    raise ctypes.WinError(ctypes.get_last_error())
print("LoadLibraryW", hex(loadlib))

tid = wintypes.DWORD(0)
hThread = CreateRemoteThread(hProcess, None, 0, loadlib, remote_mem, 0, ctypes.byref(tid))
if not hThread:
    raise ctypes.WinError(ctypes.get_last_error())
print("Remote thread created, tid", tid.value)

WaitForSingleObject(hThread, INFINITE)
exit_code = wintypes.DWORD(0)
GetExitCodeThread(hThread, ctypes.byref(exit_code))
print("LoadLibrary returned", hex(exit_code.value))

CloseHandle(hThread)
VirtualFreeEx(hProcess, remote_mem, 0, MEM_RELEASE)
CloseHandle(hProcess)
print("Done")
