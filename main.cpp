#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <cstdint>

using namespace std;

// In-memory key-value database with file persistence
// Uses unordered_map<string, set<int>> for O(1) key lookup and O(log n) value operations
// Persists to file between test cases

const char* DATA_FILE = "data.bin";

// Serialization format:
// [num_keys:4B]
// For each key:
//   [key_len:4B][key_data:variable][num_values:4B][values:4B each]

static void save_to_file(const unordered_map<string, set<int>>& db, FILE* fp) {
    fseek(fp, 0, SEEK_SET);
    int32_t num_keys = db.size();
    fwrite(&num_keys, 4, 1, fp);

    for (auto& [key, values] : db) {
        int32_t klen = key.size();
        int32_t vcount = values.size();
        fwrite(&klen, 4, 1, fp);
        fwrite(key.c_str(), klen, 1, fp);
        fwrite(&vcount, 4, 1, fp);
        for (int v : values) {
            fwrite(&v, 4, 1, fp);
        }
    }
}

static void load_from_file(unordered_map<string, set<int>>& db, FILE* fp) {
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    if (size == 0) return;

    fseek(fp, 0, SEEK_SET);
    int32_t num_keys;
    if (fread(&num_keys, 4, 1, fp) != 1) return;

    for (int32_t i = 0; i < num_keys; i++) {
        int32_t klen;
        if (fread(&klen, 4, 1, fp) != 1) break;
        if (klen <= 0 || klen > 64) break;

        string key(klen, '\0');
        if (fread(&key[0], klen, 1, fp) != 1) break;

        int32_t vcount;
        if (fread(&vcount, 4, 1, fp) != 1) break;
        if (vcount < 0 || vcount > 100000) break;

        set<int> values;
        for (int32_t j = 0; j < vcount; j++) {
            int v;
            if (fread(&v, 4, 1, fp) != 1) break;
            values.insert(v);
        }
        db[move(key)] = move(values);
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // Load existing data
    unordered_map<string, set<int>> db;
    FILE* fp = fopen(DATA_FILE, "r+b");
    if (fp) {
        load_from_file(db, fp);
    } else {
        fp = fopen(DATA_FILE, "w+b");
    }
    if (!fp) return 1;

    int n;
    cin >> n;

    string cmd, index;
    int value;

    for (int i = 0; i < n; i++) {
        cin >> cmd;
        if (cmd == "insert") {
            cin >> index >> value;
            db[index].insert(value);
        } else if (cmd == "delete") {
            cin >> index >> value;
            auto it = db.find(index);
            if (it != db.end()) {
                it->second.erase(value);
                if (it->second.empty()) {
                    db.erase(it);
                }
            }
        } else if (cmd == "find") {
            cin >> index;
            auto it = db.find(index);
            if (it == db.end() || it->second.empty()) {
                cout << "null\n";
            } else {
                bool first = true;
                for (int v : it->second) {
                    if (!first) cout << ' ';
                    cout << v;
                    first = false;
                }
                cout << '\n';
            }
        }
    }

    // Save back to file
    save_to_file(db, fp);
    fclose(fp);
    return 0;
}
