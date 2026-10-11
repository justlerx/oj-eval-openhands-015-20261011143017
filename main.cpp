#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <set>

using namespace std;

const string DATA_FILE = "database.dat";
const string INDEX_FILE = "index.dat";
const int MAX_INDEX_LEN = 64;
const int HASH_TABLE_SIZE = 32749; // Medium prime number for better distribution
const int RECORD_SIZE = MAX_INDEX_LEN + sizeof(int) + sizeof(int); // index + value + next_ptr

struct Record {
    char index[MAX_INDEX_LEN];
    int value;
    int next_ptr; // -1 for end of list
};

class FileStorage {
private:
    fstream data_file;
    fstream index_file;
    vector<int> hash_table;
    int free_list_head;
    
    // Simple cache for recently accessed records
    static const int CACHE_SIZE = 5000;
    struct CacheEntry {
        int pos;
        Record rec;
        bool valid;
    };
    CacheEntry cache[CACHE_SIZE];
    
    int hash_function(const string& key) {
        unsigned long hash = 0;
        for (char c : key) {
            hash = hash * 31 + c;
        }
        return hash % HASH_TABLE_SIZE;
    }
    
    void read_index_file() {
        index_file.open(INDEX_FILE, ios::in | ios::binary);
        if (index_file.good()) {
            index_file.read(reinterpret_cast<char*>(hash_table.data()), 
                          HASH_TABLE_SIZE * sizeof(int));
            index_file.read(reinterpret_cast<char*>(&free_list_head), sizeof(int));
        } else {
            // Initialize empty hash table
            fill(hash_table.begin(), hash_table.end(), -1);
            free_list_head = -1;
        }
        index_file.close();
    }
    
    void write_index_file() {
        index_file.open(INDEX_FILE, ios::out | ios::binary | ios::trunc);
        index_file.write(reinterpret_cast<char*>(hash_table.data()), 
                        HASH_TABLE_SIZE * sizeof(int));
        index_file.write(reinterpret_cast<char*>(&free_list_head), sizeof(int));
        index_file.close();
    }
    
    int allocate_record() {
        if (free_list_head != -1) {
            int pos = free_list_head;
            data_file.seekg(pos * RECORD_SIZE);
            Record rec;
            data_file.read(reinterpret_cast<char*>(&rec), sizeof(Record));
            free_list_head = rec.next_ptr;
            return pos;
        }
        data_file.seekp(0, ios::end);
        return data_file.tellp() / RECORD_SIZE;
    }
    
    void free_record(int pos) {
        data_file.seekp(pos * RECORD_SIZE);
        Record rec;
        data_file.read(reinterpret_cast<char*>(&rec), sizeof(Record));
        rec.next_ptr = free_list_head;
        data_file.seekp(pos * RECORD_SIZE);
        data_file.write(reinterpret_cast<char*>(&rec), sizeof(Record));
        free_list_head = pos;
    }
    
    void write_record(int pos, const Record& rec) {
        data_file.seekp(pos * RECORD_SIZE);
        data_file.write(reinterpret_cast<const char*>(&rec), sizeof(Record));
        
        // Update cache
        int cache_idx = pos % CACHE_SIZE;
        cache[cache_idx].pos = pos;
        cache[cache_idx].rec = rec;
        cache[cache_idx].valid = true;
    }
    
    void read_record(int pos, Record& rec) {
        // Check cache first
        int cache_idx = pos % CACHE_SIZE;
        if (cache[cache_idx].valid && cache[cache_idx].pos == pos) {
            rec = cache[cache_idx].rec;
            return;
        }
        
        // Read from file
        data_file.seekg(pos * RECORD_SIZE);
        data_file.read(reinterpret_cast<char*>(&rec), sizeof(Record));
        
        // Update cache
        cache[cache_idx].pos = pos;
        cache[cache_idx].rec = rec;
        cache[cache_idx].valid = true;
    }
    
public:
    FileStorage() {
        hash_table.resize(HASH_TABLE_SIZE);
        read_index_file();
        data_file.open(DATA_FILE, ios::in | ios::out | ios::binary);
        if (!data_file.good()) {
            data_file.close();
            data_file.open(DATA_FILE, ios::out | ios::binary | ios::trunc);
            data_file.close();
            data_file.open(DATA_FILE, ios::in | ios::out | ios::binary);
        }
        
        // Initialize cache
        for (int i = 0; i < CACHE_SIZE; i++) {
            cache[i].valid = false;
        }
    }
    
    ~FileStorage() {
        write_index_file();
        if (data_file.is_open()) {
            data_file.close();
        }
        if (index_file.is_open()) {
            index_file.close();
        }
    }
    
    void insert(const string& index, int value) {
        int hash_val = hash_function(index);
        int prev_ptr = -1;
        int curr_ptr = hash_table[hash_val];
        
        // Check if record already exists
        while (curr_ptr != -1) {
            Record rec;
            read_record(curr_ptr, rec);
            if (strcmp(rec.index, index.c_str()) == 0 && rec.value == value) {
                return; // Already exists
            }
            prev_ptr = curr_ptr;
            curr_ptr = rec.next_ptr;
        }
        
        // Insert new record
        Record new_rec;
        strncpy(new_rec.index, index.c_str(), MAX_INDEX_LEN - 1);
        new_rec.index[MAX_INDEX_LEN - 1] = '\0';
        new_rec.value = value;
        new_rec.next_ptr = hash_table[hash_val];
        
        int new_pos = allocate_record();
        write_record(new_pos, new_rec);
        hash_table[hash_val] = new_pos;
    }
    
    void remove(const string& index, int value) {
        int hash_val = hash_function(index);
        int prev_ptr = -1;
        int curr_ptr = hash_table[hash_val];
        
        while (curr_ptr != -1) {
            Record rec;
            read_record(curr_ptr, rec);
            if (strcmp(rec.index, index.c_str()) == 0 && rec.value == value) {
                // Found the record to delete
                if (prev_ptr == -1) {
                    // It's the first record
                    hash_table[hash_val] = rec.next_ptr;
                } else {
                    // Update previous record's next pointer
                    Record prev_rec;
                    read_record(prev_ptr, prev_rec);
                    prev_rec.next_ptr = rec.next_ptr;
                    write_record(prev_ptr, prev_rec);
                }
                free_record(curr_ptr);
                return;
            }
            prev_ptr = curr_ptr;
            curr_ptr = rec.next_ptr;
        }
    }
    
    vector<int> find(const string& index) {
        vector<int> result;
        int hash_val = hash_function(index);
        int curr_ptr = hash_table[hash_val];
        
        while (curr_ptr != -1) {
            Record rec;
            read_record(curr_ptr, rec);
            if (strcmp(rec.index, index.c_str()) == 0) {
                result.push_back(rec.value);
            }
            curr_ptr = rec.next_ptr;
        }
        
        sort(result.begin(), result.end());
        return result;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    FileStorage storage;
    
    int n;
    cin >> n;
    cin.ignore(); // Ignore newline after n
    
    for (int i = 0; i < n; i++) {
        string line;
        getline(cin, line);
        
        size_t space1 = line.find(' ');
        if (space1 == string::npos) continue;
        
        string command = line.substr(0, space1);
        
        if (command == "insert") {
            size_t space2 = line.find(' ', space1 + 1);
            if (space2 == string::npos) continue;
            
            string index = line.substr(space1 + 1, space2 - space1 - 1);
            int value = stoi(line.substr(space2 + 1));
            storage.insert(index, value);
            
        } else if (command == "delete") {
            size_t space2 = line.find(' ', space1 + 1);
            if (space2 == string::npos) continue;
            
            string index = line.substr(space1 + 1, space2 - space1 - 1);
            int value = stoi(line.substr(space2 + 1));
            storage.remove(index, value);
            
        } else if (command == "find") {
            string index = line.substr(space1 + 1);
            vector<int> values = storage.find(index);
            
            if (values.empty()) {
                cout << "null";
            } else {
                for (size_t j = 0; j < values.size(); j++) {
                    if (j > 0) cout << " ";
                    cout << values[j];
                }
            }
            cout << "\n";
        }
    }
    
    return 0;
}