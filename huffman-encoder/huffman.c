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

////////////////////////////////////////////////////////////////////////////////
// Encoding File

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

void encode_file(HuffmanCode encoding_table[ENCODING_TABLE_SIZE], FILE *input_file, FILE *output_file) {
    BitWriter writer = { 0, 0, output_file };

    printf("Encoding File...\n");
    int c;
    while ((c = fgetc(input_file)) != EOF) {
        HuffmanCode huffman_code = encoding_table[c];

        printf("Writing: %c (%s)\n", c, huffman_code.code);
        write_code(&writer, huffman_code.code);
        // printf("Char: %c, Code: %s\n", c, huffman_code.code);
        // printf("%lu", sizeof(huffman_code.code));
    }
    flush_bits(&writer);
}

// Processing File
////////////////////////////////////////////////////////////////////////////////

bool process_file(const char *input_filename, const char *output_filename) {
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

    printf("Code for 'e': %s (length: %d)\n", encoding_table['e'].code, encoding_table['e'].exists);

    // Encode File
    FILE *input_file = fopen(input_filename, "r");
    FILE *output_file = fopen(output_filename, "w");
    encode_file(encoding_table, input_file, output_file);
    fclose(input_file);
    fclose(output_file);

    return true;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }
    char* filename = argv[1];
    char output_filename[256];
    sprintf(output_filename, "%s.encoded", filename);
    process_file(filename, output_filename);

    return 0;
}
