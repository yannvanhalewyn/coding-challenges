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
        printf("Character '%c' (byte %d): code = %s\n",
                node->character, (unsigned char)node->character, code);
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

// Processing File
////////////////////////////////////////////////////////////////////////////////

bool process_file(const char *filename) {
    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        fprintf(stderr, "Error: Could not open file '%s'\n", filename);
        return false;
    }

    printf("Processing %s\n", filename);

    unsigned int freq[ENCODING_TABLE_SIZE] = {0};

    int c;
    while ((c = fgetc(file)) != EOF) {
        freq[c]++;
    }
    fclose(file);

    HuffmanNode* tree = build_huffman_tree(freq, ENCODING_TABLE_SIZE);
    HuffmanCode encoding_table[ENCODING_TABLE_SIZE] = {0};
    char code_buffer[ENCODING_TABLE_SIZE];
    build_encoding_table(tree, code_buffer, 0, encoding_table);

    printf("Code for 'l': %s (length: %d)\n", encoding_table['l'].code, encoding_table['l'].exists);

    return true;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }
    char* filename = argv[1];
    process_file(filename);

    return 0;
}
