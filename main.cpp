#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <cstdio>

using namespace std;

// Block-linked list file storage for key-value database
//
// Block format (BLOCK_SIZE bytes):
//   - next_block (int32_t): next block in chain (-1 = end)
//   - count (int32_t): number of entries in this block
//   - entries: [key_len (1 byte)][key_data][value (4 bytes)]...

const int BLOCK_SIZE = 512;
const int HEADER_SIZE = 8;
const int NUM_BUCKETS = 8192;
const char* DATA_FILE = "data.bin";

static inline uint32_t hash_key(const string& key) {
    uint32_t h = 2166136261u;
    for (char c : key) {
        h ^= (unsigned char)c;
        h *= 16777619u;
    }
    return h;
}

static inline int32_t get_bucket(uint32_t h) {
    return h % NUM_BUCKETS;
}

static inline int entry_size(const string& key) {
    return 1 + (int)key.size() + 4;
}

static void read_header(FILE* fp, int32_t block_num, int32_t& next_block, int32_t& count) {
    fseek(fp, block_num * BLOCK_SIZE, SEEK_SET);
    fread(&next_block, 4, 1, fp);
    fread(&count, 4, 1, fp);
}

static void write_header(FILE* fp, int32_t block_num, int32_t next_block, int32_t count) {
    fseek(fp, block_num * BLOCK_SIZE, SEEK_SET);
    fwrite(&next_block, 4, 1, fp);
    fwrite(&count, 4, 1, fp);
}

static vector<pair<string, int>> read_entries(FILE* fp, int32_t block_num) {
    vector<pair<string, int>> entries;
    int32_t next_block, count;
    read_header(fp, block_num, next_block, count);

    long pos = (long)block_num * BLOCK_SIZE + HEADER_SIZE;
    for (int32_t i = 0; i < count; i++) {
        fseek(fp, pos, SEEK_SET);
        unsigned char klen;
        if (fread(&klen, 1, 1, fp) != 1 || klen > 64) break;

        char buf[65];
        if (fread(buf, klen, 1, fp) != 1) break;
        string key(buf, klen);

        int value;
        if (fread(&value, 4, 1, fp) != 1) break;

        entries.emplace_back(move(key), value);
        pos += 1 + klen + 4;
    }
    return entries;
}

static int count_fit(const vector<pair<string, int>>& entries, int start) {
    int remaining = BLOCK_SIZE - HEADER_SIZE;
    for (size_t i = start; i < entries.size(); i++) {
        int sz = entry_size(entries[i].first);
        if (sz > remaining) return (int)(i - start);
        remaining -= sz;
    }
    return (int)entries.size() - start;
}

static int32_t alloc_block(FILE* fp) {
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    int32_t block_num = (int32_t)(file_size / BLOCK_SIZE);
    if (file_size % BLOCK_SIZE != 0) block_num++;

    long target = (long)(block_num + 1) * BLOCK_SIZE;
    fseek(fp, 0, SEEK_END);
    for (long i = ftell(fp); i < target; i++) {
        fputc(0, fp);
    }
    return block_num;
}

static void init_storage(FILE* fp) {
    fseek(fp, 0, SEEK_END);
    if (ftell(fp) == 0) {
        int32_t neg1 = -1, zero = 0;
        for (int i = 0; i < NUM_BUCKETS; i++) {
            fwrite(&neg1, 4, 1, fp);
            fwrite(&zero, 4, 1, fp);
            for (int j = HEADER_SIZE; j < BLOCK_SIZE; j++) fputc(0, fp);
        }
    }
}

// Collect all entries in a bucket chain, also records block numbers
static vector<pair<string, int>> collect_bucket(FILE* fp, int32_t bucket,
                                                  vector<int32_t>& blocks) {
    vector<pair<string, int>> all;
    int32_t next_block, count;
    read_header(fp, bucket, next_block, count);

    int32_t bn = next_block;
    while (bn >= 0) {
        blocks.push_back(bn);
        auto entries = read_entries(fp, bn);
        all.insert(all.end(), make_move_iterator(entries.begin()),
                   make_move_iterator(entries.end()));
        read_header(fp, bn, next_block, count);
        bn = next_block;
    }
    return all;
}

// Write entries into blocks, linking them, updating bucket pointer
static void write_chain(FILE* fp, int32_t bucket,
                        vector<pair<string, int>>& entries,
                        vector<int32_t>& old_blocks) {
    if (entries.empty()) {
        write_header(fp, bucket, -1, 0);
        return;
    }

    // Count how many blocks we need
    int total_blocks = 0;
    int idx = 0;
    while (idx < (int)entries.size()) {
        int cnt = count_fit(entries, idx);
        total_blocks++;
        idx += cnt;
    }

    // Allocate or reuse blocks
    vector<int32_t> blocks(total_blocks);
    for (int i = 0; i < total_blocks; i++) {
        if (i < (int)old_blocks.size()) {
            blocks[i] = old_blocks[i];
        } else {
            blocks[i] = alloc_block(fp);
        }
    }

    // Write entries and link
    idx = 0;
    for (int i = 0; i < total_blocks; i++) {
        int cnt = count_fit(entries, idx);
        int32_t next = (i + 1 < total_blocks) ? blocks[i + 1] : -1;

        long pos = (long)blocks[i] * BLOCK_SIZE + HEADER_SIZE;
        for (int j = 0; j < cnt; j++) {
            fseek(fp, pos, SEEK_SET);
            unsigned char klen = entries[idx + j].first.size();
            fwrite(&klen, 1, 1, fp);
            fwrite(entries[idx + j].first.c_str(), klen, 1, fp);
            fwrite(&entries[idx + j].second, 4, 1, fp);
            pos += 1 + klen + 4;
        }
        write_header(fp, blocks[i], next, cnt);
        idx += cnt;
    }

    // Update bucket pointer
    write_header(fp, bucket, blocks[0], 0);
}

static void insert_entry(FILE* fp, const string& key, int value) {
    int32_t bucket = get_bucket(hash_key(key));

    vector<int32_t> blocks;
    auto all = collect_bucket(fp, bucket, blocks);

    for (auto& e : all) {
        if (e.first == key && e.second == value) return;
    }

    all.emplace_back(key, value);
    sort(all.begin(), all.end());
    write_chain(fp, bucket, all, blocks);
}

static void delete_entry(FILE* fp, const string& key, int value) {
    int32_t bucket = get_bucket(hash_key(key));

    vector<int32_t> blocks;
    auto all = collect_bucket(fp, bucket, blocks);

    auto it = find(all.begin(), all.end(), make_pair(key, value));
    if (it == all.end()) return;
    all.erase(it);

    write_chain(fp, bucket, all, blocks);
}

static vector<int> find_values(FILE* fp, const string& key) {
    vector<int> values;
    int32_t bucket = get_bucket(hash_key(key));

    int32_t next_block, count;
    read_header(fp, bucket, next_block, count);

    int32_t bn = next_block;
    while (bn >= 0) {
        auto entries = read_entries(fp, bn);
        for (auto& e : entries) {
            if (e.first == key) values.push_back(e.second);
        }
        read_header(fp, bn, next_block, count);
        bn = next_block;
    }
    return values;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    FILE* fp = fopen(DATA_FILE, "r+b");
    if (!fp) fp = fopen(DATA_FILE, "w+b");
    if (!fp) return 1;

    init_storage(fp);

    int n;
    cin >> n;

    string cmd, index;
    int value;

    for (int i = 0; i < n; i++) {
        cin >> cmd;
        if (cmd == "insert") {
            cin >> index >> value;
            insert_entry(fp, index, value);
        } else if (cmd == "delete") {
            cin >> index >> value;
            delete_entry(fp, index, value);
        } else if (cmd == "find") {
            cin >> index;
            auto values = find_values(fp, index);
            if (values.empty()) {
                cout << "null\n";
            } else {
                sort(values.begin(), values.end());
                for (size_t j = 0; j < values.size(); j++) {
                    if (j > 0) cout << ' ';
                    cout << values[j];
                }
                cout << '\n';
            }
        }
    }

    fclose(fp);
    return 0;
}
