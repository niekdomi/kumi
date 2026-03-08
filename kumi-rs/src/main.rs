pub mod char_utils;
pub mod lexer;
pub mod token;

use std::env;
use std::fs;
use std::time::Instant;

fn get_peak_memory_mb() -> f64 {
    if let Ok(content) = fs::read_to_string("/proc/self/status") {
        for line in content.lines() {
            if line.starts_with("VmHWM:") {
                let rest = line[6..].trim();
                if let Some(end) = rest.find(' ') {
                    if let Ok(kb) = rest[..end].parse::<usize>() {
                        return (kb as f64) / 1024.0;
                    }
                }
            }
        }
    }
    0.0
}


fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("Error: No input file provided");
        std::process::exit(1);
    }

    let filename = &args[1];
    let source = match fs::read(filename) {
        Ok(s) => s,
        Err(e) => {
            eprintln!("Error: Could not open file '{}': {}", filename, e);
            std::process::exit(1);
        }
    };

    let mem_before = get_peak_memory_mb();

    let start = Instant::now();
    let mut lexer = lexer::Lexer::new(&source);
    let tokens = match lexer.tokenize() {
        Ok(t) => t,
        Err(e) => {
            eprintln!("Lex Error: {}", e.message);
            std::process::exit(1);
        }
    };
    let lex_ns = start.elapsed().as_nanos() as f64;

    let mem_after_lex = get_peak_memory_mb();

    let size_mb = (source.len() as f64) / 1_000_000.0;
    let token_count = tokens.len();

    println!("╭─────────────────────────────────────────╮");
    println!("│ File Analysis                           │");
    println!("├─────────────────────────────────────────┤");
    println!(
        "│ File:       {:<28}│",
        filename.split('/').last().unwrap_or(filename)
    );
    println!("│ Size:       {:<7.2} MB{:<18}│", size_mb, "");
    println!("│ Tokens:     {:<28}│", token_count);
    println!("╰─────────────────────────────────────────╯\n");

    let lex_ms = lex_ns / 1_000_000.0;
    let parse_ms = 0.0;
    let total_ms = lex_ms + parse_ms;

    let lex_throughput = if lex_ms > 0.0 {
        size_mb / (lex_ms / 1000.0)
    } else {
        0.0
    };
    let parse_throughput = 0.0;
    let total_throughput = if total_ms > 0.0 {
        size_mb / (total_ms / 1000.0)
    } else {
        0.0
    };

    let mem_after = get_peak_memory_mb();

    println!("╭─────────────────────────────────────────╮");
    println!("│ Performance Metrics                     │");
    println!("├─────────────────────────────────────────┤");
    println!(
        "│ Lexing:  {:>10.4} ms {:>10.2} MB/s  │",
        lex_ms, lex_throughput
    );
    println!(
        "│ Parsing: {:>10.4} ms {:>10.2} MB/s  │",
        parse_ms, parse_throughput
    );
    println!(
        "│ Total:   {:>10.4} ms {:>10.2} MB/s  │",
        total_ms, total_throughput
    );
    println!("├─────────────────────────────────────────┤");
    println!(
        "│ Lex Memory:   {:>13.2} MB          │",
        mem_after_lex - mem_before
    );
    println!(
        "│ Parse Memory: {:>13.2} MB          │",
        mem_after - mem_after_lex
    );
    println!(
        "│ Peak RSS:     {:>13.2} MB          │",
        mem_after - mem_before
    );
    println!("╰─────────────────────────────────────────╯");
}
