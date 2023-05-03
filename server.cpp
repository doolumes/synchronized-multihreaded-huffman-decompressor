#include <iostream>
#include <sstream>
#include <fstream>
#include <queue>
#include <vector>
#include <string>
#include <algorithm>
#include <map>
#include <bits/stdc++.h>
#include <pthread.h>

using namespace std;

// Node structure for the Huffman tree
struct Node
{
    char symbol;   // symbol stored in the node
    int frequency; // frequency of the symbol
    Node *left;    // pointer to the left child node
    Node *right;   // pointer to the right child node

    Node(char symbol, int frequency)
    {
        this->symbol = symbol;
        this->frequency = frequency;
        left  = nullptr;
        right = nullptr;
    }
};

struct SyncData {
    pthread_mutex_t full_mutex;
    pthread_mutex_t empty_mutex;
    pthread_cond_t full;
    pthread_cond_t empty;
    bool has_data;

    vector<string> codes;
    vector<Node*> nodes;

    string code;
    vector<int> poss;
    char result[1024];
};



// Custom comparator for the priority queue to sort nodes based on frequency
struct NodeComparator
{
    bool operator()(const Node *a, const Node *b) const
    {
        if (a->frequency == b->frequency)
        {
            return a->symbol > b->symbol;
        }
        return a->frequency > b->frequency;
    }
};



// Read the alphabet information from the input file and store it in a vector of nodes
vector<Node *> readInputFile(int n)
{
    vector<Node *> nodes;
    string line;
    getline(cin, line);
    for (int i = 0; i < n; i++) {
        getline(cin, line);
        if (line[0] == ' ')
        {
            Node *node = new Node('\x20', 3);
            nodes.push_back(node);
        }
        else
        {
            std::istringstream iss(line);
            string symbol;
            int frequency;
            if (iss >> symbol >> frequency)
            {
                if (symbol.length() == 1)
                {
                    Node *node = new Node(symbol[0], frequency);
                    nodes.push_back(node);
                }
                else
                {
                    int num = stoi(symbol);
                    Node *node = new Node(num, frequency);
                    nodes.push_back(node);
                }
            }
        }
    }
    return nodes;
}

// Execute the Huffman algorithm to generate the tree
Node *generateHuffmanTree(vector<Node *> nodes)
{
    // Create a priority queue to store the nodes
    vector<Node* > pq;
    for (Node *node : nodes)
    {
        pq.push_back(node);
    }

    // Generate the Huffman tree
    while (pq.size() > 1)
    {

        Node *left = pq[pq.size() - 1];
        Node *right = pq[pq.size() - 2];
        pq.pop_back();
        pq.pop_back();

        Node *parent = new Node('\0', left->frequency + right->frequency);
        parent->left = left;
        parent->right = right;
        pq.push_back(parent);
        sort(pq.begin(), pq.end(), NodeComparator());
    }
    return pq[0];
}

// Traverse the Huffman tree to generate the codes for each symbol
void generateCodes(Node *node, string code, vector<string> &codes, vector<Node *> &nodes)
{
    if (node->symbol)
    {
        int frequency = 0;
        for (Node *nod : nodes)
        {
            if (nod->symbol == node->symbol)
            {
                frequency = nod->frequency;
                break; // stop searching once you find the character
            }
        }
        codes[node->symbol] = code;
        // cout << "Symbol: " << node->symbol << ", Frequency :" << frequency << ", Code: " << code << endl;
        return;
    }
    generateCodes(node->left, code + "0", codes, nodes);
    generateCodes(node->right, code + "1", codes, nodes);
}

// Print the codes for each symbol
void printCodes(vector<string> &codes, vector<Node *> &nodes)
{
    for (int i = 0; i < (int)codes.size(); i++)
    {
        if (codes[i] != "")
        {
            int frequency = 0;
            for (Node *node : nodes)
            {
                if (node->symbol == (char)i)
                {
                    frequency = node->frequency;
                    break; // stop searching once you find the character
                }
            }
            cout << "Symbol:" << (char)i << ", Frequency:" << frequency << ", Code:" << codes[i] << endl;
        }
    }
}

// Thread function to decompress a symbol from the compressed file
void *decompressSymbol(void *arg)
{
    SyncData *sync = (SyncData *)arg;

    // copy data
    string code;
    vector<int> poss;


    pthread_mutex_lock(&sync->full_mutex);
    // wait data
    while (sync->has_data == false) {
        pthread_cond_wait(&sync->full, &sync->full_mutex);
    }
    code = sync->code;
    poss = sync->poss;
    sync->has_data = false;
    pthread_mutex_unlock(&sync->full_mutex);


    char symbol = 0;
    for (int i = 0; i < (int)sync->codes.size(); i++) {
        if (sync->codes[i] == code) {
            symbol = (char)i;
            break;
        }
    }
    for (int i = 0; i < (int)poss.size(); i++)
    {
        sync->result[poss[i]] = symbol;
    }

    // print
    int frequency = 0;
    for (Node *node : sync->nodes)
    {
        if (node->symbol == symbol)
        {
            frequency = node->frequency;
            break; // stop searching once you find the character
        }
    }
    cout << "Symbol:" << symbol << ", Frequency:" << frequency << ", Code:" << code << endl;


    // signal main thread after print done (we must do it to ensure the order)
    pthread_cond_signal(&sync->empty);

    return NULL;
}

int main()
{
    int n;
    cin >> n;
    vector<Node *> nodes = readInputFile(n);
    sort(nodes.begin(), nodes.end(), NodeComparator());
    Node *root = generateHuffmanTree(nodes);
    vector<string> codes(256, "");
    generateCodes(root, "", codes, nodes);
    //printCodes(codes, nodes);

    SyncData sync;
    pthread_mutex_init(&sync.full_mutex, NULL);
    pthread_mutex_init(&sync.empty_mutex, NULL);
    pthread_cond_init(&sync.full, NULL);
    pthread_cond_init(&sync.empty, NULL);
    sync.has_data = false;
    memset(sync.result, 0, sizeof(sync.result));
    sync.codes = codes;
    sync.nodes = nodes;

    // create n thread
    vector<pthread_t> ids(n);
    for (int i = 0; i < n; i++) {
        pthread_create(&ids[i], NULL, decompressSymbol, &sync);
    }

    string line;
    int pos;
    for (int i = 0; i < n; i++) {
        string code;
        vector<int> poss;

        getline(cin, line);
        istringstream iss(line);
        iss >> code;
        while (iss >> pos) {
            poss.push_back(pos);
        }


        // put the data into SyncData, need get the mutex
        pthread_mutex_lock(&sync.empty_mutex);
        while (sync.has_data) {
            pthread_cond_wait(&sync.empty, &sync.empty_mutex);
        }
        sync.code = code;
        sync.poss = poss;
        sync.has_data = true;
        pthread_mutex_unlock(&sync.empty_mutex);

        // signal child thread
        pthread_cond_signal(&sync.full);
    }


    for (int i = 0; i < n; i++) {
        pthread_join(ids[i], NULL);
    }

    cout << "Original message: " << sync.result << endl;

    return 0;
}
