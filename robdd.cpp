#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <sstream>


using namespace std;

struct BDDNode {};

// -------------------------------- Verilog Parser --------------------------------------//
struct Gate {
    string type;
    string output;
    vector<string> inputs;
};

class VerilogParser {
private:
    vector<string> inputs;
    vector<string> outputs;
    vector<string> wires;
    vector<string> regs;
    vector<Gate> gates;
    map<string, BDDNode*> signalBDDs;

public:
    void parse(const string& verilogCode) {
        stringstream ss(verilogCode);
        string line;

        while(getline(ss, line)) {
            // Remove comments
            size_t commentPos = line.find("//");
            if(commentPos != string::npos) {
                line = line.substr(0, commentPos);
            }

            // Remove extra spaces
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);

            if(line.empty()) continue;

            if(line.substr(0, 5) == "input") {
                parseInput(line);
            } else if(line.substr(0, 6) == "output") {
                parseOutput(line);
            } else if(line.substr(0, 4) == "wire") {
                parseWire(line);
            } else if(line.substr(0, 3) == "reg") {
                parseReg(line);
            } else if(line.find("(") != string::npos && line.find(")") != string::npos) {
                parseGate(line);
            }
        }

        setVariableOrder(inputs);
        initializeInputBDDs();
    }

    void parseInput(const string& line) {
        size_t initial = line.find("input");
        if(initial != string::npos) {
            string vars = line.substr(initial + 5);
            vars.erase(remove(vars.begin(), vars.end(), ';'), vars.end());
            stringstream ss(vars);
            string var;
            while(getline(ss, var, ',')) {
                var.erase(0, var.find_first_not_of(" \t"));
                var.erase(var.find_last_not_of(" \t") + 1);
                if(!var.empty()) {
                    inputs.push_back(var);
                }
            }
        }
    }

    void parseOutput(const string& line) {
        size_t initial = line.find("output");
        if(initial != string::npos) {
            string vars = line.substr(initial + 6);
            vars.erase(remove(vars.begin(), vars.end(), ';'), vars.end());
            stringstream ss(vars);
            string var;
            while(getline(ss, var, ',')) {
                var.erase(0, var.find_first_not_of(" \t"));
                var.erase(var.find_last_not_of(" \t") + 1);
                if(!var.empty()) {
                    outputs.push_back(var);
                }
            }
        }
    }

    void parseWire(const string& line) {
        size_t initial = line.find("wire");
        if(initial != string::npos) {
            string vars = line.substr(initial + 4);
            vars.erase(remove(vars.begin(), vars.end(), ';'), vars.end());
            stringstream ss(vars);
            string var;
            while(getline(ss, var, ',')) {
                var.erase(0, var.find_first_not_of(" \t"));
                var.erase(var.find_last_not_of(" \t") + 1);
                if(!var.empty()) {
                    wires.push_back(var);
                }
            }
        }
    }

    void parseReg(const string& line) {
        size_t initial = line.find("reg");
        if(initial != string::npos) {
            string vars = line.substr(initial + 3);
            vars.erase(remove(vars.begin(), vars.end(), ';'), vars.end());
            stringstream ss(vars);
            string var;
            while(getline(ss, var, ',')) {
                var.erase(0, var.find_first_not_of(" \t"));
                var.erase(var.find_last_not_of(" \t") + 1);
                if(!var.empty()) {
                    regs.push_back(var);
                }
            }
        }
    }

    void parseGate(const string& line) {
        Gate gate;

        // Get gate type
        size_t parentPos = line.find("(");
        if(parentPos != string::npos) return;

        gate.type = line.substr(0, parentPos);
        gate.type.erase(0, gate.type.find_first_not_of(" \t"));
        gate.type.erase(gate.type.find_last_not_of(" \t") + 1);

        // Get signals
        string signals = line.substr(parentPos + 1);
        signals = signals.substr(0, signals.find_last_of(")"));

        stringstream ss(signals);
        string signal;
        vector<string> signalList;
        while(getline(ss, signal, ',')) {
            
        }
    }
}
