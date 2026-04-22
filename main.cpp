#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdint>

using namespace std;

// Memory-efficient key-value database
// Uses sorted vector<pair<string, vector<int>>> with reserved capacity

const char* DATA_FILE = "data.bin";

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

static int load_from_file(vector<pair<string, vector<int>>>& data, FILE* fp) {
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    if (size == 0) return 0;

    fseek(fp, 0, SEEK_SET);
    int32_t num_keys;
    if (fread(&num_keys, 4, 1, fp) != 1) return 0;

    data.reserve(data.size() + num_keys);
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
    return num_keys;
}

static int find_key(const vector<pair<string, vector<int>>>& data, const string& key) {
    int lo = 0, hi = (int)data.size() - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int cmp = data[mid].first.compare(key);
        if (cmp == 0) return mid;
        else if (cmp < 0) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

static int lower_bound_key(const vector<pair<string, vector<int>>>& data, const string& key) {
    int lo = 0, hi = (int)data.size();
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (data[mid].first < key) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    vector<pair<string, vector<int>>> data;
    FILE* fp = fopen(DATA_FILE, "r+b");
    if (fp) {
        load_from_file(data, fp);
    } else {
        fp = fopen(DATA_FILE, "w+b");
    }
    if (!fp) return 1;

    // Reserve capacity based on current size to avoid excessive doubling
    if (!data.empty()) {
        data.reserve(data.size() * 2);
    }

    int n; cin >> n;
    string cmd, index; int value;

    for (int i = 0; i < n; i++) {
        cin >> cmd;
        if (cmd == "insert") {
            cin >> index >> value;
            int pos = find_key(data, index);
            if (pos >= 0) {
                auto& values = data[pos].second;
                if (!binary_search(values.begin(), values.end(), value)) {
                    auto it = lower_bound(values.begin(), values.end(), value);
                    values.insert(it, value);
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
                auto it = lower_bound(values.begin(), values.end(), value);
                if (it != values.end() && *it == value) {
                    values.erase(it);
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

    save_to_file(data, fp);
    fclose(fp);
    return 0;
}
