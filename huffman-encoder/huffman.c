#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define true 1
#define false 0
#define bool int

#define ENCODING_TABLE_SIZE 256

// Hufman Trees
////////////////////////////////////////////////////////////////////////////////

struct FreqEntry {
    char character;
    int count;
};

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

void sort_nodes(HuffmanNode* nodes[], int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (nodes[j] == NULL || nodes[j]->weight > nodes[j+1]->weight) {
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

// Writing Frequenties Header
typedef struct {
    size_t size;
    int num_unique_chars;
} FileHeader;

typedef struct {
    unsigned char character;
    unsigned int frequency;
} FrequencyEntry;

void write_uint32(FILE *file, unsigned int value) {
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

void write_header(FILE *output_file, unsigned int freq[], size_t freq_size) {
    write_uint32(output_file, HUFF);
    unsigned int num_unique = 0;
    for (int i = 0; i < freq_size; i++) {
        if (freq[i] > 0) {
            num_unique++;
        }
    }

    write_uint32(output_file, num_unique);

    for (int i = 0; i < ENCODING_TABLE_SIZE; i++) {
        if (freq[i] > 0) {
            fputc((unsigned char)i, output_file); // 1 byte for character
            write_uint32(output_file, freq[i]);   // 4 bytes of frequency
        }
    }
}

bool read_header(FILE *input_file, unsigned int freq[], size_t freq_size) {
    unsigned int encoding_type = read_uint32(input_file);
    if (encoding_type != HUFF) {
        return false;
    }

    unsigned int num_unique = read_uint32(input_file);
    memset(freq, 0, freq_size * sizeof(unsigned int));

    for (unsigned int i = 0; i < num_unique; i++) {
        unsigned char character = fgetc(input_file);
        freq[character] = read_uint32(input_file);
    }

    return true;
}

// Encoding file body
////////////////////////////////////////////////////////////////////////////////

typedef struct {
    unsigned char current_byte;
    int bits_filled;
    FILE *output;
} BitWriter;

void write_bit(BitWriter *writer, int bit) {
    if (bit) {
        // Shift 1 to the correct position and OR it in
        // When bits_filled is 0, sets the leftmost bit to 1
        // When bits_filled is 7, sets the rightmost bit to 1
        writer->current_byte |= (1 << (7 - writer->bits_filled));
    }

    writer->bits_filled++;

    if (writer->bits_filled == 8) {
        fputc(writer->current_byte, writer->output);
        writer->current_byte = 0;
        writer->bits_filled = 0;
    }
}

void flush_bits(BitWriter *writer) {
    if (writer->bits_filled > 0) {
        fputc(writer->current_byte, writer->output);
        writer->current_byte = 0;
        writer->bits_filled = 0;
    }
}

void write_code(BitWriter *writer, const char *code_string) {
    for (int i = 0; code_string[i] != '\0'; i++) {
        write_bit(writer, code_string[i] == '1');
    }
}

void encode_file(HuffmanCode encoding_table[ENCODING_TABLE_SIZE], unsigned int freq[], size_t freq_size, FILE *input_file, FILE *output_file) {
    printf("Encoding File...\n");
    write_header(output_file, freq, freq_size);

    BitWriter writer = { 0, 0, output_file };
    int c;
    while ((c = fgetc(input_file)) != EOF) {
        HuffmanCode huffman_code = encoding_table[c];
        write_code(&writer, huffman_code.code);
    }
    flush_bits(&writer);
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

    // Preview Encoding Table
    printf("Code for 'e': %s\n", encoding_table['e'].code);
    printf("Code for 'a': %s\n", encoding_table['a'].code);
    printf("Code for 't': %s\n", encoding_table['t'].code);
    printf("Code for 'h': %s\n", encoding_table['h'].code);
    printf("Code for 'q': %s\n", encoding_table['q'].code);

    // Encode File
    FILE *input_file = fopen(input_filename, "r");
    FILE *output_file = fopen(output_filename, "w");
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

    unsigned int freq[ENCODING_TABLE_SIZE];
    bool success = read_header(input_file, freq, ENCODING_TABLE_SIZE);
    if (!success) {
        fprintf(stderr, "Error: Invalid file format\n");
        exit(1);
    }

    // Build Hoffman Tree
    HuffmanNode* tree = build_huffman_tree(freq, ENCODING_TABLE_SIZE);

    // Build Encoding Table
    HuffmanCode encoding_table[ENCODING_TABLE_SIZE] = {0};
    char code_buffer[ENCODING_TABLE_SIZE];
    build_encoding_table(tree, code_buffer, 0, encoding_table);

    // Decode contents of file
    FILE *output_file = fopen(output_filename, "w");

    BitWriter writer = { 0, 0, output_file };
    int c;
    while ((c = fgetc(input_file)) != EOF) {
        HuffmanCode huffman_code = encoding_table[c];
        write_code(&writer, huffman_code.code);
    }
    flush_bits(&writer);
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

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <command> <input_file> -o <output_file>", argv[0]);
        exit(1);
    }

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
    Options opts = parse_options(argc, argv);
    if (strcmp(opts.command, "encode") == 0) {
        encode(opts.input_filename, opts.output_filename);
    } else if (strcmp(opts.command, "decode") == 0) {
        decode(opts.input_filename, opts.output_filename);
    } else {
        fprintf(stderr, "Error: unknown command %s", opts.command);
    }

    return 0;
}
