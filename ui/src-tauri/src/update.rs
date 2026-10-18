// Updates from releases in the private GitHub repo. ship.ps1 publishes them: an NSIS installer plus latest.json (Tauri's
// updater manifest) whose download url is the installer's API asset url. Private release assets need a token, and their
// urls change every release, so we look up this release's latest.json through the API first, then let the plugin take over.
use serde::Serialize;
use std::sync::Mutex;
use tauri::AppHandle;
use tauri_plugin_updater::{Update, UpdaterExt};

const REPO: &str = "gaknippel/crittercam";
// read-only token for this one repo, baked in by ship.ps1. Dev builds have none and never check.
const TOKEN: Option<&str> = option_env!("CRITTERCAM_UPDATE_TOKEN");

#[derive(Default)]
pub struct Pending(Mutex<Option<Update>>);

#[derive(Serialize)]
pub struct UpdateInfo {
    version: String,
    notes: Option<String>,
}

fn err(e: impl std::fmt::Display) -> String {
    e.to_string()
}

pub async fn check(app: &AppHandle, pending: &Pending) -> Result<Option<UpdateInfo>, String> {
    let Some(token) = TOKEN.filter(|t| !t.is_empty()) else {
        return Ok(None);
    };
    // reqwest is built without a bundled crypto provider (same as the updater plugin, which installs ring the same way)
    let _ = rustls::crypto::ring::default_provider().install_default();
    let auth = format!("Bearer {token}");
    let release: serde_json::Value = reqwest::Client::new()
        .get(format!("https://api.github.com/repos/{REPO}/releases/latest"))
        .header("Authorization", &auth)
        .header("Accept", "application/vnd.github+json")
        .header("User-Agent", "crittercam")
        .send()
        .await
        .map_err(err)?
        .error_for_status()
        .map_err(err)?
        .json()
        .await
        .map_err(err)?;
    let manifest = release["assets"]
        .as_array()
        .and_then(|assets| assets.iter().find(|a| a["name"] == "latest.json"))
        .and_then(|a| a["url"].as_str())
        .ok_or("The latest release has no latest.json.")?;
    // asset api urls answer with the file itself when asked for octet-stream; the same headers are used for the installer download
    let update = app
        .updater_builder()
        .endpoints(vec![manifest.parse().map_err(err)?])
        .map_err(err)?
        .header("Authorization", auth)
        .map_err(err)?
        .header("Accept", "application/octet-stream")
        .map_err(err)?
        .header("User-Agent", "crittercam")
        .map_err(err)?
        .build()
        .map_err(err)?
        .check()
        .await
        .map_err(err)?;
    let info = update.as_ref().map(|u| UpdateInfo { version: u.version.clone(), notes: u.body.clone() });
    *pending.0.lock().unwrap() = update;
    Ok(info)
}

pub async fn install(app: &AppHandle, pending: &Pending) -> Result<(), String> {
    let update = pending.0.lock().unwrap().take().ok_or("No update to install.")?;
    update.download_and_install(|_, _| {}, || {}).await.map_err(err)?;
    app.restart();
}
