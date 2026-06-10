use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};
use serde::{Serialize, Deserialize};
use crate::protractor::Point2D;

#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum ActionType {
    Iconify,
    Kill,
    Lower,
    Raise,
    Maximize,
    Restore,
    ToggleMaximized,
    Keypress(String),
    Execute(String),
    WorkspaceLeft,
    WorkspaceRight,
    WorkspaceUp,
    WorkspaceDown,
    ShowOverview,
    ShowAppGrid,
    Click(Option<i32>),
    ToggleFullscreen,
    ShowDesktop,
    LockScreen,
    Terminal,
    VolumeUp,
    VolumeDown,
    VolumeMute,
    MediaPlay,
    MediaNext,
    MediaPrev,
    Www,
    Home,
    Email,
    Search,
    Calculator,
    ControlCenter,
    Logout,
    Screenshot,
    ScreenshotWindow,
    ScreenshotArea,
    Gnome(String),
    Abort,
}

impl ActionType {
    pub fn from_string(s: &str) -> Option<Self> {
        let s = s.trim();
        let mut parts = s.splitn(2, |c| c == ' ' || c == '\t');
        let cmd = parts.next()?.to_lowercase();
        let arg = parts.next().unwrap_or("").trim().to_string();

        match cmd.as_str() {
            "iconify" => Some(ActionType::Iconify),
            "kill" => Some(ActionType::Kill),
            "lower" => Some(ActionType::Lower),
            "raise" => Some(ActionType::Raise),
            "maximize" => Some(ActionType::Maximize),
            "restore" => Some(ActionType::Restore),
            "toggle-maximized" => Some(ActionType::ToggleMaximized),
            "keypress" | "keys" => {
                let normalized = arg.replace("_L", "").replace("_R", "");
                Some(ActionType::Keypress(normalized))
            }
            "exec" => Some(ActionType::Execute(arg)),
            "workspace-left" => Some(ActionType::WorkspaceLeft),
            "workspace-right" => Some(ActionType::WorkspaceRight),
            "workspace-up" => Some(ActionType::WorkspaceUp),
            "workspace-down" => Some(ActionType::WorkspaceDown),
            "show-overview" => Some(ActionType::ShowOverview),
            "show-app-grid" => Some(ActionType::ShowAppGrid),
            "click" => {
                let btn = arg.parse::<i32>().ok();
                Some(ActionType::Click(btn))
            }
            "toggle-fullscreen" => Some(ActionType::ToggleFullscreen),
            "show-desktop" => Some(ActionType::ShowDesktop),
            "lock-screen" => Some(ActionType::LockScreen),
            "terminal" => Some(ActionType::Terminal),
            "volume-up" => Some(ActionType::VolumeUp),
            "volume-down" => Some(ActionType::VolumeDown),
            "volume-mute" => Some(ActionType::VolumeMute),
            "media-play" => Some(ActionType::MediaPlay),
            "media-next" => Some(ActionType::MediaNext),
            "media-prev" => Some(ActionType::MediaPrev),
            "www" => Some(ActionType::Www),
            "home" => Some(ActionType::Home),
            "email" => Some(ActionType::Email),
            "search" => Some(ActionType::Search),
            "calculator" => Some(ActionType::Calculator),
            "control-center" => Some(ActionType::ControlCenter),
            "logout" => Some(ActionType::Logout),
            "screenshot" => Some(ActionType::Screenshot),
            "screenshot-window" => Some(ActionType::ScreenshotWindow),
            "screenshot-area" => Some(ActionType::ScreenshotArea),
            "gnome" => Some(ActionType::Gnome(arg)),
            "disabled" | "abort" => Some(ActionType::Abort),
            _ => None,
        }
    }

    pub fn to_string(&self) -> String {
        match self {
            ActionType::Iconify => "iconify".to_string(),
            ActionType::Kill => "kill".to_string(),
            ActionType::Lower => "lower".to_string(),
            ActionType::Raise => "raise".to_string(),
            ActionType::Maximize => "maximize".to_string(),
            ActionType::Restore => "restore".to_string(),
            ActionType::ToggleMaximized => "toggle-maximized".to_string(),
            ActionType::Keypress(arg) => format!("keypress {}", arg),
            ActionType::Execute(arg) => format!("exec {}", arg),
            ActionType::WorkspaceLeft => "workspace-left".to_string(),
            ActionType::WorkspaceRight => "workspace-right".to_string(),
            ActionType::WorkspaceUp => "workspace-up".to_string(),
            ActionType::WorkspaceDown => "workspace-down".to_string(),
            ActionType::ShowOverview => "show-overview".to_string(),
            ActionType::ShowAppGrid => "show-app-grid".to_string(),
            ActionType::Click(btn) => {
                if let Some(b) = btn {
                    format!("click {}", b)
                } else {
                    "click".to_string()
                }
            }
            ActionType::ToggleFullscreen => "toggle-fullscreen".to_string(),
            ActionType::ShowDesktop => "show-desktop".to_string(),
            ActionType::LockScreen => "lock-screen".to_string(),
            ActionType::Terminal => "terminal".to_string(),
            ActionType::VolumeUp => "volume-up".to_string(),
            ActionType::VolumeDown => "volume-down".to_string(),
            ActionType::VolumeMute => "volume-mute".to_string(),
            ActionType::MediaPlay => "media-play".to_string(),
            ActionType::MediaNext => "media-next".to_string(),
            ActionType::MediaPrev => "media-prev".to_string(),
            ActionType::Www => "www".to_string(),
            ActionType::Home => "home".to_string(),
            ActionType::Email => "email".to_string(),
            ActionType::Search => "search".to_string(),
            ActionType::Calculator => "calculator".to_string(),
            ActionType::ControlCenter => "control-center".to_string(),
            ActionType::Logout => "logout".to_string(),
            ActionType::Screenshot => "screenshot".to_string(),
            ActionType::ScreenshotWindow => "screenshot-window".to_string(),
            ActionType::ScreenshotArea => "screenshot-area".to_string(),
            ActionType::Gnome(arg) => format!("gnome {}", arg),
            ActionType::Abort => "abort".to_string(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(untagged)]
pub enum ActionsVecOrString {
    Single(String),
    Multiple(Vec<String>),
}

impl ActionsVecOrString {
    pub fn to_actions(&self) -> Vec<ActionType> {
        match self {
            ActionsVecOrString::Single(s) => {
                ActionType::from_string(s).into_iter().collect()
            }
            ActionsVecOrString::Multiple(v) => {
                v.iter().filter_map(|s| ActionType::from_string(s)).collect()
            }
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GestureConfig {
    pub id: Option<String>,
    #[serde(rename = "move")]
    pub movement_expr: Option<String>,
    #[serde(rename = "do")]
    pub action_expr: ActionsVecOrString,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppMatch {
    pub class: Option<String>,
    pub title: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppConfig {
    pub name: String,
    #[serde(rename = "match")]
    pub match_rules: AppMatch,
    pub gestures: HashMap<String, GestureConfig>,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct YamlConfig {
    pub global: Option<HashMap<String, GestureConfig>>,
    pub apps: Option<Vec<AppConfig>>,
}

#[derive(Debug, Clone)]
pub struct Gesture {
    pub id: String,
    pub name: String,
    pub raw_movement: String,
    pub points: Vec<Point2D>,
    pub actions: Vec<ActionType>,
    pub is_custom: bool,
    pub is_modified: bool,
    pub is_deleted: bool,
}

#[derive(Debug, Clone)]
pub struct Configuration {
    pub gestures: Vec<Gesture>,
    pub apps: Vec<AppConfig>,
    pub user_config_path: PathBuf,
}

pub fn parse_points(expr: &str) -> Vec<Point2D> {
    let mut points = Vec::new();
    for token in expr.split_whitespace() {
        let mut coords = token.split(',');
        if let (Some(x_str), Some(y_str)) = (coords.next(), coords.next()) {
            if let (Ok(x), Ok(y)) = (x_str.parse::<f64>(), y_str.parse::<f64>()) {
                points.push(Point2D { x, y });
            }
        }
    }
    points
}

fn get_environment_suffix() -> Option<&'static str> {
    if std::env::var("SWAYSOCK").is_ok() {
        return Some("sway");
    }
    if std::env::var("HYPRLAND_INSTANCE_SIGNATURE").is_ok() {
        return Some("hyprland");
    }
    if let Ok(desktop) = std::env::var("XDG_CURRENT_DESKTOP") {
        if desktop.contains("GNOME") || desktop.contains("gnome") || desktop.contains("Ubuntu") {
            return Some("gnome");
        }
        if desktop.contains("KDE") || desktop.contains("kde") {
            return Some("kde");
        }
        if desktop.contains("XFCE") || desktop.contains("xfce") {
            return Some("xfce");
        }
    }
    None
}

pub fn get_default_config_path() -> PathBuf {
    let base = if let Ok(xdg) = std::env::var("XDG_CONFIG_HOME") {
        PathBuf::from(xdg)
    } else if let Ok(home) = std::env::var("HOME") {
        PathBuf::from(home).join(".config")
    } else {
        PathBuf::from(".")
    };
    let dir = base.join("mygestures");
    if let Some(suffix) = get_environment_suffix() {
        dir.join(format!("mygestures_{}.yaml", suffix))
    } else {
        dir.join("mygestures.yaml")
    }
}

pub fn initialize_user_config_if_missing() -> std::io::Result<PathBuf> {
    let user_path = get_default_config_path();
    if user_path.exists() {
        return Ok(user_path);
    }

    let sys_paths = vec![
        PathBuf::from("/etc/mygestures"),
        PathBuf::from("/usr/local/etc/mygestures"),
        PathBuf::from("/usr/share/mygestures"),
        PathBuf::from("/usr/local/share/mygestures"),
        PathBuf::from("."),
    ];

    let suffix = get_environment_suffix();
    let mut default_content = None;

    for base in &sys_paths {
        let path = if let Some(s) = suffix {
            base.join(format!("mygestures_{}.yaml", s))
        } else {
            base.join("mygestures.yaml")
        };
        if path.exists() {
            if let Ok(content) = fs::read_to_string(&path) {
                default_content = Some(content);
                break;
            }
        }
    }

    if default_content.is_none() {
        for base in &sys_paths {
            let path = base.join("mygestures.yaml");
            if path.exists() {
                if let Ok(content) = fs::read_to_string(&path) {
                    default_content = Some(content);
                    break;
                }
            }
        }
    }

    if let Some(content) = default_content {
        if let Some(parent) = user_path.parent() {
            fs::create_dir_all(parent)?;
        }
        fs::write(&user_path, &content)?;
        println!("Initialized user configuration from template at: {}", user_path.display());
    } else {
        if let Some(parent) = user_path.parent() {
            fs::create_dir_all(parent)?;
        }
        fs::write(&user_path, "# MyGestures Configuration\nglobal:\n")?;
        println!("Initialized empty user configuration at: {}", user_path.display());
    }

    Ok(user_path)
}

impl Configuration {
    pub fn load_from_defaults() -> Self {
        let user_path = get_default_config_path();
        let mut config = Configuration {
            gestures: Vec::new(),
            apps: Vec::new(),
            user_config_path: user_path.clone(),
        };

        if user_path.exists() {
            if let Ok(content) = fs::read_to_string(&user_path) {
                config.parse_and_merge(&content, true);
            }
        }

        config
    }

    pub fn load_from_file<P: AsRef<Path>>(path: P) -> Option<Self> {
        let content = fs::read_to_string(path.as_ref()).ok()?;
        let mut config = Configuration {
            gestures: Vec::new(),
            apps: Vec::new(),
            user_config_path: path.as_ref().to_path_buf(),
        };
        config.parse_and_merge(&content, true);
        Some(config)
    }

    fn parse_and_merge(&mut self, content: &str, is_user: bool) -> bool {
        let parsed: YamlConfig = match serde_yaml::from_str(content) {
            Ok(c) => c,
            Err(e) => {
                log::error!("YAML parse error: {}", e);
                return false;
            }
        };

        if let Some(global) = parsed.global {
            for (name, gest_cfg) in global {
                let actions = gest_cfg.action_expr.to_actions();
                let is_abort = actions.contains(&ActionType::Abort);
                let raw_movement = gest_cfg.movement_expr.unwrap_or_else(|| "0,0 0,0".to_string());
                let points = parse_points(&raw_movement);

                let matched_pos = if let Some(ref id) = gest_cfg.id {
                    self.gestures.iter().position(|g| &g.id == id)
                } else {
                    self.gestures.iter().position(|g| g.name == name)
                };

                if let Some(pos) = matched_pos {
                    let existing = &mut self.gestures[pos];
                    if is_user {
                        existing.is_modified = true;
                    }
                    if let Some(ref id) = gest_cfg.id {
                        existing.id = id.clone();
                    }
                    existing.name = name;
                    existing.raw_movement = raw_movement;
                    existing.points = points;
                    existing.actions = actions;
                    existing.is_deleted = is_abort;
                } else {
                    let id = gest_cfg.id.clone().unwrap_or_else(|| {
                        if is_user {
                            generate_unique_id()
                        } else {
                            slugify(&name)
                        }
                    });
                    self.gestures.push(Gesture {
                        id,
                        name,
                        raw_movement,
                        points,
                        actions,
                        is_custom: is_user,
                        is_modified: false,
                        is_deleted: is_abort,
                    });
                }
            }
        }

        if let Some(apps) = parsed.apps {
            self.apps = apps;
        }

        true
    }

    pub fn save_to_file(&self) -> std::io::Result<()> {
        if let Some(parent) = self.user_config_path.parent() {
            fs::create_dir_all(parent)?;
        }

        let mut lines = Vec::new();
        lines.push("# MyGestures Configuration - Generated by Gestos".to_string());
        lines.push("".to_string());

        if !self.gestures.is_empty() {
            lines.push("global:".to_string());
            for g in &self.gestures {
                lines.push(format!("  \"{}\":", g.name));
                lines.push(format!("    move: \"{}\"", g.raw_movement));
                if !g.actions.is_empty() {
                    let action_str = g.actions.iter()
                        .map(|a| a.to_string())
                        .collect::<Vec<_>>()
                        .join(", ");
                    lines.push(format!("    do: {}", action_str));
                }
            }
            lines.push("".to_string());
        }

        // We can also serialize apps config here if present
        if !self.apps.is_empty() {
            if let Ok(apps_yaml) = serde_yaml::to_string(&self.apps) {
                // Strip the leading "---" of yaml if present
                let clean = apps_yaml.strip_prefix("---\n").unwrap_or(&apps_yaml);
                lines.push("apps:".to_string());
                for line in clean.lines() {
                    lines.push(format!("  {}", line));
                }
            }
        }

        fs::write(&self.user_config_path, lines.join("\n"))?;
        Ok(())
    }
}

pub fn generate_unique_id() -> String {
    if let Ok(mut file) = std::fs::File::open("/dev/urandom") {
        use std::io::Read;
        let mut bytes = [0u8; 8];
        if file.read_exact(&mut bytes).is_ok() {
            return bytes.iter().map(|b| format!("{:02x}", b)).collect();
        }
    }
    use std::time::{SystemTime, UNIX_EPOCH};
    let start = SystemTime::now().duration_since(UNIX_EPOCH).unwrap_or_default().as_nanos();
    format!("{:x}", start)
}

fn slugify(s: &str) -> String {
    s.to_lowercase()
        .chars()
        .map(|c| if c.is_alphanumeric() { c } else { '-' })
        .collect::<String>()
        .split('-')
        .filter(|part| !part.is_empty())
        .collect::<Vec<_>>()
        .join("-")
}
