#include <cassert>
#include <chrono>
#include <iostream>
#include "leveldb/db.h"
#include "stats.h"
#include <cstring>
#include "cxxopts.hpp"
#include <unistd.h>
#include <fstream>
#include <cmath>
#include <random>

#ifdef JL_LIBCFS
#include "fsapi.h"
#endif

using namespace leveldb;
using namespace adgMod;
using namespace std;

int num_pairs_base = 1000;
int mix_base = 20;

class DBOperation {
public:
    uint8_t op;
    uint8_t sub_field;
    string target;

    DBOperation(uint32_t opcode, string&& key, uint32_t sub = 0) : op(opcode), sub_field(sub), target(key) {}
};

string FormatString(const string& original, int size) {
    if (original.length() < size) {
        return std::move(string(size - original.length(), '0') + original);
    } else {
        return std::move(original.substr(original.length() - size));
    }
} 


int main(int argc, char *argv[]) {
    int key_size, value_size, n, db_offset;
    string input_filename;
    bool print_single_timing, evict, fresh_write, pause, debug;
#ifdef JL_LIBCFS
    string db_location_base = "";
#else
    string db_location_base = "/ssd-data/0/";
#endif

    cxxopts::Options commandline_options("leveldb read test", "Testing leveldb read performance.");
    commandline_options.add_options()
            ("k,key_size", "the size of key", cxxopts::value<int>(key_size)->default_value("16"))
            ("v,value_size", "the size of value", cxxopts::value<int>(value_size)->default_value("64"))
            ("single_timing", "print the time of every single get", cxxopts::value<bool>(print_single_timing)->default_value("false"))
            ("f,input_file", "the filename of input file", cxxopts::value<string>(input_filename)->default_value(""))
            ("w,write", "writedb", cxxopts::value<bool>(fresh_write)->default_value("false"))
            ("e,uncache", "evict cache", cxxopts::value<bool>(evict)->default_value("false"))
            ("p,pause", "pause between operation", cxxopts::value<bool>(pause)->default_value("false"))
            ("g,debug", "print debug info", cxxopts::value<bool>(debug)->default_value("false"))
            ("n,num_operation", "number of operations", cxxopts::value<int>(n)->default_value("10000000"))
            ("d, db_loc_offset", "db location offset", cxxopts::value<int>(db_offset)->default_value("0"));
    
    auto result = commandline_options.parse(argc, argv);

    string db_location = db_location_base + to_string(db_offset);

    vector<DBOperation> ops;
    {
        ifstream input(input_filename);
        if (!input) exit(-11);
        uint32_t op;
        string key;
        int count = 0;
        while (input >> op >> key) {
            string formatted_key = FormatString(key, key_size);
            uint32_t sub = 0;
            if (op == 2) input >> sub;
            ops.emplace_back(op, std::move(formatted_key), sub);
            if (++count >= n) break;
        }
    }

    adgMod::Stats* instance = adgMod::Stats::GetInstance();
    string values(1024 * 1024, '0');

    if (fresh_write) {
#ifndef JL_LIBCFS
        string command = "rm -rf " + db_location + "/*";
        system(command.c_str());
#endif
    }

    if (evict) {
        system("sync; echo 3 | sudo tee /proc/sys/vm/drop_caches");
    }

    Options options;
    ReadOptions read_options;
    WriteOptions write_options;
    Status status;
    DB* db;

#ifdef JL_LIBCFS
    struct stat tmp_st;
    int rt = fs_stat(db_location.c_str(), &tmp_st);
    cerr << "fs_stat" << db_location << " ret:" << rt << endl;
    string current_str = db_location + "/CURRENT";
    rt =  fs_stat(current_str.c_str(), &tmp_st);
    cerr << "fs_stat" << current_str << " ret:" << rt << endl;
#endif

    cerr << "Starting up" << endl;
    status = DB::Open(options, db_location, &db);
    if (!status.ok()) {
        throw std::runtime_error("open Failed");
    }
    Iterator* db_iter = db->NewIterator(read_options);

    cerr << "Start running " << n << " operations at " << db_location << " op_file_size " << ops.size() << endl;
    instance->StartTimer(0);
    string value;
    for (int i = 0; i < n; ++i) {
        DBOperation& op = ops[i % ops.size()];
        // op.op = 1;
        if (op.op == 0) {
            status = db->Get(read_options, op.target, &value);
        } else if (op.op == 1) {
            value = values.substr(0, value_size);
            status = db->Put(write_options, op.target, value);
        } else if (op.op == 2) {
            db_iter->Seek(op.target);
            for (int r = 0; r < op.sub_field; ++r) {
                if (!db_iter->Valid()) break;
                value = db_iter->value().ToString();
                db_iter->Next();
            }
        } else {
            assert(false && "Unknown OpCode");
        }
        assert(status.ok() && "Operation not OK");
        if (debug) {
            if (status.ok()) {
                printf("operation %d finished %d, %s\n", i, op.op, op.target.c_str());
            } else {
                printf("operation %d failed %d, %s\n", i, op.op, op.target.c_str());
                throw std::runtime_error("operation failed");
            }
        }
    }
    instance->PauseTimer(0);

    instance->ReportTime();

    sleep(5);
    delete db;

#ifdef JL_LIBCFS
    if (db_offset == 1) {
        fprintf(stderr, "trying to syncall\n");
        fflush(stderr);
        fs_syncall();
    }
    fs_exit();
    fprintf(stderr, "fs_exit DONE\n");
    fs_cleanup();
    fprintf(stderr, "fs_cleanup DONE\n");
#endif
}
