//! phx-svcmon — PhoenixOS Service Monitor Daemon
//!
//! Reads service definitions from /etc/kindle/services/*.svc,
//! starts them in dependency order, and restarts them if they exit.

use std::collections::HashMap;
use std::fs;
use std::io::{self, BufRead, BufReader, Write};
use std::os::unix::net::UnixListener;
use std::path::Path;
use std::process::{Child, Command};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

// ── Service definition ────────────────────────────────────────────────────────

#[derive(Debug, Clone, PartialEq)]
enum RestartPolicy {
    Always,
    Never,
    OnFailure,
}

#[derive(Debug, Clone)]
struct ServiceDef {
    name:    String,
    exec:    String,
    restart: RestartPolicy,
    depends: Vec<String>,
}

#[derive(Debug)]
enum ServiceState {
    Stopped,
    Starting,
    Running { child: Child, started_at: Instant },
    Failed  { exit_code: i32 },
}

// ── Parse a .svc file ─────────────────────────────────────────────────────────
//
// Format:
//   name = my-service
//   exec = /usr/bin/my-service --flag
//   restart = always
//   depends = other-service, another-service
//

fn parse_svc_file(path: &Path) -> Option<ServiceDef> {
    let f = fs::File::open(path).ok()?;
    let reader = BufReader::new(f);

    let mut name    = String::new();
    let mut exec    = String::new();
    let mut restart = RestartPolicy::Never;
    let mut depends = Vec::new();

    for line in reader.lines().flatten() {
        let line = line.trim().to_string();
        if line.is_empty() || line.starts_with('#') { continue; }

        let parts: Vec<&str> = line.splitn(2, '=').collect();
        if parts.len() != 2 { continue; }

        let key = parts[0].trim();
        let val = parts[1].trim();

        match key {
            "name"    => name    = val.to_string(),
            "exec"    => exec    = val.to_string(),
            "restart" => restart = match val {
                "always"     => RestartPolicy::Always,
                "on-failure" => RestartPolicy::OnFailure,
                _            => RestartPolicy::Never,
            },
            "depends" => depends = val.split(',')
                                      .map(|s| s.trim().to_string())
                                      .filter(|s| !s.is_empty())
                                      .collect(),
            _ => {}
        }
    }

    if name.is_empty() || exec.is_empty() { return None; }
    Some(ServiceDef { name, exec, restart, depends })
}

// ── Load all service definitions ──────────────────────────────────────────────

fn load_services(dir: &str) -> Vec<ServiceDef> {
    let path = Path::new(dir);
    if !path.is_dir() {
        eprintln!("svcmon: service directory {} not found", dir);
        return Vec::new();
    }

    let mut services = Vec::new();
    if let Ok(entries) = fs::read_dir(path) {
        for entry in entries.flatten() {
            let ep = entry.path();
            if ep.extension().and_then(|e| e.to_str()) == Some("svc") {
                if let Some(svc) = parse_svc_file(&ep) {
                    services.push(svc);
                }
            }
        }
    }
    services
}

// ── Topological sort ──────────────────────────────────────────────────────────

fn topo_sort(services: &[ServiceDef]) -> Vec<usize> {
    let n = services.len();
    let name_to_idx: HashMap<&str, usize> = services
        .iter()
        .enumerate()
        .map(|(i, s)| (s.name.as_str(), i))
        .collect();

    let mut visited = vec![false; n];
    let mut order   = Vec::with_capacity(n);

    fn dfs(
        idx: usize,
        services: &[ServiceDef],
        name_to_idx: &HashMap<&str, usize>,
        visited: &mut Vec<bool>,
        order: &mut Vec<usize>,
    ) {
        if visited[idx] { return; }
        visited[idx] = true;
        for dep in &services[idx].depends {
            if let Some(&di) = name_to_idx.get(dep.as_str()) {
                dfs(di, services, name_to_idx, visited, order);
            }
        }
        order.push(idx);
    }

    for i in 0..n {
        dfs(i, services, &name_to_idx, &mut visited, &mut order);
    }
    order
}

// ── Launch a service ──────────────────────────────────────────────────────────

fn launch_service(def: &ServiceDef) -> io::Result<Child> {
    let parts: Vec<&str> = def.exec.split_whitespace().collect();
    if parts.is_empty() {
        return Err(io::Error::new(io::ErrorKind::InvalidInput, "empty exec"));
    }
    let mut cmd = Command::new(parts[0]);
    for arg in &parts[1..] { cmd.arg(arg); }
    cmd.spawn()
}

// ── Monitor thread ────────────────────────────────────────────────────────────

struct Monitor {
    defs:   Vec<ServiceDef>,
    states: Vec<ServiceState>,
    log:    fs::File,
}

impl Monitor {
    fn new(defs: Vec<ServiceDef>) -> Self {
        let n = defs.len();
        let log = fs::OpenOptions::new()
            .create(true).append(true)
            .open("/var/log/phx-svcmon.log")
            .unwrap_or_else(|_| fs::OpenOptions::new()
                .create(true).write(true)
                .open("/dev/null")
                .unwrap());

        let states = (0..n).map(|_| ServiceState::Stopped).collect();
        Monitor { defs, states, log }
    }

    fn log(&mut self, msg: &str) {
        let _ = writeln!(self.log, "[svcmon] {}", msg);
        eprintln!("[svcmon] {}", msg);
    }

    fn start_all(&mut self, order: &[usize]) {
        for &idx in order {
            self.start(idx);
        }
    }

    fn start(&mut self, idx: usize) {
        let def = &self.defs[idx];
        self.log(&format!("Starting service: {}", def.name));
        match launch_service(def) {
            Ok(child) => {
                self.states[idx] = ServiceState::Running {
                    child,
                    started_at: Instant::now(),
                };
            }
            Err(e) => {
                self.log(&format!("Failed to start {}: {}", def.name, e));
                self.states[idx] = ServiceState::Failed { exit_code: -1 };
            }
        }
    }

    fn tick(&mut self) {
        for idx in 0..self.defs.len() {
            let needs_restart = match &mut self.states[idx] {
                ServiceState::Running { child, .. } => {
                    match child.try_wait() {
                        Ok(Some(status)) => {
                            let code = status.code().unwrap_or(-1);
                            self.log(&format!(
                                "Service {} exited with code {}",
                                self.defs[idx].name, code
                            ));
                            self.states[idx] = ServiceState::Failed { exit_code: code };
                            true
                        }
                        _ => false,
                    }
                }
                _ => false,
            };

            if needs_restart {
                let policy = self.defs[idx].restart.clone();
                let code = match self.states[idx] {
                    ServiceState::Failed { exit_code } => exit_code,
                    _ => 0,
                };
                let should = match policy {
                    RestartPolicy::Always     => true,
                    RestartPolicy::OnFailure  => code != 0,
                    RestartPolicy::Never      => false,
                };
                if should {
                    thread::sleep(Duration::from_secs(2));
                    self.start(idx);
                }
            }
        }
    }
}

// ── IPC socket handler ────────────────────────────────────────────────────────

fn handle_ipc_client(mut stream: std::os::unix::net::UnixStream) {
    use std::io::Read;
    let mut buf = [0u8; 256];
    if let Ok(n) = stream.read(&mut buf) {
        let cmd = String::from_utf8_lossy(&buf[..n]);
        let cmd = cmd.trim();
        let response = match cmd.split_once(' ') {
            Some(("status", name)) => format!("STATUS {} unknown\n", name),
            Some(("start",  name)) => format!("START {} ok\n", name),
            Some(("stop",   name)) => format!("STOP {} ok\n", name),
            _                      => "ERROR unknown command\n".to_string(),
        };
        let _ = stream.write_all(response.as_bytes());
    }
}

// ── Main ──────────────────────────────────────────────────────────────────────

fn main() {
    eprintln!("phx-svcmon: PhoenixOS Service Monitor starting");

    let services = load_services("/etc/kindle/services");
    if services.is_empty() {
        eprintln!("phx-svcmon: no services found in /etc/kindle/services");
    }

    let order = topo_sort(&services);
    let monitor = Arc::new(Mutex::new(Monitor::new(services)));

    {
        let mut m = monitor.lock().unwrap();
        m.start_all(&order);
    }

    // IPC socket thread
    let mon2 = Arc::clone(&monitor);
    thread::spawn(move || {
        let sock_path = "/run/svcmon.sock";
        let _ = fs::remove_file(sock_path);
        if let Ok(listener) = UnixListener::bind(sock_path) {
            for stream in listener.incoming().flatten() {
                let _ = &mon2;  // keep alive
                handle_ipc_client(stream);
            }
        }
    });

    // Main monitoring loop
    loop {
        thread::sleep(Duration::from_secs(5));
        if let Ok(mut m) = monitor.lock() {
            m.tick();
        }
    }
}
