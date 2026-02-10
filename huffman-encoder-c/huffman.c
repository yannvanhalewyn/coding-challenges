#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define true 1
#define false 0
#define bool int

// Technically this could overflow but it's very unlikely with typical data.
#define ENCODING_TABLE_SIZE 256

// Hufman Trees
////////////////////////////////////////////////////////////////////////////////

typedef struct HuffmanNode {
    int weight;
    char character;
    struct HuffmanNode *left;
    struct HuffmanNode *right;
} HuffmanNode;

HuffmanNode* create_node(unsigned char character, unsigned int weight) {
    HuffmanNode *node = (HuffmanNode*)malloc(sizeof(HuffmanNode));
    if (node == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }

    node->character = character;
    node->weight = weight;
    node->left = NULL;
    node->right = NULL;
    return node;
}

HuffmanNode* create_parent_node(HuffmanNode* left, HuffmanNode* right) {
    HuffmanNode *node = (HuffmanNode*)malloc(sizeof(HuffmanNode));
    node->left = left;
    node->right = right;
    node->weight = left->weight + right->weight;
    return node;
}

bool is_leaf(HuffmanNode* node) {
    return node->left == NULL && node->right == NULL;
}

// Using Bubblesort would theoretically be more efficient
// Practically it's usually a relatively small number of nodes and this part is
// no bottleneck.
void sort_nodes(HuffmanNode* nodes[], int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (nodes[j]->weight > nodes[j+1]->weight) {
                // Swap pointers
                HuffmanNode* temp = nodes[j];
                nodes[j] = nodes[j+1];
                nodes[j+ 1] = temp;
            }
        }
    }
}

// Builds a huffman tree, which is a tree whose leaves are the letters and the
// edges are 0 or 1, declaring a path to encoding each character
HuffmanNode* build_huffman_tree(unsigned int freq[], size_t freq_size) {
    int node_count = 0;
    for (size_t i = 0; i < freq_size; i++) {
        if (freq[i] > 0) {
            node_count++;
        }
    }

    HuffmanNode* nodes[node_count];
    int idx = 0;
    for (size_t i = 0; i < freq_size; i++) {
        if (freq[i] > 0) {
            nodes[idx++] = create_node(i, freq[i]);
        }
    }

    while (node_count > 1) {
        sort_nodes(nodes, node_count);

        HuffmanNode *left = nodes[0];
        HuffmanNode *right = nodes[1];
        HuffmanNode *parent = create_parent_node(left, right);
        nodes[0] = parent;
        for (int i = 1; i < node_count -1; i++) {
            nodes[i] = nodes[i+1];
        }
        node_count--;
    }

    return nodes[0];
}

void free_huffman_tree(HuffmanNode* node) {
    if (node != NULL) {
        free_huffman_tree(node->left);
        free_huffman_tree(node->right);
        free(node);
    }
}

// Prefix table
////////////////////////////////////////////////////////////////////////////////

typedef struct {
    char code[ENCODING_TABLE_SIZE];
    int length;
    int exists;
} HuffmanCode;

// Builds a prefix table, going from char -> HuffmanCode
void build_encoding_table(
        HuffmanNode *node,
        char *code, int depth,
        HuffmanCode encoding_table[ENCODING_TABLE_SIZE]
        ) {
    if (node->left == NULL && node->right == NULL) {
        // Null terminate current code str
        code[depth] = '\0';
        // Copy current code str to prefix table entry
        HuffmanCode *entry = &encoding_table[(unsigned char)node->character];
        strcpy(entry->code, code);
        entry->exists = 1;
        return;
    }
    // Traverse left
    if (node->left != NULL) {
        code[depth] = '0';
        build_encoding_table(node->left, code, depth + 1, encoding_table);
    }

    if (node->right != NULL) {
        code[depth] = '1';
        build_encoding_table(node->right, code, depth + 1, encoding_table);
    }
}

// Encoding File Header
////////////////////////////////////////////////////////////////////////////////

// "HUFF" in ASCII
#define HUFF 0x48554646

typedef struct {
    unsigned int num_unique_chars;
    unsigned int padding_bits;
    unsigned int frequencies[ENCODING_TABLE_SIZE];
} FileHeader;

// Writes value in little endian mode (least significant bit first)
// So all bytes can be read in order from disk
void write_uint8(FILE *file, unsigned int value) {
    // 0xFF is 8 '1'-bits. Each F is 1111.
    // Example with value = 0x1234. value & 0xFF -> 0x34
    //
    //   value: 0001 0010 0011 0100  (16 bits)
    //   0xFF:  0000 0000 1111 1111  (8 bits of 1's, padded to match)
    //
    // And so effectively writes the lowest byte.
    fputc(value & 0xFF, file);
}

void write_uint32(FILE *file, unsigned int value) {
    // First write upper byte, then the second byte, third and then the lowest byte
    fputc((value >> 24) & 0xFF, file);
    fputc((value >> 16) & 0xFF, file);
    fputc((value >> 8) & 0xFF, file);
    fputc(value & 0xFF, file);
}

unsigned int read_uint32(FILE *file) {
    unsigned int value = 0;
    value |= (fgetc(file) << 24);
    value |= (fgetc(file) << 16);
    value |= (fgetc(file) << 8);
    value |= fgetc(file);
    return value;
}

void write_provisionary_header(FILE *output_file, unsigned int freq[], size_t freq_size) {
    // Write magic number "HUFF" header, used to recognise the format when
    // decoding
    write_uint32(output_file, HUFF);

    // Calculate and write number of chars stored in freq table
    unsigned int num_unique = 0;
    for (int i = 0; i < freq_size; i++) {
        if (freq[i] > 0) {
            num_unique++;
        }
    }
    write_uint32(output_file, num_unique);

    // Placeholder for for padding count to be written later
    write_uint8(output_file, 0);

    // Write all the found entries in the frequencies table
    for (int i = 0; i < ENCODING_TABLE_SIZE; i++) {
        if (freq[i] > 0) {
            fputc((unsigned char)i, output_file); // 1 byte for character
            write_uint32(output_file, freq[i]);   // 4 bytes of frequency
        }
    }
}

void write_padding_to_header(FILE *output_file, int padding) {
    fseek(output_file, 8, SEEK_SET);
    write_uint8(output_file, padding);
}


FileHeader* read_header(FILE *input_file, unsigned int frequenties[], size_t freq_size) {
    unsigned int encoding_type = read_uint32(input_file);
    if (encoding_type != HUFF) {
        return NULL;
    }

    unsigned int num_unique = read_uint32(input_file);
    memset(frequenties, 0, freq_size * sizeof(unsigned int));

    int padding_bits = fgetc(input_file);

    for (unsigned int i = 0; i < num_unique; i++) {
        unsigned char character = fgetc(input_file);
        frequenties[character] = read_uint32(input_file);
    }

    FileHeader *header = malloc(sizeof(FileHeader));
    if (header == NULL) {
        return NULL;
    }
    header->num_unique_chars = num_unique;
    header->padding_bits = padding_bits;
    memcpy(header->frequencies, frequenties, freq_size * sizeof(unsigned int));
    return header;
}

// Encoding and decoding file body
////////////////////////////////////////////////////////////////////////////////

typedef struct {
    unsigned char current_byte;
    int bits_filled;
    FILE *output;
    int total_bits_written;
} BitWriter;

void write_bit(BitWriter *writer, int bit) {
    if (bit) {
        // Shift 1 to the correct position and OR it in
        // When bits_filled is 0, sets the leftmost bit to 1
        // When bits_filled is 7, sets the rightmost bit to 1
        writer->current_byte |= (1 << (7 - writer->bits_filled));
    }

    writer->bits_filled++;
    writer->total_bits_written++;

    if (writer->bits_filled == 8) {
        fputc(writer->current_byte, writer->output);
        writer->current_byte = 0;
        writer->bits_filled = 0;
    }
}

int flush_bits(BitWriter *writer) {
    int padding = 0;
    if (writer->bits_filled > 0) {
        padding = 8 - writer->bits_filled;
        fputc(writer->current_byte, writer->output);
        writer->current_byte = 0;
        writer->bits_filled = 0;
    }
    return padding;
}

void write_code(BitWriter *writer, const char *code_string) {
    for (int i = 0; code_string[i] != '\0'; i++) {
        write_bit(writer, code_string[i] == '1');
    }
}

int encode_file(HuffmanCode encoding_table[ENCODING_TABLE_SIZE], unsigned int freq[], size_t freq_size, FILE *input_file, FILE *output_file) {
    printf("Encoding File...\n");
    write_provisionary_header(output_file, freq, freq_size);

    BitWriter writer = { 0, 0, output_file };
    int c;
    while ((c = fgetc(input_file)) != EOF) {
        HuffmanCode huffman_code = encoding_table[c];
        write_code(&writer, huffman_code.code);
    }
    int padding = flush_bits(&writer);
    write_padding_to_header(output_file, padding);
    return padding;
}

typedef struct {
    unsigned char *data;
    size_t size;
    size_t byte_index;
    int bit_index;
} BitReader;

int read_bit(BitReader *reader) {
    if (reader->byte_index >= reader->size) {
        return -1; // EOF
    }

    unsigned char current_byte = reader->data[reader->byte_index];
    int bit = (current_byte >> (7 - reader->bit_index)) & 1;

    reader->bit_index++;

    if (reader->bit_index == 8) {
        reader->bit_index = 0;
        reader->byte_index++;
    }

    return bit;
}

void decode_file(FileHeader *header, HuffmanNode *tree, FILE *input_file, FILE *output_file) {
    long cur_pos = ftell(input_file);
    fseek(input_file, 0, SEEK_END);
    long end_pos = ftell(input_file);

    // Read file into memory
    fseek(input_file, cur_pos, SEEK_SET);
    size_t data_size = end_pos - cur_pos;

    unsigned char *data = malloc(data_size);
    if (data == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }

    fread(data, 1, data_size, input_file);

    size_t total_bits = data_size * 8 - header->padding_bits;

    // Read all bits into memory
    printf("Decoding %zu characters from %zu bytes (%zu bits, %u padding)...\n",
            total_bits / 8, data_size, total_bits, header->padding_bits
            );

    BitReader reader = { data, data_size, 0, 0 };
    HuffmanNode *current = tree;
    size_t chars_decoded = 0;
    size_t bits_read = 0;

    // Read every bit and write decoded character to output file
    while (bits_read < total_bits) {
        int bit = read_bit(&reader);
        if (bit == -1) {
            break;
        }
        bits_read++;

        current = (bit == 0) ? current->left : current->right;

        if (is_leaf(current)) {
            fputc(current->character, output_file);
            chars_decoded++;
            current = tree; // Reset to root
        }
    }

    free(data);
    printf("Decoded %zu characters\n", chars_decoded);
}

// Processing File
////////////////////////////////////////////////////////////////////////////////

bool encode(const char *input_filename, const char *output_filename) {
    // Computing Hufman Trees and prefix tables
    FILE *file = fopen(input_filename, "r");

    if (file == NULL) {
        fprintf(stderr, "Error: Could not open file '%s'\n", input_filename);
        return false;
    }

    printf("Processing %s\n", input_filename);

    unsigned int freq[ENCODING_TABLE_SIZE] = {0};

    int c;
    while ((c = fgetc(file)) != EOF) {
        freq[c]++;
    }
    fclose(file);

    // Build Hoffman Tree
    HuffmanNode* tree = build_huffman_tree(freq, ENCODING_TABLE_SIZE);

    // Build Encoding Table
    HuffmanCode encoding_table[ENCODING_TABLE_SIZE] = {0};
    char code_buffer[ENCODING_TABLE_SIZE];
    build_encoding_table(tree, code_buffer, 0, encoding_table);
    free_huffman_tree(tree);

    // Preview Encoding Table
    printf("Code for 'e': %s\n", encoding_table['e'].code);
    printf("Code for 'a': %s\n", encoding_table['a'].code);
    printf("Code for 't': %s\n", encoding_table['t'].code);
    printf("Code for 'h': %s\n", encoding_table['h'].code);
    printf("Code for 'q': %s\n", encoding_table['q'].code);

    // Encode File
    FILE *input_file = fopen(input_filename, "r");
    FILE *output_file = fopen(output_filename, "wb");
    encode_file(encoding_table, freq, ENCODING_TABLE_SIZE, input_file, output_file);
    fclose(input_file);
    fclose(output_file);

    return true;
}

void decode(const char *input_filename, const char *output_filename) {
    FILE *input_file = fopen(input_filename, "r");
    if (input_file == NULL) {
        fprintf(stderr, "Error: Unknown file %s\n", input_filename);
        exit(1);
    }

    // Read file header
    unsigned int frequencies[ENCODING_TABLE_SIZE];
    FileHeader *header = read_header(input_file, frequencies, ENCODING_TABLE_SIZE);
    if (header == NULL) {
        fprintf(stderr, "Error: Invalid file format\n");
        exit(1);
    }

    // Build Hoffman Tree
    HuffmanNode* tree = build_huffman_tree(header->frequencies, ENCODING_TABLE_SIZE);

    // Decode contents of file
    FILE *output_file = fopen(output_filename, "w");
    decode_file(header, tree, input_file, output_file);
}

// Options parsing
////////////////////////////////////////////////////////////////////////////////

typedef struct {
    char *command;
    char *input_filename;
    char *output_filename;
} Options;

Options parse_options(int argc, char *argv[]) {
    char *command = NULL;
    char *input_filename = NULL;
    char *output_filename = NULL;

    command = argv[1];
    input_filename = argv[2];

    // Look for -o flag
    for (int i = 3; i < argc - 1; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            output_filename = argv[i + 1];
            break;
        }
    }

    if (output_filename == NULL) {
        fprintf(stderr, "No output option provided. Usage: %s <command> <input_file> -o <output_file>", argv[0]);
        exit(1);
    }

    Options options = { command, input_filename, output_filename };
    return options;
}

void print_usage(const char *program_name) {
    printf("Usage: %s <command> <input_file> [options]\n", program_name);
    printf("\nCommands:\n");
    printf("  encode    Encode a file using Huffman compression\n");
    printf("  decode    Decode a Huffman-encoded file\n");
    printf("\nOptions:\n");
    printf("  -o, --output FILE    Output file (default: <input>.encoded/.decoded)\n");
    printf("  -h, --help           Show this help message\n");
    printf("  -v, --verbose        Verbose output\n");
    printf("\nExamples:\n");
    printf("  %s encode test.txt\n", program_name);
    printf("  %s encode test.txt -o compressed.huf\n", program_name);
    printf("  %s decode test.txt.encoded -o restored.txt\n", program_name);
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        print_usage(argv[0]);
        return 0;
    } else if (argc < 3) {
        fprintf(stderr, "Wrong number of arguments");
        print_usage(argv[0]);
        return -1;
    }

    Options opts = parse_options(argc, argv);

    if (strcmp(opts.command, "encode") == 0) {
        encode(opts.input_filename, opts.output_filename);
    } else if (strcmp(opts.command, "decode") == 0) {
        decode(opts.input_filename, opts.output_filename);
    } else {
        fprintf(stderr, "Error: unknown command %s", opts.command);
        print_usage(argv[0]);
        return -1;
    }
    return 0;
}
