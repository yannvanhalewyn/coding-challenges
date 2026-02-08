use std::process::exit;
use std::fs::File;
use std::io::{BufReader, Read};

struct Options {
    command: String,
    input_filename: String,
    output_filename: String,
}

fn parse_args(args: &Vec<String>) -> Options {
    let mut output = None;

    for i in 3..args.len()  {
        if args[i] == "-o" && i + 1 < args.len() {
            output = Some(args[i + 1].clone());
            break;
        }
    }

    let output_filename = match output {
        Some(s) => s,
        None => {
            eprintln!("Error: Missing -o option");
            exit(1);
        }
    };

    Options {
        command: args[1].clone(),
        input_filename: args[2].clone(),
        output_filename,
    }
}

fn print_usage(program_name: &str) {
    println!("Usage: {} <command> <input_file> [options]", program_name);
    println!("\nCommands:");
    println!("  encode    Encode a file using Huffman compression");
    println!("  decode    Decode a Huffman-encoded file");
    println!("\nOptions:");
    println!("  -o, --output FILE    Output file (default: <input>.encoded/.decoded)");
    println!("  -h, --help           Show this help message");
    println!("  -v, --verbose        Verbose output");
    println!("\nExamples:");
    println!("  {} encode test.txt", program_name);
    println!("  {} encode test.txt -o compressed.huf", program_name);
    println!("  {} decode test.txt.encoded -o restored.txt", program_name);
}

fn encode(opts: &Options) {
    let input_file = File::open(&opts.input_filename).expect("Failed to open file");
    let mut reader = BufReader::new(input_file);
    let mut buffer = [0; 1]; // 1-byte buffer

    loop {
        match reader.read(&mut buffer) {
            Ok(0) => break, // EOF
            Ok(_) => {
                let byte = buffer[0];
                println!("Read byte: {}", byte);
            },
            Err(e) => panic!("Error reading: {}", e),
        }
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();

    if args.is_empty() {
        print_usage(&args[0]);
        exit(0)
    } else if args.len() < 4 {
        print_usage(&args[0]);
        exit(-1);
    } else {
        let opts = parse_args(&args);
        match opts.command.as_str() {
            "encode" => encode(&opts),
            "decode" => println!("Decoding {} to {:?}", opts.input_filename, opts.output_filename),
            _ => {
                eprintln!("Error: Unknown command '{}'", opts.command);
                print_usage(&args[0]);
                exit(1);
            }
        }
    }
}
