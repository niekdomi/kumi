use clap::{Args, Parser, Subcommand};
use owo_colors::{OwoColorize, Stream};

// TODO: Refactor the Commands, so its less fragile and easier to extend. e.g.,
// we shouldn't define "[options]", this should be inferred by the amounts of
// options available. So we should making the Command handling smarter in general

macro_rules! paint {
    ($val:expr, $stream:expr, $style:ident) => {
        $val.if_supports_color($stream, |t| t.$style())
    };
}

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
            Self::Cyan => paint!(text, Stream::Stdout, cyan).to_string(),
            Self::Green => paint!(text, Stream::Stdout, green).to_string(),
            Self::Yellow => paint!(text, Stream::Stdout, yellow).to_string(),
            Self::Magenta => paint!(text, Stream::Stdout, magenta).to_string(),
        }
    }
}

struct Command {
    name: &'static str,
    summary: &'static str,
    arguments: &'static str,
    options: &'static [(&'static str, &'static str)],
}

struct CommandGroup {
    title: &'static str,
    color: GroupColor,
    commands: &'static [Command],
}

const COMMANDS: &[CommandGroup] = &[
    CommandGroup {
        title: "Build",
        color: GroupColor::Cyan,
        commands: &[
            Command {
                name: "build",
                summary: "Compile the project",
                arguments: "[options]",
                options: &[
                    ("--release", "Build with release optimizations"),
                    ("--target <name>", "Build a specific target"),
                    ("-j, --jobs <n>", "Number of parallel jobs"),
                    ("-v, --verbose", "Print verbose output"),
                ],
            },
            Command {
                name: "run",
                summary: "Build and execute a target",
                arguments: "[options] [-- <args>...]",
                options: &[
                    ("--release", "Build with release optimizations"),
                    ("--target <name>", "Build and run a specific target"),
                    ("-- <args>...", "Arguments to pass to the executable"),
                ],
            },
            Command {
                name: "clean",
                summary: "Remove build artifacts",
                arguments: "[options]",
                options: &[
                    ("--release", "Clean release artifacts"),
                    ("--all", "Clean all artifacts and caches"),
                ],
            },
            Command {
                name: "check",
                summary: "Type-check without building",
                arguments: "[options] [file]",
                options: &[
                    ("--target <name>", "Check a specific target"),
                    ("<file>", "Kumi file to parse and check"),
                ],
            },
        ],
    },
    CommandGroup {
        title: "Project",
        color: GroupColor::Green,
        commands: &[Command {
            name: "init",
            summary: "Create a new project",
            arguments: "[options]",
            options: &[
                ("--name <name>", "Name of the project"),
                ("--lib", "Create a library project"),
                ("--bare", "Create a bare project without the TUI template"),
            ],
        }],
    },
    CommandGroup {
        title: "Dependencies",
        color: GroupColor::Yellow,
        commands: &[
            Command {
                name: "add",
                summary: "Add a dependency",
                arguments: "<name> [options]",
                options: &[
                    ("<name>", "Name of the package to add"),
                    ("--version <ver>", "Version string"),
                    ("--git <url>", "Git repository URL"),
                    ("--dev", "Add as a development dependency"),
                ],
            },
            Command {
                name: "update",
                summary: "Update dependencies",
                arguments: "[name]",
                options: &[("<name>", "Name of the package to update (or all if omitted)")],
            },
            Command {
                name: "remove",
                summary: "Remove a dependency",
                arguments: "<name>",
                options: &[("<name>", "Name of the package to remove")],
            },
            Command {
                name: "search",
                summary: "Search for packages in the registry",
                arguments: "<query>",
                options: &[("<query>", "Package name to search for")],
            },
        ],
    },
    CommandGroup {
        title: "Tooling",
        color: GroupColor::Magenta,
        commands: &[
            Command {
                name: "serve",
                summary: "Start the LSP server",
                arguments: "[options]",
                options: &[
                    ("--port <port>", "Port to listen on (TCP mode)"),
                    ("--stdio", "Communicate via stdin/stdout"),
                ],
            },
            Command {
                name: "fmt",
                summary: "Format kumi files",
                arguments: "[options] [file]",
                options: &[
                    ("--check", "Check formatting without applying changes"),
                    ("<file>", "Specific file to format"),
                ],
            },
        ],
    },
];

fn find_command(name: &str) -> Option<(&'static CommandGroup, &'static Command)> {
    COMMANDS
        .iter()
        .find_map(|g| g.commands.iter().find(|c| c.name == name).map(|c| (g, c)))
}

pub fn print_help() {
    let s = Stream::Stdout;
    println!("{}", paint!("kumi \u{2014} a modern C++ build system", s, bold));
    println!();
    println!(
        "{} kumi {} {}",
        paint!("Usage:", s, bold),
        paint!("<command>", s, cyan),
        paint!("[options]", s, dimmed)
    );
    println!();

    let max_cmd_len = COMMANDS
        .iter()
        .flat_map(|g| g.commands.iter().map(|c| c.name.len()))
        .max()
        .unwrap_or(0);

    for group in COMMANDS {
        println!("{}", paint!(group.title, s, bold));
        for cmd in group.commands {
            let padded = format!("{:width$}", cmd.name, width = max_cmd_len);
            println!("  {}   {}", group.color.paint(&padded), paint!(cmd.summary, s, dimmed));
        }
        println!();
    }

    println!("{} kumi build --release --target myapp", paint!("Example:", s, dimmed));
    println!("{} kumi help <command>", paint!("        ", s, dimmed));
}

pub fn print_subcommand_help(name: &str) {
    let s = Stream::Stdout;
    let Some((group, cmd)) = find_command(name) else {
        print_help();
        return;
    };

    println!("{} — {}", paint!(cmd.name, s, bold), cmd.summary);
    println!();
    let usage = format!("kumi {} {}", cmd.name, cmd.arguments);
    println!("{} {}", paint!("Usage:", s, bold), group.color.paint(usage.trim()));

    if !cmd.options.is_empty() {
        println!();
        println!("{}", paint!("Options:", s, bold));

        let max_opt_len = cmd.options.iter().map(|(opt, _)| opt.len()).max().unwrap_or(0);

        for (opt, desc) in cmd.options {
            let padded = format!("{opt:max_opt_len$}");
            println!("  {}   {}", paint!(&padded, s, green), paint!(desc, s, dimmed));
        }
    }
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
#[command(disable_help_flag = true)]
pub struct BuildArgs {
    #[arg(short, long)]
    pub help: bool,
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
#[command(disable_help_flag = true)]
pub struct RunArgs {
    #[arg(short, long)]
    pub help: bool,
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
#[command(disable_help_flag = true)]
pub struct CleanArgs {
    #[arg(short, long)]
    pub help: bool,
    /// Clean release artifacts
    #[arg(long)]
    pub release: bool,
    /// Clean all artifacts and caches
    #[arg(long)]
    pub all: bool,
}

#[derive(Args)]
#[command(disable_help_flag = true)]
pub struct InitArgs {
    #[arg(short, long)]
    pub help: bool,
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
#[command(disable_help_flag = true)]
pub struct AddArgs {
    #[arg(short, long)]
    pub help: bool,
    /// Name of the package to add
    pub name: Option<String>,
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
#[command(disable_help_flag = true)]
pub struct UpdateArgs {
    #[arg(short, long)]
    pub help: bool,
    /// Name of the package to update (or all if not specified)
    pub name: Option<String>,
}

#[derive(Args)]
#[command(disable_help_flag = true)]
pub struct RemoveArgs {
    #[arg(short, long)]
    pub help: bool,
    /// Name of the package to remove
    pub name: Option<String>,
}

#[derive(Args)]
#[command(disable_help_flag = true)]
pub struct FmtArgs {
    #[arg(short, long)]
    pub help: bool,
    /// Check formatting without applying changes
    #[arg(long)]
    pub check: bool,
    /// Specific file to format
    pub file: Option<String>,
}

#[derive(Args)]
#[command(disable_help_flag = true)]
pub struct ServeArgs {
    #[arg(short, long)]
    pub help: bool,
    /// Port to listen on (TCP mode)
    #[arg(long)]
    pub port: Option<u16>,
    /// Communicate via stdin/stdout
    #[arg(long)]
    pub stdio: bool,
}

#[derive(Args)]
#[command(disable_help_flag = true)]
pub struct CheckArgs {
    #[arg(short, long)]
    pub help: bool,
    /// Check a specific target
    #[arg(long)]
    pub target: Option<String>,
    /// Kumi file to parse and check
    pub file: Option<String>,
}

#[derive(Args)]
#[command(disable_help_flag = true)]
pub struct SearchArgs {
    #[arg(short, long)]
    pub help: bool,
    /// Package name
    pub query: Option<String>,
}
