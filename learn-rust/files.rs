use std::io::{
    BufReader, Read, Result as IoResult, Error, ErrorKind,
    Seek, SeekFrom, Write
};

fn encode_something(file: &mut File) -> IoResult<()> {
    file.write_all(b"HUFF")?;
    // Write unique number of chars in frequency table
    let x = "hello";
    file.write_all(x.to_le_bytes())?;
    // Write 1 placeholder byte for the padding to be written later
    file.write_all(&0u8.to_le_bytes())?;
    Ok(())
}

fn example() -> IoResult<()> {
    let mut input_file = File::open(&opts.input_filename).expect("Failed to open file");
    encode_something(&mut file)?;
    Ok(())
}
