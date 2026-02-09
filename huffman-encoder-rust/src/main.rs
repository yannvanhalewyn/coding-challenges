use std::collections::HashMap;
use std::fmt;
use std::fs::File;
use std::io::{BufReader, Read, Write, Seek, SeekFrom, Result as IoResult};
use std::process::exit;

// Option Parsing
////////////////////////////////////////////////////////////////////////////////

struct Options {
    command: String,
    input_filename: String,
    output_filename: String,
}

fn parse_args(args: &[String]) -> Options {
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

// Frequency table
////////////////////////////////////////////////////////////////////////////////

// Returns a map of ascii char to count (uint32)
fn calculate_frequencies(input_file: &File) -> HashMap<u8, u32> {
    let mut frequencies = HashMap::new();

    let mut reader = BufReader::new(input_file);
    let mut buffer = [0; 1]; // 1-byte buffer

    loop {
        match reader.read(&mut buffer) {
            Ok(0) => break, // EOF
            Ok(_) => {
                let byte = buffer[0];
                *frequencies.entry(byte).or_insert(0) += 1;
            },
            Err(e) => panic!("Error reading: {}", e),
        }
    }
    frequencies
}

// Building Hufman tables
////////////////////////////////////////////////////////////////////////////////

enum HuffmanNode {
    Leaf {
        weight: u32,
        character: u8,
    },
    Parent {
        weight: u32,
        left: Box<HuffmanNode>,
        right: Box<HuffmanNode>,
    }
}

impl HuffmanNode {
    pub fn new_leaf(character: u8, weight: u32) -> Self {
        HuffmanNode::Leaf { weight, character }
    }

    pub fn new_parent(left: Box<HuffmanNode>, right: Box<HuffmanNode>) -> Self {
        let weight = left.weight() + right.weight();
        HuffmanNode::Parent { weight, left, right, }
    }

    pub fn weight(&self) -> u32 {
        match self {
            HuffmanNode::Leaf { weight, .. } => *weight,
            HuffmanNode::Parent { weight, .. } => *weight,
        }
    }
}

impl fmt::Display for HuffmanNode {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            HuffmanNode::Leaf { character, .. } => {
                write!(f, "'{}'", *character as char)
            },
            HuffmanNode::Parent { left, right, .. } => {
                write!(f, "(parent of {} and {})", left, right)
            }
        }
    }
}

fn build_huffman_tree(frequencies: &HashMap<u8, u32>) -> Box<HuffmanNode> {
    let mut nodes: Vec<Box<HuffmanNode>> = frequencies
        .iter()
        .filter(|(_, &count) | count > 0)
        .map(|(&byte, &count)| Box::new(HuffmanNode::new_leaf(byte, count)))
        .collect();

    // Continually sort and join the two lowest nodes. Could be made more
    // efficient by using a BinaryHeap
    while nodes.len() > 1 {
        nodes.sort_by_key(|a| a.weight());
        let left = nodes.remove(0);
        let right = nodes.remove(0);
        let parent = Box::new(HuffmanNode::new_parent(left, right));
        nodes.push(parent);
    }
    nodes.pop().unwrap()
}

struct Code {
    bits: u8,
    length: u8,
}

fn traverse(node: &HuffmanNode, code: Code, table: &mut HashMap<u8, Code>) {
    println!("Traversing {}, {:#010b}:{}", node, code.bits, code.length);
    match node {
        HuffmanNode::Leaf { character, .. } => {
            let code_record = Code { bits: code.bits, length: code.length };
            table.insert(*character, code_record);
        }
        HuffmanNode::Parent { left, right, .. } => {
            // Left just increments the length, keeping a 0
            traverse(left, Code { bits: code.bits, length: code.length + 1}, table);
            // Flips the next bit to '1'
            // 1 = 0b00000001
            // 1 << 7 = 0b10000000
            // 1 << 6 = 0b01000000 etc..
            let right_bits = code.bits | 1 << (7 - code.length);
            traverse(right, Code { bits: right_bits, length: code.length + 1}, table);
        }
    }
}

// Encoding Table
////////////////////////////////////////////////////////////////////////////////


// Builds a map from character to binary code, so 'a'-> 10
fn build_encoding_table(tree: &HuffmanNode) -> HashMap<u8, Code> {
    let mut table = HashMap::new();
    traverse(tree, Code { bits: 0, length: 0 }, &mut table);
    table
}

// Codec
////////////////////////////////////////////////////////////////////////////////

fn encode_header(file: &mut File, frequencies: &HashMap<u8, u32>) -> IoResult<()> {
    file.write_all(b"HUFF")?;
    // Write unique number of chars in frequency table
    file.write_all(&(frequencies.len() as u32).to_le_bytes())?;
    // Write 1 placeholder byte for the padding to be written later
    file.write_all(&0u8.to_le_bytes())?;

    // Write all the entries of the frequencies table
    for (character, frequency) in frequencies {
        file.write_all(&[*character])?;
        file.write_all(&frequency.to_le_bytes())?;
    }
    Ok(())
}

struct BitWriter<'a> {
    current_byte: u8,
    bits_filled: u8,
    output_file: &'a mut File,
    total_bits: u32,
}

impl<'a> BitWriter<'a> {
    fn new(file: &'a mut File) -> IoResult<Self> {
        Ok(BitWriter {
            current_byte: 0,
            bits_filled: 0,
            output_file: file,
            total_bits: 0,
        })
    }

    fn write_bit(&mut self, bit: bool) -> IoResult<()> {
        if bit {
            // Flip the bit at the next position
            self.current_byte |= 1 << (7 - self.bits_filled);
        }
        self.bits_filled += 1;
        self.total_bits += 1;

        if self.bits_filled == 8 {
            self.output_file.write_all(&[self.current_byte])?;
            self.current_byte = 0;
            self.bits_filled = 0;
        }
        Ok(())
    }

    fn write_bits(&mut self, bits: u8, length: u8) -> IoResult<()> {
        for i in 0..length {
            // Example:
            //   bits = 0b11010000 and length = 4
            // bits >> (7 - i) is about bits in order to be rightmost
            // bits >> (7 - 0) -> bits >> 7 = 0b00000001 (first bit to rightmost)
            // bits >> 6 = 0b00000011 (second bit to rightmost)
            // bits >> 5 = 0b00000110 (etc..)
            // bits >> 4 = 0b00001101
            let bit = (bits >> (7 - i)) & 1;
            self.write_bit(bit == 1)?;
        }
        Ok(())
    }

    fn flush(&mut self) -> IoResult<()> {
        if self.bits_filled > 0 {
            self.output_file.write_all(&[self.current_byte])?;
            self.current_byte = 0;
            self.bits_filled = 0;
        }
        self.output_file.sync_all()?;
        Ok(())
    }
}

fn encode_file(
    input_file: &mut File, output_file: &mut File,
    frequencies: &HashMap<u8, u32>, encoding_table: &HashMap<u8, Code>
) -> IoResult<()> {

    encode_header(output_file, frequencies)?;

    input_file.seek(SeekFrom::Start(0))?;
    let mut reader = BufReader::new(input_file);
    let mut bit_writer = BitWriter::new(output_file)?;
    let mut buffer = [0u8; 1]; // 1-byte buffer;

    loop {
        match reader.read(&mut buffer) {
            Ok(0) => break, // EOF,
            Ok(_) => {
                let byte = buffer[0];
                match encoding_table.get(&byte) {
                    Some(code) => {
                        println!("Char '{}', Code: {:#010b}, count: {}",
                            byte as char, code.bits, code.length
                        );
                        bit_writer.write_bits(code.bits, code.length)?;
                    },
                    None => {
                        panic!("Character not in encoding table: {}", byte);
                    }
                }
            },
            Err(e) => panic!("Error reading: {}", e),
        }
    }
    bit_writer.flush()?;

    Ok(())
}

fn print_encoding_table(encoding_table: &HashMap<u8, Code>, frequencies: &HashMap<u8, u32>) {
    for (character, frequency) in frequencies {
        let code = encoding_table.get(character).unwrap();
        println!("Char '{}' - freq: {}, encoding: {:#b}, length: {}",
            *character as char, frequency, code.bits, code.length
        );
    }
}

fn encode(opts: &Options) -> IoResult<()> {
    // Compute frequencies, huffman tree and encoding table
    let mut input_file = File::open(&opts.input_filename).expect("Failed to open file");
    let frequencies = calculate_frequencies(&input_file);
    let tree = build_huffman_tree(&frequencies);
    let encoding_table = build_encoding_table(&tree);
    print_encoding_table(&encoding_table, &frequencies);

    // Encode the file
    let mut output_file = File::create(&opts.output_filename)?;
    match encode_file(&mut input_file, &mut output_file, &frequencies, &encoding_table) {
        Ok(()) => println!("Encoding successful"),
        Err(e) => eprintln!("Encoding failed: {}", e),
    }

    Ok(())
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
            "encode" => { let _ = encode(&opts); },
            "decode" => println!("Decoding {} to {:?}", opts.input_filename, opts.output_filename),
            _ => {
                eprintln!("Error: Unknown command '{}'", opts.command);
                print_usage(&args[0]);
                exit(1);
            }
        }
    }
}
