mod game;
mod pipes;
mod update;

use pipes::Link;
use serde::Serialize;
use std::sync::atomic::Ordering;
use tauri::State;

#[derive(Serialize)]
struct Status {
    game_running: bool,
    injected: bool,
    connected: bool,
    pipe_blocked: bool,
}

#[tauri::command]
fn status(link: State<Link>) -> Status {
    let pid = game::find_game();
    Status {
        game_running: pid.is_some(),
        injected: pid.is_some_and(game::has_dll_loaded),
        connected: link.connected.load(Ordering::SeqCst),
        pipe_blocked: link.pipe_blocked.load(Ordering::SeqCst),
    }
}

#[tauri::command]
fn inject() -> Result<(), String> {
    let pid = game::find_game().ok_or("ELDEN RING NIGHTREIGN isn't running.")?;
    if game::has_dll_loaded(pid) {
        // injecting twice would hook everything twice.
        return Err("The camera is already in the game. Restart the game to inject again.".into());
    }
    let dll = game::find_dll().ok_or("crittercam.dll not found. Build it in Visual Studio (Release | x64) first.")?;
    game::inject(pid, &dll)
}

#[tauri::command]
fn send(message: Vec<u8>, link: State<Link>) -> Result<(), String> {
    pipes::send(&link, &message)
}

#[tauri::command]
async fn check_update(app: tauri::AppHandle, pending: State<'_, update::Pending>) -> Result<Option<update::UpdateInfo>, String> {
    update::check(&app, &pending).await
}

#[tauri::command]
async fn install_update(app: tauri::AppHandle, pending: State<'_, update::Pending>) -> Result<(), String> {
    update::install(&app, &pending).await
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_updater::Builder::new().build())
        .manage(Link::default())
        .manage(update::Pending::default())
        .setup(|app| {
            let handle = app.handle().clone();
            std::thread::spawn(move || pipes::serve(handle));
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![status, inject, send, check_update, install_update])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
