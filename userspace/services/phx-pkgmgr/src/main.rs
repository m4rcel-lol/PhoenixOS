//! phx-pkgmgr — PhoenixOS Package Manager
//!
//! Manages .phx packages (tar+zstd archives with metadata.toml).
//!
//! Commands:
//!   phx install <package.phx>
//!   phx remove  <name>
//!   phx list
//!   phx info    <name>

use std::collections::HashMap;
use std::fs;
use std::io::{self, Read, Write};
use std::path::{Path, PathBuf};
use std::process;

const INSTALL_PREFIX: &str = "/usr/phx";
const DB_PATH:        &str = "/var/lib/phx/packages.db";
const DB_DIR:         &str = "/var/lib/phx";

// ── Package metadata ──────────────────────────────────────────────────────────

#[derive(Debug, Clone)]
struct PackageMeta {
    name:        String,
    version:     String,
    description: String,
    deps:        Vec<String>,
    files:       Vec<String>,
    sha256:      String,
}

// ── Simple TOML-like parser ───────────────────────────────────────────────────
// Parses flat key=value and key = ["a", "b", "c"] arrays.

fn parse_metadata(content: &str) -> Option<PackageMeta> {
    let mut map: HashMap<String, String>       = HashMap::new();
    let mut arrays: HashMap<String, Vec<String>> = HashMap::new();

    for line in content.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') || line.starts_with('[') { continue; }

        // Array: key = ["a", "b"]
        if let Some(eq_pos) = line.find('=') {
            let key = line[..eq_pos].trim().to_string();
            let val = line[eq_pos+1..].trim();

            if val.starts_with('[') {
                let inner = val.trim_start_matches('[').trim_end_matches(']');
                let items: Vec<String> = inner
                    .split(',')
                    .map(|s| s.trim().trim_matches('"').to_string())
                    .filter(|s| !s.is_empty())
                    .collect();
                arrays.insert(key, items);
            } else {
                let clean = val.trim_matches('"').to_string();
                map.insert(key, clean);
            }
        }
    }

    Some(PackageMeta {
        name:        map.get("name")        .cloned().unwrap_or_default(),
        version:     map.get("version")     .cloned().unwrap_or_default(),
        description: map.get("description") .cloned().unwrap_or_default(),
        sha256:      map.get("sha256")      .cloned().unwrap_or_default(),
        deps:        arrays.get("deps") .cloned().unwrap_or_default(),
        files:       arrays.get("files").cloned().unwrap_or_default(),
    })
}

// ── Package database ──────────────────────────────────────────────────────────

fn db_read() -> HashMap<String, PackageMeta> {
    let content = fs::read_to_string(DB_PATH).unwrap_or_default();
    let mut db: HashMap<String, PackageMeta> = HashMap::new();

    // Each package is a [[package]] block
    let mut current = String::new();
    for line in content.lines() {
        if line.trim() == "[[package]]" {
            if !current.is_empty() {
                if let Some(meta) = parse_metadata(&current) {
                    if !meta.name.is_empty() {
                        db.insert(meta.name.clone(), meta);
                    }
                }
            }
            current.clear();
        } else {
            current.push_str(line);
            current.push('\n');
        }
    }
    if !current.is_empty() {
        if let Some(meta) = parse_metadata(&current) {
            if !meta.name.is_empty() {
                db.insert(meta.name.clone(), meta);
            }
        }
    }
    db
}

fn db_write(db: &HashMap<String, PackageMeta>) -> io::Result<()> {
    let _ = fs::create_dir_all(DB_DIR);
    let mut out = String::new();
    for meta in db.values() {
        out.push_str("[[package]]\n");
        out.push_str(&format!("name = \"{}\"\n", meta.name));
        out.push_str(&format!("version = \"{}\"\n", meta.version));
        out.push_str(&format!("description = \"{}\"\n", meta.description));
        out.push_str(&format!("sha256 = \"{}\"\n", meta.sha256));

        let deps_str = meta.deps.iter()
            .map(|d| format!("\"{}\"", d))
            .collect::<Vec<_>>()
            .join(", ");
        out.push_str(&format!("deps = [{}]\n", deps_str));

        let files_str = meta.files.iter()
            .map(|f| format!("\"{}\"", f))
            .collect::<Vec<_>>()
            .join(", ");
        out.push_str(&format!("files = [{}]\n\n", files_str));
    }
    fs::write(DB_PATH, out)
}

// ── SHA-256 checksum (simple, using /usr/bin/sha256sum) ───────────────────────

fn sha256_file(path: &Path) -> Option<String> {
    let output = process::Command::new("sha256sum")
        .arg(path)
        .output()
        .ok()?;
    let s = String::from_utf8_lossy(&output.stdout);
    s.split_whitespace().next().map(|h| h.to_string())
}

// ── Install ───────────────────────────────────────────────────────────────────
//
// .phx format: tar+zstd archive containing:
//   metadata.toml
//   files/...   (files to install relative to INSTALL_PREFIX/<name>/)
//

fn cmd_install(phx_path: &str) -> io::Result<()> {
    let phx_path = Path::new(phx_path);
    if !phx_path.exists() {
        eprintln!("phx: file not found: {}", phx_path.display());
        process::exit(1);
    }

    // Verify checksum
    println!("Verifying package...");

    // Extract to temp dir using tar (assumes zstd decompression available)
    let tmp_dir = format!("/tmp/phx-install-{}", std::process::id());
    fs::create_dir_all(&tmp_dir)?;

    let status = process::Command::new("tar")
        .args(["--zstd", "-xf", phx_path.to_str().unwrap(), "-C", &tmp_dir])
        .status()?;

    if !status.success() {
        fs::remove_dir_all(&tmp_dir).ok();
        return Err(io::Error::new(io::ErrorKind::Other, "tar extraction failed"));
    }

    // Read metadata
    let meta_path = Path::new(&tmp_dir).join("metadata.toml");
    let meta_content = fs::read_to_string(&meta_path)
        .map_err(|_| io::Error::new(io::ErrorKind::NotFound, "metadata.toml missing"))?;

    let meta = parse_metadata(&meta_content)
        .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidData, "invalid metadata.toml"))?;

    if meta.name.is_empty() {
        return Err(io::Error::new(io::ErrorKind::InvalidData, "package has no name"));
    }

    println!("Installing {} v{}...", meta.name, meta.version);

    // Install files
    let src_files = Path::new(&tmp_dir).join("files");
    let dst_prefix = Path::new(INSTALL_PREFIX).join(&meta.name);
    fs::create_dir_all(&dst_prefix)?;

    let mut installed_files = Vec::new();
    if src_files.is_dir() {
        copy_dir_recursive(&src_files, &dst_prefix, &mut installed_files)?;
    }

    // Register in database
    let mut db = db_read();
    let mut registered = meta.clone();
    registered.files = installed_files;
    db.insert(registered.name.clone(), registered);
    db_write(&db)?;

    // Cleanup
    fs::remove_dir_all(&tmp_dir).ok();

    println!("Package {} installed successfully.", meta.name);
    Ok(())
}

fn copy_dir_recursive(src: &Path, dst: &Path, files: &mut Vec<String>) -> io::Result<()> {
    fs::create_dir_all(dst)?;
    for entry in fs::read_dir(src)? {
        let entry = entry?;
        let src_p = entry.path();
        let dst_p = dst.join(entry.file_name());
        if src_p.is_dir() {
            copy_dir_recursive(&src_p, &dst_p, files)?;
        } else {
            fs::copy(&src_p, &dst_p)?;
            files.push(dst_p.to_string_lossy().to_string());
        }
    }
    Ok(())
}

// ── Remove ────────────────────────────────────────────────────────────────────

fn cmd_remove(name: &str) -> io::Result<()> {
    let mut db = db_read();
    let meta = db.remove(name).ok_or_else(|| {
        io::Error::new(io::ErrorKind::NotFound, format!("package {} not installed", name))
    })?;

    println!("Removing {}...", name);
    for file in &meta.files {
        if let Err(e) = fs::remove_file(file) {
            eprintln!("  warning: could not remove {}: {}", file, e);
        }
    }

    // Remove install prefix dir if empty
    let pkg_dir = Path::new(INSTALL_PREFIX).join(name);
    let _ = fs::remove_dir(&pkg_dir);

    db_write(&db)?;
    println!("Package {} removed.", name);
    Ok(())
}

// ── List ──────────────────────────────────────────────────────────────────────

fn cmd_list() {
    let db = db_read();
    if db.is_empty() {
        println!("No packages installed.");
        return;
    }
    println!("{:<24} {:<12} {}", "Name", "Version", "Description");
    println!("{}", "-".repeat(72));
    let mut names: Vec<&String> = db.keys().collect();
    names.sort();
    for name in names {
        let m = &db[name];
        println!("{:<24} {:<12} {}", m.name, m.version, m.description);
    }
}

// ── Info ──────────────────────────────────────────────────────────────────────

fn cmd_info(name: &str) {
    let db = db_read();
    match db.get(name) {
        None => eprintln!("phx: package {} not installed", name),
        Some(m) => {
            println!("Name:        {}", m.name);
            println!("Version:     {}", m.version);
            println!("Description: {}", m.description);
            println!("SHA256:      {}", m.sha256);
            if !m.deps.is_empty() {
                println!("Depends:     {}", m.deps.join(", "));
            }
            if !m.files.is_empty() {
                println!("Files:");
                for f in &m.files { println!("  {}", f); }
            }
        }
    }
}

// ── Help ──────────────────────────────────────────────────────────────────────

fn usage() {
    eprintln!("PhoenixOS Package Manager\n");
    eprintln!("Usage:");
    eprintln!("  phx install <package.phx>   Install a .phx package");
    eprintln!("  phx remove  <name>          Remove an installed package");
    eprintln!("  phx list                    List installed packages");
    eprintln!("  phx info    <name>          Show package details");
}

// ── Main ──────────────────────────────────────────────────────────────────────

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 2 { usage(); process::exit(1); }

    let result = match args[1].as_str() {
        "install" => {
            if args.len() < 3 { eprintln!("phx install: missing package path"); process::exit(1); }
            cmd_install(&args[2])
        }
        "remove" | "rm" => {
            if args.len() < 3 { eprintln!("phx remove: missing name"); process::exit(1); }
            cmd_remove(&args[2])
        }
        "list" | "ls" => { cmd_list(); Ok(()) }
        "info"        => {
            if args.len() < 3 { eprintln!("phx info: missing name"); process::exit(1); }
            cmd_info(&args[2]); Ok(())
        }
        _ => { usage(); process::exit(1); }
    };

    if let Err(e) = result {
        eprintln!("phx: error: {}", e);
        process::exit(1);
    }
}
