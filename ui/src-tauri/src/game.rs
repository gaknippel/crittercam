// Finding the game process and injecting the camera dll with LoadLibraryW in a remote thread.
use std::ffi::OsStr;
use std::os::windows::ffi::OsStrExt;
use std::path::{Path, PathBuf};
use std::ptr::{null, null_mut};
use windows_sys::Win32::Foundation::{CloseHandle, GetLastError, INVALID_HANDLE_VALUE, WAIT_OBJECT_0};
use windows_sys::Win32::System::Diagnostics::Debug::WriteProcessMemory;
use windows_sys::Win32::System::Diagnostics::ToolHelp::{
    CreateToolhelp32Snapshot, MODULEENTRY32W, Module32FirstW, Module32NextW, PROCESSENTRY32W, Process32FirstW, Process32NextW,
    TH32CS_SNAPMODULE, TH32CS_SNAPMODULE32, TH32CS_SNAPPROCESS,
};
use windows_sys::Win32::System::LibraryLoader::{GetModuleHandleW, GetProcAddress};
use windows_sys::Win32::System::Memory::{MEM_COMMIT, MEM_RELEASE, MEM_RESERVE, PAGE_READWRITE, VirtualAllocEx, VirtualFreeEx};
use windows_sys::Win32::System::Threading::{
    CreateRemoteThread, GetExitCodeThread, OpenProcess, PROCESS_CREATE_THREAD, PROCESS_QUERY_INFORMATION, PROCESS_VM_OPERATION,
    PROCESS_VM_READ, PROCESS_VM_WRITE, WaitForSingleObject,
};

pub const GAME_EXE: &str = "nightreign.exe";
pub const DLL_NAME: &str = "crittercam.dll";

fn wide(s: &OsStr) -> Vec<u16> {
    s.encode_wide().chain(std::iter::once(0)).collect()
}

fn name_of(chars: &[u16]) -> String {
    let end = chars.iter().position(|&c| c == 0).unwrap_or(chars.len());
    String::from_utf16_lossy(&chars[..end])
}

pub fn find_game() -> Option<u32> {
    unsafe {
        let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if snapshot == INVALID_HANDLE_VALUE {
            return None;
        }
        let mut entry: PROCESSENTRY32W = std::mem::zeroed();
        entry.dwSize = size_of::<PROCESSENTRY32W>() as u32;
        let mut found = None;
        let mut more = Process32FirstW(snapshot, &mut entry) != 0;
        while more {
            if name_of(&entry.szExeFile).eq_ignore_ascii_case(GAME_EXE) {
                found = Some(entry.th32ProcessID);
                break;
            }
            more = Process32NextW(snapshot, &mut entry) != 0;
        }
        CloseHandle(snapshot);
        found
    }
}

pub fn has_dll_loaded(pid: u32) -> bool {
    unsafe {
        let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if snapshot == INVALID_HANDLE_VALUE {
            return false;
        }
        let mut entry: MODULEENTRY32W = std::mem::zeroed();
        entry.dwSize = size_of::<MODULEENTRY32W>() as u32;
        let mut found = false;
        let mut more = Module32FirstW(snapshot, &mut entry) != 0;
        while more {
            if name_of(&entry.szModule).eq_ignore_ascii_case(DLL_NAME) {
                found = true;
                break;
            }
            more = Module32NextW(snapshot, &mut entry) != 0;
        }
        CloseHandle(snapshot);
        found
    }
}

// shortcut: looks next to the exe, then in the repo's Visual Studio output (bin\x64\Release) above it. Bundle the dll as a resource if this ever ships.
pub fn find_dll() -> Option<PathBuf> {
    let exe = std::env::current_exe().ok()?;
    let mut dir = exe.parent();
    if let Some(d) = dir {
        let beside = d.join(DLL_NAME);
        if beside.is_file() {
            return Some(beside);
        }
    }
    while let Some(d) = dir {
        let built = d.join("bin").join("x64").join("Release").join(DLL_NAME);
        if built.is_file() {
            return Some(built);
        }
        dir = d.parent();
    }
    None
}

pub fn inject(pid: u32, dll: &Path) -> Result<(), String> {
    let path = wide(dll.as_os_str());
    let size = path.len() * 2;
    unsafe {
        let process = OpenProcess(
            PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
            0,
            pid,
        );
        if process.is_null() {
            return Err(format!("Couldn't open the game process (error {}).", GetLastError()));
        }
        let result = (|| {
            let remote = VirtualAllocEx(process, null(), size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if remote.is_null() {
                return Err(format!("Couldn't allocate memory in the game (error {}).", GetLastError()));
            }
            let outcome = (|| {
                if WriteProcessMemory(process, remote, path.as_ptr().cast(), size, null_mut()) == 0 {
                    return Err(format!("Couldn't write the dll path into the game (error {}).", GetLastError()));
                }
                // kernel32 is mapped at the same address in every process, so our LoadLibraryW address is valid in the game too.
                let kernel32 = GetModuleHandleW(wide(OsStr::new("kernel32.dll")).as_ptr());
                let load_library = GetProcAddress(kernel32, c"LoadLibraryW".as_ptr().cast()).ok_or("LoadLibraryW not found.")?;
                let thread = CreateRemoteThread(process, null(), 0, Some(std::mem::transmute(load_library)), remote, 0, null_mut());
                if thread.is_null() {
                    return Err(format!("Couldn't start the loader thread in the game (error {}).", GetLastError()));
                }
                let finished = WaitForSingleObject(thread, 10_000) == WAIT_OBJECT_0;
                let mut module: u32 = 0;
                GetExitCodeThread(thread, &mut module);
                CloseHandle(thread);
                match (finished, module) {
                    (false, _) => Err("The game didn't finish loading the dll within 10 seconds.".into()),
                    (true, 0) => Err("The game refused to load the dll (LoadLibrary failed).".into()),
                    _ => Ok(()),
                }
            })();
            VirtualFreeEx(process, remote, 0, MEM_RELEASE);
            outcome
        })();
        CloseHandle(process);
        result
    }
}
