use crate::cli::args::InitArgs;
use anyhow::{Context, Result};
use dialoguer::{Input, Select, theme::ColorfulTheme};
use owo_colors::{OwoColorize, Stream};
use std::env;
use std::fs;
use std::path::Path;
use std::process::Command;

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
    fn as_str(self) -> &'static str {
        match self {
            Language::Cpp => "C++",
            Language::C => "C",
        }
    }

    fn standards(self) -> Vec<&'static str> {
        match self {
            Language::Cpp => vec!["11", "14", "17", "20", "23", "26"],
            Language::C => vec!["99", "11", "17", "23"],
        }
    }

    fn default_standard_index(self) -> usize {
        match self {
            Language::Cpp => 4, // 23
            Language::C => 2,   // 17
        }
    }

    fn standard_key(self) -> &'static str {
        match self {
            Language::Cpp => "cpp-standard",
            Language::C => "c-standard",
        }
    }
}

#[derive(Debug, PartialEq, Clone, Copy)]
enum ProjectType {
    Executable,
    Library,
}

impl ProjectType {
    fn as_str(self) -> &'static str {
        match self {
            ProjectType::Executable => "executable",
            ProjectType::Library => "static-library",
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
            project_type: if args.lib {
                ProjectType::Library
            } else {
                ProjectType::Executable
            },
            standard: "23".to_string(),
        }
    }
}

//===----------------------------------------------------------------------===//
// Prompt helper — handles cancellation uniformly
//===----------------------------------------------------------------------===//

macro_rules! prompt_opt {
    ($expr:expr) => {
        match $expr {
            Ok(Some(v)) => v,
            Ok(None) | Err(_) => {
                use owo_colors::{OwoColorize, Stream};
                println!("{}", "Operation canceled.".if_supports_color(Stream::Stdout, |t| t.style(owo_colors::Style::new().red().bold())));
                std::process::exit(1);
            }
        }
    };
}

macro_rules! prompt_val {
    ($expr:expr) => {
        match $expr {
            Ok(v) => v,
            Err(_) => {
                use owo_colors::{OwoColorize, Stream};
                println!("{}", "Operation canceled.".if_supports_color(Stream::Stdout, |t| t.style(owo_colors::Style::new().red().bold())));
                std::process::exit(1);
            }
        }
    };
}

//===----------------------------------------------------------------------===//
// Entry point
//===----------------------------------------------------------------------===//

pub fn run(args: InitArgs) -> Result<()> {
    let current_dir = env::current_dir().context("failed to get current directory")?;
    let default_name = current_dir
        .file_name()
        .unwrap_or_default()
        .to_string_lossy()
        .to_string();

    let config = if args.bare {
        WizardConfig::from_args(&args, default_name)
    } else {
        println!("{}\n", "Initializing a new Kumi project".if_supports_color(Stream::Stdout, |t| t.bold()));
        run_wizard(&args, default_name)?
    };

    create_project(&config)?;
    init_git()?;

    let s = Stream::Stdout;
    println!(
        "\n{} Created {} project `{}`",
        "success".if_supports_color(s, |t| t.style(owo_colors::Style::new().green().bold())),
        config.project_type.as_str(),
        config.project_name.if_supports_color(s, |t| t.cyan()),
    );

    Ok(())
}

//===----------------------------------------------------------------------===//
// Wizard
//===----------------------------------------------------------------------===//

fn run_wizard(args: &InitArgs, default_name: String) -> Result<WizardConfig> {
    let theme = ColorfulTheme::default();

    // Project name
    let initial_name = args.name.clone().unwrap_or(default_name.clone());
    let project_name: String = prompt_val!(
        Input::with_theme(&theme)
            .with_prompt("Project name")
            .default(initial_name)
            .interact()
    );

    // Project type
    let types = vec!["Executable", "Library"];
    let type_selection = prompt_opt!(
        Select::with_theme(&theme)
            .with_prompt("Project type")
            .default(if args.lib { 1 } else { 0 })
            .items(&types)
            .interact_opt()
    );

    let project_type = match type_selection {
        1 => ProjectType::Library,
        _ => ProjectType::Executable,
    };

    // Language
    let languages = vec!["C++", "C"];
    let lang_selection = prompt_opt!(
        Select::with_theme(&theme)
            .with_prompt("Language")
            .default(0)
            .items(&languages)
            .interact_opt()
    );

    let language = match lang_selection {
        1 => Language::C,
        _ => Language::Cpp,
    };

    // Standard
    let standards = language.standards();
    let standard_idx = prompt_opt!(
        Select::with_theme(&theme)
            .with_prompt(format!("{} standard", language.as_str()))
            .default(language.default_standard_index())
            .items(&standards)
            .interact_opt()
    );
    let standard = standards[standard_idx].to_string();

    println!();

    Ok(WizardConfig {
        project_name,
        language,
        project_type,
        standard,
    })
}

//===----------------------------------------------------------------------===//
// Project creation
//===----------------------------------------------------------------------===//

fn create_project(cfg: &WizardConfig) -> Result<()> {
    let is_cpp = cfg.language == Language::Cpp;
    let is_lib = cfg.project_type == ProjectType::Library;

    let headers_line = if is_lib {
        format!(
            "  headers: \"include/**/*.{}\";\n  include-dirs: \"include\";",
            if is_cpp { "hpp" } else { "h" }
        )
    } else {
        String::new()
    };

    let kumi_content = KUMI_BUILD_TEMPLATE
        .replace("{PROJECT_NAME}", &cfg.project_name)
        .replace("{TARGET_TYPE}", cfg.project_type.as_str())
        .replace("{HEADERS_LINE}", &headers_line)
        .replace("cpp-standard", cfg.language.standard_key())
        .replace("{CPP_STANDARD}", &cfg.standard);

    // create and enter project directory
    let project_dir = Path::new(&cfg.project_name);
    if !project_dir.exists() {
        fs::create_dir_all(project_dir)
            .with_context(|| format!("failed to create directory '{}'", project_dir.display()))?;
    }
    env::set_current_dir(project_dir)
        .with_context(|| format!("failed to enter directory '{}'", project_dir.display()))?;

    fs::write(".gitignore", GITIGNORE_TEMPLATE).context("failed to write .gitignore")?;
    fs::write("kumi.build", kumi_content).context("failed to write kumi.build")?;
    fs::create_dir_all("src").context("failed to create src directory")?;

    if is_lib {
        create_library_files(cfg, is_cpp)?;
    } else {
        create_executable_files(cfg, is_cpp)?;
    }

    Ok(())
}

fn create_library_files(cfg: &WizardConfig, is_cpp: bool) -> Result<()> {
    let header_ext = if is_cpp { "hpp" } else { "h" };
    let source_ext = if is_cpp { "cpp" } else { "c" };

    let include_dir = format!("include/{}", cfg.project_name);
    fs::create_dir_all(&include_dir)
        .with_context(|| format!("failed to create directory '{include_dir}'"))?;

    let project_name_upper = cfg.project_name.to_uppercase();

    let header_template = if is_cpp {
        LIBRARY_HEADER_TEMPLATE
    } else {
        LIBRARY_HEADER_C_TEMPLATE
    };
    let source_template = if is_cpp {
        LIBRARY_SOURCE_TEMPLATE
    } else {
        LIBRARY_SOURCE_C_TEMPLATE
    };

    let header_content = header_template
        .replace("{PROJECT_NAME}", &cfg.project_name)
        .replace("{PROJECT_NAME_UPPER}", &project_name_upper);

    let source_content = source_template
        .replace("{PROJECT_NAME}", &cfg.project_name)
        .replace("{PROJECT_NAME_UPPER}", &project_name_upper);

    fs::write(
        format!("{include_dir}/{}.{header_ext}", cfg.project_name),
        header_content,
    )
    .context("failed to write header file")?;
    fs::write(
        format!("src/{}.{source_ext}", cfg.project_name),
        source_content,
    )
    .context("failed to write source file")?;

    Ok(())
}

fn create_executable_files(cfg: &WizardConfig, is_cpp: bool) -> Result<()> {
    let source_ext = if is_cpp { "cpp" } else { "c" };

    let source_content = if is_cpp {
        let template = match cfg.standard.as_str() {
            "23" | "26" => MAIN_CPP23_TEMPLATE,
            _ => MAIN_CPP_LEGACY_TEMPLATE,
        };
        template.replace("{PROJECT_NAME}", &cfg.project_name)
    } else {
        MAIN_C_TEMPLATE.replace("{PROJECT_NAME}", &cfg.project_name)
    };

    fs::write(format!("src/main.{source_ext}"), source_content)
        .context("failed to write main source file")?;

    Ok(())
}

//===----------------------------------------------------------------------===//
// Git
//===----------------------------------------------------------------------===//

fn init_git() -> Result<()> {
    if Path::new(".git").exists() {
        return Ok(());
    }
    let _ = Command::new("git")
        .args(["-q", "init"]) // -q suppresses default branch hints
        .stdout(std::process::Stdio::null())
        .stderr(std::process::Stdio::null())
        .status();
    Ok(())
}
