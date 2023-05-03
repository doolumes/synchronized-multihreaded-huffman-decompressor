#include <iostream>
#include <fstream>

#include <vector>
#include <algorithm>
#include<iterator>

#include <string>
#include <sstream>

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <cmath>
#include <stack>
#include <map>
#include <utility>
#include <functional>
#include <unordered_map>

//information of each line of input file will be stored in a Huffman node
struct HuffmanNode {
    char c;
    int freq;
    std::string code;
    HuffmanNode* left;
    HuffmanNode* right ;

    HuffmanNode(int freq, char c = '\0'){

        this->c = c;
        this->freq = freq;
        this->left = nullptr;
        this->right = nullptr;
    }

    //returns code based on position of node on Huffman Tree
    char decode(std::string str){
        if (str == "")
            return c;
        else{
            char temp  = str.at(0);
            if (temp == '0')
                return this->left->decode(str.substr(1));
            return this->right->decode(str.substr(1));
        }
    }

    HuffmanNode *find_node(HuffmanNode* root, char c){
    if(root != nullptr){
        if(root->c == c){
           return root;
        } else {
            HuffmanNode *foundNode = find_node(root->left, c);
            if(foundNode == nullptr) {
                foundNode = find_node(root->right, c);
            }
            return foundNode;
         }
    } else {
        return nullptr;
    }
}
};

//args for pthread_create will be a pointer to this struct **BECOMES SHARED RESOURCE FOR PA3**
struct decompress_info{
    HuffmanNode* root;
    std::string str;
    std::vector<int> positions;
    char* output;
    int threadNumber = 0;

    pthread_mutex_t* print_lock;
    pthread_mutex_t* copy_lock;
    pthread_cond_t* waitTurn;
    int* turn;
};

//comparator for strong ordering of nodes contained in vector
struct HuffmanNodeComparison{
   bool operator()( const HuffmanNode* a, const HuffmanNode* b ) const{
        if( a->freq != b->freq)
            return ( a->freq > b->freq);
        return ( a->c > b->c);
   }
};

//args function for pthread_create; adds decoded characters to final output array based on their positions
void *code_to_string(void *decompress_info_void_ptr){
    decompress_info* decompress_info_ptr = (decompress_info*) decompress_info_void_ptr;

    //Critical section #1 is using the copy_lock mutex to allow for us to copy the void* args into local variables and signal other threads
    pthread_mutex_lock(decompress_info_ptr->copy_lock);
        std::string str = decompress_info_ptr->str;
        std::vector<int> positions = decompress_info_ptr->positions;
        int threadNumber = decompress_info_ptr->threadNumber;
        char decoded_char = decompress_info_ptr->root->decode(str);
        int freq = decompress_info_ptr->root->find_node(decompress_info_ptr->root, decoded_char)->freq;
        std::string code = decompress_info_ptr->root->find_node(decompress_info_ptr->root, decoded_char)->code;

        //after we are done copying thread args into local variables, we wake up the next thread to continue
        pthread_cond_signal(decompress_info_ptr->waitTurn);
    pthread_mutex_unlock(decompress_info_ptr->copy_lock);

    //copy characters into output array
    for(int i = 0; i  < positions.size(); i++){
        *(decompress_info_ptr->output + positions[i]) = decoded_char;
    }

    //Critical section #2 using to sleep other threads using print_lock mutex so we can print tree info in thread order
    pthread_mutex_lock(decompress_info_ptr->print_lock);
        while (*decompress_info_ptr->turn != threadNumber){
            pthread_cond_wait(decompress_info_ptr->waitTurn, decompress_info_ptr->print_lock);
        }
    pthread_mutex_unlock(decompress_info_ptr->print_lock);

    std::cout << "Symbol: " << decoded_char << ", Frequency: " << freq<< ", Code: " << code << std::endl;

    //Critical section #3 using print_lock used to increment turn variable and signal other threads so they can accesses critical sections
    pthread_mutex_lock(decompress_info_ptr->print_lock);
        *(decompress_info_ptr->turn)+=1;
        pthread_cond_signal(decompress_info_ptr->waitTurn);
    pthread_mutex_unlock(decompress_info_ptr->print_lock);
    return nullptr;
}

//converts the output array to a string that can be printed
std::string convertToString(char* a, int size){
    int i;
    std::string s = "";

    for (i = 0; i < size; i++)
        s = s + a[i];
    return s;
}

//uses algorithm to build Huffman tree based on the nodes in the vector passed in parameter list
int build_huffman_tree(std::vector<HuffmanNode*> &huffmanNodeVector, std::vector<std::pair<char, std::pair<int, std::string>>> &myVector){
    char c;
    int freq;
    int num;
    //name of input file is read from STDIN
    std::cin >> num;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    int counter = 0;
    //alphabet information is read of the input file
    while(counter < num){
        std::string line;
        getline(std::cin, line);
        c = line.at(0);
        freq = stoi(line.substr(2));
        huffmanNodeVector.push_back(new HuffmanNode(freq ,c));
        counter++;
        myVector.push_back(make_pair(c, std::make_pair(freq, "")));
    }

    sort(huffmanNodeVector.begin(), huffmanNodeVector.end(), HuffmanNodeComparison());

    //Huffman tree is generated based on the vector of nodes
    while(huffmanNodeVector.size() > 1){

        HuffmanNode* left = huffmanNodeVector[huffmanNodeVector.size()-1];
        HuffmanNode* right = huffmanNodeVector[huffmanNodeVector.size()-2];

        huffmanNodeVector.pop_back();
        huffmanNodeVector.pop_back();

        HuffmanNode* root = new HuffmanNode(left->freq + right->freq);

        root->left = left;
        root->right = right;

        huffmanNodeVector.push_back(root);

        sort(huffmanNodeVector.begin(), huffmanNodeVector.end(), HuffmanNodeComparison());
    }
    return num;
}

//creates the codes for each node in Huffman tree
void create_huffman_code(HuffmanNode* root,  std::vector<std::pair<char, std::pair<int, std::string>>> &myVector, std::string code){
    if(root == nullptr)
        return;
    if(root->left == nullptr && root->right == nullptr){
        root->code = code;
        for(int i = 0; i < myVector.size(); i++){
            if(myVector[i].first == root->c){
                myVector[i].second.second = code;
            }
        }

    }

    if(root->left)
        create_huffman_code(root->left, myVector, code + '0');
    if(root->right)
        create_huffman_code(root->right, myVector, code + '1');
}

int height(HuffmanNode* root)
{
    if (root == nullptr)
        return 0;
    return 1 + std::max(height(root->left), height(root->right));
}

void print_huffman_level(HuffmanNode* root, int level){
    if(root == nullptr)
        return;
    if (level == 1 && root->c)
        std::cout << "Symbol: " << root->c << ", Frequency: " << root->freq << ", Code: " << root->code << std::endl;
    else if (level > 1){
        print_huffman_level(root->left, level-1);
        print_huffman_level(root->right, level-1);
    }
}

//prints nodes of Huffman tree in level order traversal
void print_huffman_tree(std::vector<std::pair<char, std::pair<int, std::string>>> myVector){
    for(auto it = myVector.begin(); it != myVector.end(); it++){
        std::cout << "Symbol: " << it->first << ", Frequency: " << it->second.first<< ", Code: " << it->second.second<< std::endl;
    }
}

//compressed file is decoded using multi-threaded approach
void decompress_huffman_code(HuffmanNode* huffmanNode, int numLines){
    char *output = new char[huffmanNode->freq];
    static std::vector<pthread_t> tid;
    decompress_info temp;

    static pthread_mutex_t print_lock;
    static pthread_mutex_t copy_lock;
    static pthread_cond_t waitTurn;
    static int turn;
    pthread_mutex_init(&print_lock, NULL);
    pthread_mutex_init(&copy_lock, NULL);
    pthread_cond_init(&waitTurn, NULL);
    temp.print_lock = &print_lock;
    temp.copy_lock = &copy_lock;
    temp.waitTurn = &waitTurn;
    temp.turn = &turn;

    //information is read from the compressed file
    for(int i = 0; i < numLines; i++){
        std::string line;
        getline(std::cin, line);
        std::istringstream ss(line);
        std::string code;
        std::string buffer;
        std::vector<int> positions;
        bool getCode = true;

        while(ss >> buffer){
            if(getCode){
                code = buffer;
                getCode = false;
            }
            else{
                positions.push_back(stoi(buffer));
            }
        }

        //n POSIX threads are created (n is the number of lines in the compressed file)
        pthread_t thread;
        tid.push_back(thread);
        /*
        each thread receives a pointer to a struct that contains the following:
        struct decompress_info{
            HuffmanNode* root;
            std::string str;
            std::vector<int> positions;
            char* output;
            int threadNumber = 0;

            pthread_mutex_t* print_lock;
            pthread_mutex_t* copy_lock;
            pthread_cond_t* waitTurn;
            int* turn;
        }
        (1) a pointer to the root node of the Huffman tree
        (2) the binary code
        (3) a vector of positions in which the code should be placed in the output array
        (4) a pointer to the output array that will be mutated by each thread
        (5) thread number assinged to thread in main function
        (6) mutex for controlling crtical sections
        (7) mutex for copying thread values from args into local variables
        (8) condition variable that uses the copy mutex to synchronize threads
        (9) turn of current thread to access critical sections
        */
        temp.root = huffmanNode;
        temp.str = code;
        temp.positions = positions;
        temp.output = output;
        temp.threadNumber = i;
        if(pthread_create(&tid[i], NULL, code_to_string, &temp)){
            fprintf(stderr, "Error creating thread\n");
            return;
        }
        //sleeping each thread right after creation in order to avoid a race condition
        pthread_cond_wait(&waitTurn, &copy_lock);
    }

    //joining threads together after their processes are finished
    for(int j = 0; j < tid.size(); j++)
        pthread_join(tid[j], NULL);
    pthread_mutex_destroy(&print_lock);
    pthread_mutex_destroy(&copy_lock);
    /*After threads mutate the output array,
    and are joined, the original message is printed after*/
    std::cout << "Original message: " << convertToString(output, huffmanNode->freq) << std::endl;
    delete[] output;
    //delete temp;
}

void delete_huffman_tree(HuffmanNode* root){
     if (root == nullptr) {
        return; // if the root is null, return immediately
    }
    // recursively delete the left and right subtrees
    delete_huffman_tree(root->left);
    delete_huffman_tree(root->right);
    // delete the current node
    delete root;
}

int main(int argc/*1 arguments from command line*/, char**argv/*file name*/){
    if(argc < 1){
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
    }
    std::vector<HuffmanNode*> huffmanNodeVector;
    std::vector<std::pair<char, std::pair<int, std::string>>> myVector;
    //1. Generate Huffman tree using a sorted vector as a priority queue; returns number of lines in input redirection
    int numLines = build_huffman_tree(huffmanNodeVector, myVector);
    //2. Huffman codes are created from the generated tree
    create_huffman_code(huffmanNodeVector[0], myVector, "");
    //3. compressed file is decompressed using the codes of each character in the tree; implemented with POSIX threads
    decompress_huffman_code(huffmanNodeVector[0], numLines);
    //4. Delete memory allocated by all nodes in huffman tree
    delete_huffman_tree(huffmanNodeVector[0]);

    return 0;
}
