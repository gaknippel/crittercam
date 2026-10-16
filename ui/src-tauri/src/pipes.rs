// The two named pipes the camera dll talks over (same protocol as IGCS):
//   \\.\pipe\IgcsDllToClient  we create it, the dll connects once while it starts and writes [type, ascii text...] messages.
//   \\.\pipe\IgcsClientToDll  the dll creates it, we connect and write [type, id, payload...] messages (settings, key bindings).
use serde::Serialize;
use std::fs::{File, OpenOptions};
use std::io::{Read, Write};
use std::os::windows::io::FromRawHandle;
use std::sync::Mutex;
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;
use tauri::{AppHandle, Emitter, Manager};
use windows_sys::Win32::Foundation::{ERROR_PIPE_CONNECTED, GetLastError, INVALID_HANDLE_VALUE};
use windows_sys::Win32::Storage::FileSystem::PIPE_ACCESS_INBOUND;
use windows_sys::Win32::System::Pipes::{ConnectNamedPipe, CreateNamedPipeW, PIPE_READMODE_MESSAGE, PIPE_TYPE_MESSAGE, PIPE_WAIT};

const DLL_TO_CLIENT: &str = r"\\.\pipe\IgcsDllToClient";
const CLIENT_TO_DLL: &str = r"\\.\pipe\IgcsClientToDll";
const BUFFER_SIZE: u32 = 4 * 1024;

#[derive(Default)]
pub struct Link {
    writer: Mutex<Option<File>>,
    pub connected: AtomicBool,
    pub pipe_blocked: AtomicBool, // another client owns the pipe name
}

#[derive(Clone, Serialize)]
struct LogLine {
    kind: u8, // MessageType: 3 notification, 4 normal, 5 error, 6 debug
    text: String,
}

fn log(app: &AppHandle, kind: u8, text: impl Into<String>) {
    let _ = app.emit("log", LogLine { kind, text: text.into() });
}

pub fn send(link: &Link, message: &[u8]) -> Result<(), String> {
    let mut writer = link.writer.lock().unwrap();
    let pipe = writer.as_mut().ok_or("Not connected to the camera.")?;
    // one write = one pipe message, the dll reads them one at a time.
    pipe.write(message).map(|_| ()).map_err(|e| e.to_string())
}

// Runs forever: waits for the dll to connect, forwards what it says, and starts over when the game closes.
pub fn serve(app: AppHandle) {
    let name: Vec<u16> = DLL_TO_CLIENT.encode_utf16().chain(std::iter::once(0)).collect();
    let mut reported = false;
    loop {
        let handle = unsafe {
            CreateNamedPipeW(name.as_ptr(), PIPE_ACCESS_INBOUND, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1, 0, BUFFER_SIZE, 0, std::ptr::null())
        };
        if handle == INVALID_HANDLE_VALUE {
            if !reported {
                log(&app, 5, "Can't open the connection for the camera: the old IGCSClient or a second crittercam window is open. Close it, this retries by itself.");
                reported = true;
                app.state::<Link>().pipe_blocked.store(true, Ordering::SeqCst);
            }
            std::thread::sleep(Duration::from_secs(1));
            continue;
        }
        if reported {
            log(&app, 4, "Connection for the camera is ready.");
            reported = false;
            app.state::<Link>().pipe_blocked.store(false, Ordering::SeqCst);
        }
        let connected = unsafe { ConnectNamedPipe(handle, std::ptr::null_mut()) != 0 || GetLastError() == ERROR_PIPE_CONNECTED };
        let mut pipe = unsafe { File::from_raw_handle(handle) };
        if !connected {
            continue;
        }
        // the dll creates its own pipe right after connecting to ours; connect on another thread so we keep reading its startup log meanwhile.
        let handle_app = app.clone();
        std::thread::spawn(move || connect_to_dll(handle_app));

        let mut buffer = vec![0u8; BUFFER_SIZE as usize];
        while let Ok(read) = pipe.read(&mut buffer) {
            if read == 0 {
                break;
            }
            if read >= 2 {
                log(&app, buffer[0], String::from_utf8_lossy(&buffer[1..read]));
            }
        }
        let link = app.state::<Link>();
        *link.writer.lock().unwrap() = None;
        link.connected.store(false, Ordering::SeqCst);
        let _ = app.emit("link", false);
        log(&app, 4, "Camera disconnected (game closed?).");
    }
}

fn connect_to_dll(app: AppHandle) {
    for _ in 0..100 {
        if let Ok(file) = OpenOptions::new().write(true).open(CLIENT_TO_DLL) {
            let link = app.state::<Link>();
            *link.writer.lock().unwrap() = Some(file);
            link.connected.store(true, Ordering::SeqCst);
            let _ = app.emit("link", true); // the UI sends all settings and key bindings on this
            return;
        }
        std::thread::sleep(Duration::from_millis(100));
    }
    log(&app, 5, "The camera connected, but its settings pipe never showed up.");
}
