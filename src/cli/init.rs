use crate::cli::args::InitArgs;
use dialoguer::{Input, Select, theme::ColorfulTheme};
use owo_colors::{OwoColorize, Style};
use std::{
    env, fs,
    io::{self},
    path::Path,
    process::{Command, exit},
};

//===----------------------------------------------------------------------===//
// Templates
//===----------------------------------------------------------------------===//

const GITIGNORE_TEMPLATE: &str = include_str!("templates/project/gitignore.template");
const KUMI_BUILD_TEMPLATE: &str = include_str!("templates/project/kumi_build.template");
const MAIN_CPP23_TEMPLATE: &str = include_str!("templates/source/main_cpp23.template");
const MAIN_CPP_LEGACY_TEMPLATE: &str = include_str!("templates/source/main_cpp_legacy.template");
const LIBRARY_HEADER_TEMPLATE: &str = include_str!("templates/source/library_header.template");
const LIBRARY_SOURCE_TEMPLATE: &str = include_str!("templates/source/library_source.template");
const MAIN_C_TEMPLATE: &str = include_str!("templates/source/main_c.template");
const LIBRARY_HEADER_C_TEMPLATE: &str = include_str!("templates/source/library_header_c.template");
const LIBRARY_SOURCE_C_TEMPLATE: &str = include_str!("templates/source/library_source_c.template");

//===----------------------------------------------------------------------===//
// Types
//===----------------------------------------------------------------------===//

#[derive(Debug, PartialEq, Clone, Copy)]
enum Language {
    Cpp,
    C,
}

impl Language {
    const fn as_str(self) -> &'static str {
        match self {
            Self::Cpp => "C++",
            Self::C => "C",
        }
    }

    const fn extension(self, is_header: bool) -> &'static str {
        match (self, is_header) {
            (Self::Cpp, true) => "hpp",
            (Self::Cpp, false) => "cpp",
            (Self::C, true) => "h",
            (Self::C, false) => "c",
        }
    }

    const fn standards(self) -> &'static [&'static str] {
        match self {
            Self::Cpp => &["11", "14", "17", "20", "23", "26"],
            Self::C => &["99", "11", "17", "23"],
        }
    }

    const fn default_standard_index(self) -> usize {
        match self {
            Self::Cpp => 4, // 23
            Self::C => 2,   // 17
        }
    }

    const fn standard_key(self) -> &'static str {
        match self {
            Self::Cpp => "cpp-standard",
            Self::C => "c-standard",
        }
    }
}

#[derive(Debug, PartialEq, Clone, Copy)]
enum ProjectType {
    Executable,
    Library,
}

impl ProjectType {
    const fn as_str(self) -> &'static str {
        match self {
            Self::Executable => "executable",
            Self::Library => "static-library",
        }
    }
}

#[derive(Debug)]
struct WizardConfig {
    project_name: String,
    language: Language,
    project_type: ProjectType,
    standard: String,
}

impl WizardConfig {
    fn from_args(args: &InitArgs, default_name: String) -> Self {
        Self {
            project_name: args.name.clone().unwrap_or(default_name),
            language: Language::Cpp,
            project_type: if args.lib { ProjectType::Library } else { ProjectType::Executable },
            standard: "23".to_string(),
        }
    }
}

//===----------------------------------------------------------------------===//
// Macros
//===----------------------------------------------------------------------===//

macro_rules! handle_cancel {
    () => {{
        let style = Style::new().red().bold();
        println!("{}", "Operation canceled.".style(style));
        exit(1);
    }};
}

macro_rules! prompt_opt {
    ($expr:expr) => {
        match $expr {
            Ok(Some(v)) => v,
            _ => handle_cancel!(),
        }
    };
}

macro_rules! prompt_val {
    ($expr:expr) => {
        match $expr {
            Ok(v) => v,
            _ => handle_cancel!(),
        }
    };
}

//===----------------------------------------------------------------------===//
// Logic
//===----------------------------------------------------------------------===//

pub fn run(args: &InitArgs) -> io::Result<()> {
    let default_name = env::current_dir()?
        .file_name()
        .unwrap_or_default()
        .to_string_lossy()
        .into_owned();

    let config = if args.bare {
        WizardConfig::from_args(args, default_name)
    } else {
        println!("{}", "Initializing a new Kumi project".bold());
        run_wizard(args, default_name)
    };

    create_project(&config)?;
    init_git();

    // Direct styling avoids the closure-lifetime issue
    let success_style = Style::new().green().bold();
    let name_style = Style::new().cyan();

    println!(
        "\n{} Created {} project `{}`",
        "success".style(success_style),
        config.project_type.as_str(),
        config.project_name.style(name_style),
    );

    Ok(())
}

fn run_wizard(args: &InitArgs, default_name: String) -> WizardConfig {
    let theme = ColorfulTheme::default();

    let project_name: String = prompt_val!(
        Input::with_theme(&theme)
            .with_prompt("Project name")
            .default(args.name.clone().unwrap_or(default_name))
            .interact()
    );

    let type_idx = prompt_opt!(
        Select::with_theme(&theme)
            .with_prompt("Project type")
            .default(usize::from(args.lib))
            .items(["Executable", "Library"])
            .interact_opt()
    );

    let lang_idx = prompt_opt!(
        Select::with_theme(&theme)
            .with_prompt("Language")
            .default(0)
            .items(["C++", "C"])
            .interact_opt()
    );

    let language = if lang_idx == 1 { Language::C } else { Language::Cpp };
    let project_type = if type_idx == 1 { ProjectType::Library } else { ProjectType::Executable };

    let standards = language.standards();
    let std_idx = prompt_opt!(
        Select::with_theme(&theme)
            .with_prompt(format!("{} standard", language.as_str()))
            .default(language.default_standard_index())
            .items(standards)
            .interact_opt()
    );

    println!();

    WizardConfig {
        project_name,
        language,
        project_type,
        standard: standards[std_idx].to_string(),
    }
}

fn create_project(cfg: &WizardConfig) -> io::Result<()> {
    let lang = cfg.language;
    let is_lib = cfg.project_type == ProjectType::Library;

    let headers_line = if is_lib {
        format!(
            "  headers: \"include/**/*.{}\";\n  include-dirs: \"include\";",
            lang.extension(true)
        )
    } else {
        String::new()
    };

    let kumi_content = KUMI_BUILD_TEMPLATE
        .replace("{PROJECT_NAME}", &cfg.project_name)
        .replace("{TARGET_TYPE}", cfg.project_type.as_str())
        .replace("{HEADERS_LINE}", &headers_line)
        .replace("cpp-standard", lang.standard_key())
        .replace("{CPP_STANDARD}", &cfg.standard);

    fs::create_dir_all(format!("{}/src", cfg.project_name))?;
    env::set_current_dir(&cfg.project_name)?;

    fs::write(".gitignore", GITIGNORE_TEMPLATE)?;
    fs::write("kumi.build", kumi_content)?;

    if is_lib {
        let inc_dir = format!("include/{}", cfg.project_name);
        fs::create_dir_all(&inc_dir)?;

        let (h_temp, s_temp) = if lang == Language::Cpp {
            (LIBRARY_HEADER_TEMPLATE, LIBRARY_SOURCE_TEMPLATE)
        } else {
            (LIBRARY_HEADER_C_TEMPLATE, LIBRARY_SOURCE_C_TEMPLATE)
        };

        let upper = cfg.project_name.to_uppercase();
        fs::write(
            format!("{}/{}.{}", inc_dir, cfg.project_name, lang.extension(true)),
            h_temp
                .replace("{PROJECT_NAME}", &cfg.project_name)
                .replace("{PROJECT_NAME_UPPER}", &upper),
        )?;
        fs::write(
            format!("src/{}.{}", cfg.project_name, lang.extension(false)),
            s_temp
                .replace("{PROJECT_NAME}", &cfg.project_name)
                .replace("{PROJECT_NAME_UPPER}", &upper),
        )?;
    } else {
        let template = match lang {
            Language::Cpp if matches!(cfg.standard.as_str(), "23" | "26") => MAIN_CPP23_TEMPLATE,
            Language::Cpp => MAIN_CPP_LEGACY_TEMPLATE,
            Language::C => MAIN_C_TEMPLATE,
        };
        fs::write(
            format!("src/main.{}", lang.extension(false)),
            template.replace("{PROJECT_NAME}", &cfg.project_name),
        )?;
    }

    Ok(())
}

fn init_git() {
    if !Path::new(".git").exists() {
        let _ = Command::new("git")
            .args(["-q", "init"])
            .stdout(std::process::Stdio::null())
            .stderr(std::process::Stdio::null())
            .status();
    }
}
