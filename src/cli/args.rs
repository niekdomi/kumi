use clap::{Args, Parser, Subcommand};
use owo_colors::{OwoColorize, Stream};

#[derive(Clone, Copy)]
enum GroupColor {
    Cyan,
    Green,
    Yellow,
    Magenta,
}

impl GroupColor {
    fn paint(self, text: &str) -> String {
        match self {
            GroupColor::Cyan => text
                .if_supports_color(Stream::Stdout, |t| t.cyan())
                .to_string(),
            GroupColor::Green => text
                .if_supports_color(Stream::Stdout, |t| t.green())
                .to_string(),
            GroupColor::Yellow => text
                .if_supports_color(Stream::Stdout, |t| t.yellow())
                .to_string(),
            GroupColor::Magenta => text
                .if_supports_color(Stream::Stdout, |t| t.magenta())
                .to_string(),
        }
    }
}

struct HelpGroup {
    title: &'static str,
    color: GroupColor,
    commands: &'static [(&'static str, &'static str)],
}

const HELP_GROUPS: &[HelpGroup] = &[
    HelpGroup {
        title: "Build",
        color: GroupColor::Cyan,
        commands: &[
            (
                "build",
                "Compile the project [--release] [--target] [--jobs]",
            ),
            ("run", "Build and execute a target [--release] [--target]"),
            ("clean", "Remove build artifacts"),
            ("check", "Type-check without building"),
        ],
    },
    HelpGroup {
        title: "Project",
        color: GroupColor::Green,
        commands: &[("init", "Create a new project")],
    },
    HelpGroup {
        title: "Dependencies",
        color: GroupColor::Yellow,
        commands: &[
            ("add", "Add a dependency"),
            ("update", "Update dependencies [<name>]"),
            ("remove", "Remove a dependency"),
            ("search", "Search for packages in the registry"),
        ],
    },
    HelpGroup {
        title: "Tooling",
        color: GroupColor::Magenta,
        commands: &[
            ("serve", "Start the LSP server"),
            ("fmt", "Format kumi files [--check]"),
        ],
    },
];

pub fn print_help() {
    let s = Stream::Stdout;
    println!(
        "{}",
        "kumi \u{2014} a modern C++ build system".if_supports_color(s, |t| t.bold())
    );
    println!();
    println!(
        "{} kumi {} {}",
        "Usage:".if_supports_color(s, |t| t.bold()),
        "<command>".if_supports_color(s, |t| t.cyan()),
        "[options]".if_supports_color(s, |t| t.dimmed())
    );
    println!();

    let max_cmd_len = HELP_GROUPS
        .iter()
        .flat_map(|g| g.commands.iter().map(|(cmd, _)| cmd.len()))
        .max()
        .unwrap_or(0);

    for group in HELP_GROUPS {
        println!("{}", group.title.if_supports_color(s, |t| t.bold()));
        for (cmd, desc) in group.commands {
            let padded = format!("{:width$}", cmd, width = max_cmd_len);
            println!(
                "  {}   {}",
                group.color.paint(&padded),
                desc.if_supports_color(s, |t| t.dimmed())
            );
        }
        println!();
    }

    println!(
        "{} kumi build --release --target myapp",
        "Example:".if_supports_color(s, |t| t.dimmed())
    );
    println!(
        "{} kumi help <command>",
        "        ".if_supports_color(s, |t| t.dimmed())
    );
}

#[derive(Parser)]
#[command(name = "kumi")]
#[command(disable_help_flag = true)]
#[command(disable_help_subcommand = true)]
#[command(help_template = "")]
pub struct Cli {
    #[command(subcommand)]
    pub command: Option<Commands>,

    #[arg(short, long)]
    pub help: bool,
}

#[derive(Subcommand)]
pub enum Commands {
    Build(BuildArgs),
    Run(RunArgs),
    Clean(CleanArgs),
    Check(CheckArgs),
    Add(AddArgs),
    Update(UpdateArgs),
    Remove(RemoveArgs),
    Search(SearchArgs),
    Init(InitArgs),
    Fmt(FmtArgs),
    Serve(ServeArgs),
}

#[derive(Args)]
pub struct BuildArgs {
    /// Build with release optimizations
    #[arg(long)]
    pub release: bool,
    /// Build a specific target
    #[arg(long)]
    pub target: Option<String>,
    /// Number of parallel jobs
    #[arg(long, short)]
    pub jobs: Option<usize>,
    /// Print verbose output
    #[arg(long, short)]
    pub verbose: bool,
}

#[derive(Args)]
pub struct RunArgs {
    /// Build with release optimizations
    #[arg(long)]
    pub release: bool,
    /// Build and run a specific target
    #[arg(long)]
    pub target: Option<String>,
    /// Arguments to pass to the executable
    #[arg(last = true)]
    pub args: Vec<String>,
}

#[derive(Args)]
pub struct CleanArgs {
    /// Clean release artifacts
    #[arg(long)]
    pub release: bool,
    /// Clean all artifacts and caches
    #[arg(long)]
    pub all: bool,
}

#[derive(Args)]
pub struct InitArgs {
    /// Name of the project
    #[arg(long)]
    pub name: Option<String>,
    /// Create a library project
    #[arg(long)]
    pub lib: bool,
    /// Create a bare project without the TUI template
    #[arg(long)]
    pub bare: bool,
}

#[derive(Args)]
pub struct AddArgs {
    /// Name of the package to add
    pub name: String,
    /// Version string
    #[arg(long)]
    pub version: Option<String>,
    /// Git repository URL
    #[arg(long)]
    pub git: Option<String>,
    /// Add as a development dependency
    #[arg(long)]
    pub dev: bool,
}

#[derive(Args)]
pub struct UpdateArgs {
    /// Name of the package to update (or all if not specified)
    pub name: Option<String>,
}

#[derive(Args)]
pub struct RemoveArgs {
    /// Name of the package to remove
    pub name: String,
}

#[derive(Args)]
pub struct FmtArgs {
    /// Check formatting without applying changes
    #[arg(long)]
    pub check: bool,
    /// Specific file to format
    #[arg(long)]
    pub path: Option<String>,
}

#[derive(Args)]
pub struct ServeArgs {
    /// Port to listen on (TCP mode)
    #[arg(long)]
    pub port: Option<u16>,
    /// Communicate via stdin/stdout
    #[arg(long)]
    pub stdio: bool,
}

#[derive(Args)]
pub struct CheckArgs {
    /// Check a specific target
    #[arg(long)]
    pub target: Option<String>,

    /// Kumi file to parse and check
    pub file: Option<String>,
}

#[derive(Args)]
pub struct SearchArgs {
    /// Package name
    pub query: String,
}
