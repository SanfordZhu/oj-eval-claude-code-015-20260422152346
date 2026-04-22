#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstring>
#include <cstdint>

using namespace std;

// Memory-efficient key-value database using std::map
// map has less overhead than unordered_map (no bucket array, no hash stored)
// and avoids vector capacity doubling waste

const char* DATA_FILE = "data.bin";

static void save_to_file(const map<string, vector<int>>& data, FILE* fp) {
    fseek(fp, 0, SEEK_SET);
    int32_t num_keys = data.size();
    fwrite(&num_keys, 4, 1, fp);

    for (auto& [key, values] : data) {
        int32_t klen = key.size();
        int32_t vcount = values.size();
        fwrite(&klen, 4, 1, fp);
        fwrite(key.c_str(), klen, 1, fp);
        fwrite(&vcount, 4, 1, fp);
        fwrite(values.data(), 4, vcount, fp);
    }
}

static void load_from_file(map<string, vector<int>>& data, FILE* fp) {
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

        vector<int> values(vcount);
        if (vcount > 0 && fread(values.data(), 4, vcount, fp) != (size_t)vcount) break;

        data.emplace(move(key), move(values));
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    map<string, vector<int>> data;
    FILE* fp = fopen(DATA_FILE, "r+b");
    if (fp) {
        load_from_file(data, fp);
    } else {
        fp = fopen(DATA_FILE, "w+b");
    }
    if (!fp) return 1;

    int n; cin >> n;
    string cmd, index; int value;

    for (int i = 0; i < n; i++) {
        cin >> cmd;
        if (cmd == "insert") {
            cin >> index >> value;
            auto& values = data[index];
            if (!binary_search(values.begin(), values.end(), value)) {
                auto it = lower_bound(values.begin(), values.end(), value);
                values.insert(it, value);
            }
        } else if (cmd == "delete") {
            cin >> index >> value;
            auto it = data.find(index);
            if (it != data.end()) {
                auto& values = it->second;
                auto vit = lower_bound(values.begin(), values.end(), value);
                if (vit != values.end() && *vit == value) {
                    values.erase(vit);
                    if (values.empty()) data.erase(it);
                }
            }
        } else if (cmd == "find") {
            cin >> index;
            auto it = data.find(index);
            if (it == data.end() || it->second.empty()) {
                cout << "null\n";
            } else {
                for (size_t j = 0; j < it->second.size(); j++) {
                    if (j > 0) cout << ' ';
                    cout << it->second[j];
                }
                cout << '\n';
            }
        }
    }

    save_to_file(data, fp);
    fclose(fp);
    return 0;
}
