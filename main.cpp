#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdint>

using namespace std;

// Memory-efficient key-value database
// Uses map<string, vector<int>> with sorted vectors for values
// More memory efficient than set (no per-node allocation overhead)

const char* DATA_FILE = "data.bin";

// Serialization format:
// [num_keys:4B]
// For each key:
//   [key_len:4B][key_data:variable][num_values:4B][values:4B each]

static void save_to_file(const vector<pair<string, vector<int>>>& data, FILE* fp) {
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

static void load_from_file(vector<pair<string, vector<int>>>& data, FILE* fp) {
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    if (size == 0) return;

    fseek(fp, 0, SEEK_SET);
    int32_t num_keys;
    if (fread(&num_keys, 4, 1, fp) != 1) return;

    data.reserve(num_keys);
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

        data.emplace_back(move(key), move(values));
    }
}

// Binary search for key in sorted vector
static int find_key(const vector<pair<string, vector<int>>>& data, const string& key) {
    int lo = 0, hi = (int)data.size() - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int cmp = data[mid].first.compare(key);
        if (cmp == 0) return mid;
        else if (cmp < 0) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;  // not found
}

// Binary search for insertion point
static int lower_bound_key(const vector<pair<string, vector<int>>>& data, const string& key) {
    int lo = 0, hi = (int)data.size();
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (data[mid].first < key) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

// Binary search for value in sorted vector
static bool find_value(const vector<int>& values, int value) {
    return binary_search(values.begin(), values.end(), value);
}

static int lower_bound_value(const vector<int>& values, int value) {
    return lower_bound(values.begin(), values.end(), value) - values.begin();
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // Load existing data
    vector<pair<string, vector<int>>> data;
    FILE* fp = fopen(DATA_FILE, "r+b");
    if (fp) {
        load_from_file(data, fp);
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
            int pos = find_key(data, index);
            if (pos >= 0) {
                auto& values = data[pos].second;
                if (!find_value(values, value)) {
                    int ins = lower_bound_value(values, value);
                    values.insert(values.begin() + ins, value);
                }
            } else {
                int ins = lower_bound_key(data, index);
                data.emplace(data.begin() + ins, index, vector<int>{value});
            }
        } else if (cmd == "delete") {
            cin >> index >> value;
            int pos = find_key(data, index);
            if (pos >= 0) {
                auto& values = data[pos].second;
                int vpos = lower_bound_value(values, value);
                if (vpos < (int)values.size() && values[vpos] == value) {
                    values.erase(values.begin() + vpos);
                    if (values.empty()) {
                        data.erase(data.begin() + pos);
                    }
                }
            }
        } else if (cmd == "find") {
            cin >> index;
            int pos = find_key(data, index);
            if (pos < 0 || data[pos].second.empty()) {
                cout << "null\n";
            } else {
                auto& values = data[pos].second;
                for (size_t j = 0; j < values.size(); j++) {
                    if (j > 0) cout << ' ';
                    cout << values[j];
                }
                cout << '\n';
            }
        }
    }

    // Save back to file
    save_to_file(data, fp);
    fclose(fp);
    return 0;
}
