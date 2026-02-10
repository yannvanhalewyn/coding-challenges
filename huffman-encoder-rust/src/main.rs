use std::collections::HashMap;
use std::fmt;
use std::fs::File;
use std::io::{
    BufReader, Read, Result as IoResult, Error, ErrorKind,
    Seek, SeekFrom, Write
};
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

type FrequencyTable = HashMap<u8, u32>;

// Returns a map of ascii char to count (uint32)
fn calculate_frequencies(input_file: &File) -> FrequencyTable {
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

fn build_huffman_tree(frequencies: &FrequencyTable) -> Box<HuffmanNode> {
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

type EncodingTable = HashMap<u8, Code>;

fn traverse(node: &HuffmanNode, code: Code, encoding_table: &mut EncodingTable) {
    match node {
        HuffmanNode::Leaf { character, .. } => {
            let code_record = Code { bits: code.bits, length: code.length };
            encoding_table.insert(*character, code_record);
        }
        HuffmanNode::Parent { left, right, .. } => {
            // Left just increments the length, keeping a 0
            traverse(left, Code { bits: code.bits, length: code.length + 1}, encoding_table);
            // Flips the next bit to '1'
            // 1 = 0b00000001
            // 1 << 7 = 0b10000000
            // 1 << 6 = 0b01000000 etc..
            let right_bits = code.bits | 1 << (7 - code.length);
            traverse(right, Code { bits: right_bits, length: code.length + 1}, encoding_table);
        }
    }
}

// Encoding Table
////////////////////////////////////////////////////////////////////////////////

// Builds a map from character to binary code, so 'a'-> 10
fn build_encoding_table(tree: &HuffmanNode) -> EncodingTable {
    let mut encoding_table = HashMap::new();
    traverse(tree, Code { bits: 0, length: 0 }, &mut encoding_table);
    encoding_table
}

// Codec
////////////////////////////////////////////////////////////////////////////////

struct Header {
    num_entries: u32,
    padding_bits: u8,
    encoding_table: EncodingTable
}

fn encode_provisionary_header(file: &mut File, encoding_table: &EncodingTable) -> IoResult<()> {
    file.write_all(b"HRST")?; // Huffman Rust
    // Write unique number of chars in frequency table
    file.write_all(&(encoding_table.len() as u32).to_le_bytes())?;

    // Write 1 placeholder byte for the padding to be written later
    file.write_all(&0u8.to_le_bytes())?;

    // Write all the entries of the frequencies table
    for (character, code) in encoding_table {
        file.write_all(&[*character])?;
        file.write_all(&[code.bits])?;
        file.write_all(&[code.length])?;
    }
    Ok(())
}

fn encode_header_padding_bits(file: &mut File, padding_bits: u8) -> IoResult<()> {
    file.seek(SeekFrom::Start(8))?;
    file.write_all(&padding_bits.to_le_bytes())?;
    Ok(())
}

fn decode_header(reader: &mut BufReader<File>) -> IoResult<Header> {
    let mut huff_bytes = [0u8; 4];
    reader.read_exact(&mut huff_bytes)?;
    if huff_bytes != *b"HRST" {
        return Err(Error::new(ErrorKind::InvalidData, "Invalid file format"))
    }

    let mut num_entries_bytes = [0u8; 4];
    reader.read_exact(&mut num_entries_bytes)?;
    let num_entries = u32::from_le_bytes(num_entries_bytes);

    let mut padding_bits_buf = [0u8; 1];
    reader.read_exact(&mut padding_bits_buf)?;
    let padding_bits = padding_bits_buf[0];

    let mut encoding_table = HashMap::new();
    for _i in 0..num_entries {
        let mut buffer = [0u8;3];
        reader.read_exact(&mut buffer)?;
        let char = buffer[0];
        let bits = buffer[1];
        let length = buffer[2];
        encoding_table.insert(char, Code { bits, length });
    }
    Ok(Header { num_entries, padding_bits, encoding_table })
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

    // Returns the number of padded bits that were flushed
    fn flush(&mut self) -> IoResult<u8> {
        let padding_bits = 8 - self.bits_filled;
        if self.bits_filled > 0 {
            self.output_file.write_all(&[self.current_byte])?;
            self.current_byte = 0;
            self.bits_filled = 0;
        }
        self.output_file.sync_all()?;
        Ok(padding_bits)
    }
}

fn encode_file(
    input_file: &mut File,
    output_file: &mut File,
    encoding_table: &EncodingTable
) -> IoResult<()> {

    encode_provisionary_header(output_file, encoding_table)?;

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

    let padding_bits = bit_writer.flush()?;
    println!("Padding bits: {}", padding_bits);
    encode_header_padding_bits(output_file, padding_bits)?;
    Ok(())
}

// 'a is about the lifetime of a reference. It says 'this reference is valid for
// some scope called 'a". This is so the compiler can connect the lifetime of
// input references with the lifetime of the output reference.
//
// Every struct holding a reference needs to declare a lifetime.
// This says: "BitReader holds a reference, and BitReader cannot outlive the
// thing it references." Without it, when the reader goes out of scope it would
// result in a dangling reference in the BitReader.
//
// No lifetime is needed if the struct 'owns' the reader (no reference)
//
// Mental model: think of 'a as a contract:
// - &'a T = "I'm borrowing a T, and I promise not to use it after scope 'a ends"
struct BitReader<'a> {
    reader: &'a mut BufReader<File>,
    current_byte: Option<u8>,
    next_byte: Option<u8>,
    bit_index: u8,
    padding_bits: u8,
}

impl<'a> BitReader<'a> {
    pub fn new(reader: &'a mut BufReader<File>, padding_bits: u8) -> IoResult<Self> {
        // Eagerly load first two bytes
        let current_byte = Self::read_byte(reader)?;
        let next_byte = if current_byte.is_some() {
            Self::read_byte(reader)?
        } else {
            None
        };

        Ok(BitReader {
            reader,
            current_byte,
            next_byte,
            bit_index: 0,
            padding_bits,
        })
    }

    fn read_byte(reader: &mut BufReader<File>) -> IoResult<Option<u8>> {
        let mut buffer = [0u8; 1];
        match reader.read_exact(&mut buffer) {
            Ok(()) => Ok(Some(buffer[0])),
            Err(e) if e.kind() == std::io::ErrorKind::UnexpectedEof => Ok(None),
            Err(e) => Err(e)
        }
    }

    fn advance(&mut self) -> IoResult<()> {
        self.current_byte = self.next_byte;
        self.next_byte = if self.current_byte.is_some() {
            Self::read_byte(self.reader)?
        } else {
            None
        };
        self.bit_index = 0;
        Ok(())
    }

    pub fn read_bit(&mut self) -> IoResult<Option<bool>> {
        let byte = match self.current_byte {
            Some(byte) => byte,
            None => return Ok(None), // EOF
        };

        let is_last_byte = self.next_byte.is_none();
        let valid_bits = if is_last_byte { 8 - self.padding_bits } else { 8 };

        if self.bit_index >= valid_bits {
            return Ok(None);
        }

        let bit = (byte >> (7 - self.bit_index)) & 1 == 1;
        self.bit_index += 1;

        if self.bit_index == 8 {
            self.advance()?;
        }

        Ok(Some(bit))
    }
}

fn decode_file(reader: &mut BufReader<File>, output_file: &mut File, padding_bits: u8, encoding_table: &EncodingTable) -> IoResult<()> {
    // (bits, length) -> character
    let mut decode_table: HashMap<(u8, u8), u8> = HashMap::new();

    for (character, code) in encoding_table {
        // encoded as 0b01000000. Store as 0b00000010 for decoding
        let right_aligned_bits = code.bits >> (8 - code.length);
        decode_table.insert((right_aligned_bits, code.length), *character);
    }

    let mut bit_reader = BitReader::new(reader, padding_bits)?;

    let mut current_bits = 0u8;
    let mut current_length = 0u8;

    while let Ok(Some(bit)) = bit_reader.read_bit() {
        current_bits = (current_bits << 1) | (bit as u8);
        current_length += 1;

        if let Some(character) = decode_table.get(&(current_bits, current_length)) {
            output_file.write_all(&[*character])?;
            current_bits = 0;
            current_length = 0;
        }
    }

    Ok(())
}

fn print_encoding_table(encoding_table: &EncodingTable) {
    for (character, code) in encoding_table {
        println!("Char '{}' - encoding: {:#b}, length: {}",
            *character as char, code.bits, code.length
        );
    }
}

fn encode(opts: &Options) -> IoResult<()> {
    // Compute frequencies, huffman tree and encoding table
    let mut input_file = File::open(&opts.input_filename).expect("Failed to open file");
    let frequencies = calculate_frequencies(&input_file);
    let tree = build_huffman_tree(&frequencies);
    let encoding_table = build_encoding_table(&tree);
    print_encoding_table(&encoding_table);

    // Encode the file
    let mut output_file = File::create(&opts.output_filename)?;
    match encode_file(&mut input_file, &mut output_file, &encoding_table) {
        Ok(()) => println!("Encoding successful"),
        Err(e) => eprintln!("Encoding failed: {}", e),
    }

    Ok(())
}

fn decode(opts: &Options) -> IoResult<()> {
    // Decode Header
    let input_file = File::open(&opts.input_filename).expect("Failed to open file");
    let mut reader = BufReader::new(input_file);
    let header = decode_header(&mut reader)?;
    println!("Header - entries: {}, padding: {}", header.num_entries, header.padding_bits);
    print_encoding_table(&header.encoding_table);

    // Write decoded data to output_file
    let mut output_file = File::create(&opts.output_filename).expect("Failed to open file");
    match decode_file(&mut reader, &mut output_file, header.padding_bits, &header.encoding_table) {
        Ok(()) => println!("Decoding successful"),
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
            "decode" => { let _ = decode(&opts); },
            _ => {
                eprintln!("Error: Unknown command '{}'", opts.command);
                print_usage(&args[0]);
                exit(1);
            }
        }
    }
}
