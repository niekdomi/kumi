pub mod cli;
pub mod diagnostics;
pub mod lang;

use crate::diagnostics::{Diagnostic, DiagnosticPrinter};
use clap::Parser as ClapParser;
use cli::args::{Cli, Commands};
use std::fs;
use std::time::Instant;

fn get_peak_memory_mb() -> f64 {
    if let Ok(content) = fs::read_to_string("/proc/self/status") {
        for line in content.lines() {
            if let Some(rest) = line.strip_prefix("VmHWM:") {
                let rest = rest.trim();
                if let Some(end) = rest.find(' ')
                    && let Ok(kb) = rest[..end].parse::<usize>()
                {
                    return (kb as f64) / 1024.0;
                }
            }
        }
    }
    0.0
}

fn run_parser_test(filename: &str) {
    let source = match fs::read(filename) {
        Ok(s) => s,
        Err(e) => {
            eprintln!("Error: Could not open file '{}': {}", filename, e);
            std::process::exit(1);
        }
    };

    let mem_before = get_peak_memory_mb();

    let start = Instant::now();
    let lexer = lang::lex::Lexer::new(&source);
    let (tokens, mut lex_errors) = lexer.tokenize();

    let mut open_braces = Vec::new();
    for token in &tokens {
        if token.kind == lang::lex::TokenType::LeftBrace {
            open_braces.push(token.position);
        } else if token.kind == lang::lex::TokenType::RightBrace && open_braces.pop().is_none() {
            lex_errors.push(Diagnostic::new(
                "unexpected closing brace",
                token.position,
                "check for extra punctuation",
            ));
        }
    }
    for pos in open_braces {
        lex_errors.push(Diagnostic::new(
            "unclosed brace",
            pos,
            "ensure scopes are correctly closed",
        ));
    }

    lex_errors.sort_by_key(|e| e.position);

    let lex_ns = start.elapsed().as_nanos() as f64;

    let source_str = std::str::from_utf8(&source).unwrap_or("");
    let printer = DiagnosticPrinter::new(source_str, filename);

    if !lex_errors.is_empty() {
        for e in &lex_errors {
            printer.print_error(e);
        }
        std::process::exit(1);
    }

    let mem_after_lex = get_peak_memory_mb();

    let parse_start = Instant::now();
    let parser = lang::parse::Parser::new(&tokens, &source);
    let ast = parser.parse(filename);
    let parse_ns = parse_start.elapsed().as_nanos() as f64;

    let mut has_errors = false;
    if !ast.errors.is_empty() {
        has_errors = true;
        for e in &ast.errors {
            printer.print_error(e);
        }
    }

    let mem_after_parse = get_peak_memory_mb();

    let check_start = Instant::now();
    let checker = lang::semantic::Checker::new();
    let check_errors = checker.check(&ast);
    let check_ns = check_start.elapsed().as_nanos() as f64;

    if !check_errors.is_empty() {
        has_errors = true;
        for e in &check_errors {
            printer.print_error(e);
        }
    }

    let mem_after_check = get_peak_memory_mb();

    let size_mb = (source.len() as f64) / 1_000_000.0;
    let token_count = tokens.len();

    println!("╭─────────────────────────────────────────╮");
    println!("│ File Analysis                           │");
    println!("├─────────────────────────────────────────┤");
    println!("│ File:       {:<28}│", filename.split('/').next_back().unwrap_or(filename));
    println!("│ Size:       {:<7.2} MB{:<18}│", size_mb, "");
    println!("│ Tokens:     {:<28}│", token_count);
    println!("│ AST Nodes:  {:<28}│", ast.statements.len());
    println!("╰─────────────────────────────────────────╯\n");

    let lex_ms = lex_ns / 1_000_000.0;
    let parse_ms = parse_ns / 1_000_000.0;
    let check_ms = check_ns / 1_000_000.0;
    let total_ms = lex_ms + parse_ms + check_ms;

    let lex_throughput = if lex_ms > 0.0 { size_mb / (lex_ms / 1000.0) } else { 0.0 };
    let parse_throughput = if parse_ms > 0.0 { size_mb / (parse_ms / 1000.0) } else { 0.0 };
    let check_throughput = if check_ms > 0.0 { size_mb / (check_ms / 1000.0) } else { 0.0 };
    let total_throughput = if total_ms > 0.0 { size_mb / (total_ms / 1000.0) } else { 0.0 };

    println!("╭─────────────────────────────────────────╮");
    println!("│ Performance Metrics                     │");
    println!("├─────────────────────────────────────────┤");
    println!("│ Lexing:  {:>10.4} ms {:>10.2} MB/s  │", lex_ms, lex_throughput);
    println!("│ Parsing: {:>10.4} ms {:>10.2} MB/s  │", parse_ms, parse_throughput);
    println!("│ Check:   {:>10.4} ms {:>10.2} MB/s  │", check_ms, check_throughput);
    println!("│ Total:   {:>10.4} ms {:>10.2} MB/s  │", total_ms, total_throughput);
    println!("├─────────────────────────────────────────┤");
    println!("│ Lex Memory:   {:>13.2} MB          │", mem_after_lex - mem_before);
    println!("│ Parse Memory: {:>13.2} MB          │", mem_after_parse - mem_after_lex);
    println!("│ Check Memory: {:>13.2} MB          │", mem_after_check - mem_after_parse);
    println!("│ Peak RSS:     {:>13.2} MB          │", mem_after_check - mem_before);
    println!("╰─────────────────────────────────────────╯");

    if has_errors {
        std::process::exit(1);
    }
}

fn main() {
    // Intercept clap parsing to completely suppress default errors/help and enforce our own.
    let cli = match Cli::try_parse() {
        Ok(c) => c,
        Err(_) => {
            cli::args::print_help();
            std::process::exit(1);
        }
    };

    if cli.help || cli.command.is_none() {
        cli::args::print_help();
        return;
    }

    macro_rules! check_help {
        ($args:expr, $name:literal) => {
            if $args.help {
                cli::args::print_subcommand_help($name);
                return;
            }
        };
    }

    match cli.command.unwrap() {
        Commands::Build(args) => {
            check_help!(args, "build");
            println!("Building project...");
            if args.release {
                println!("  Profile: release");
            }
        }
        Commands::Run(args) => {
            check_help!(args, "run");
            println!("Running project...");
            if !args.args.is_empty() {
                println!("  Arguments: {:?}", args.args);
            }
        }
        Commands::Clean(args) => {
            check_help!(args, "clean");
            println!("Cleaning project artifacts...");
            if args.all {
                println!("  Removing all caches and dependencies");
            }
        }
        Commands::Init(args) => {
            check_help!(args, "init");
            if let Err(e) = cli::init::run(args) {
                eprintln!("Error initializing project: {}", e);
                std::process::exit(1);
            }
        }
        Commands::Add(args) => {
            check_help!(args, "add");
            match args.name {
                Some(name) => println!("Adding dependency: {}", name),
                None => {
                    eprintln!("Error: missing required argument <name>");
                    cli::args::print_subcommand_help("add");
                    std::process::exit(1);
                }
            }
        }
        Commands::Update(args) => {
            check_help!(args, "update");
            if let Some(name) = args.name {
                println!("Updating dependency: {}", name);
            } else {
                println!("Updating all dependencies");
            }
        }
        Commands::Remove(args) => {
            check_help!(args, "remove");
            match args.name {
                Some(name) => println!("Removing dependency: {}", name),
                None => {
                    eprintln!("Error: missing required argument <name>");
                    cli::args::print_subcommand_help("remove");
                    std::process::exit(1);
                }
            }
        }
        Commands::Fmt(args) => {
            check_help!(args, "fmt");
            println!("Formatting kumi files...");
        }
        Commands::Serve(args) => {
            check_help!(args, "serve");
            println!("Starting Kumi LSP Server...");
            if let Some(port) = args.port {
                println!("  Listening on TCP port {}", port);
            } else {
                println!("  Using stdio transport");
            }
        }
        Commands::Check(args) => {
            check_help!(args, "check");
            if let Some(ref file) = args.file {
                run_parser_test(file);
            } else {
                println!("Checking project... (Fast validation without build)");
            }
        }
        Commands::Search(args) => {
            check_help!(args, "search");
            match args.query {
                Some(query) => println!("Searching for packages matching: {}", query),
                None => {
                    eprintln!("Error: missing required argument <query>");
                    cli::args::print_subcommand_help("search");
                    std::process::exit(1);
                }
            }
        }
    }
}
