#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <sstream>
#include <set>
#include <functional>
#include <utility>

using namespace std;

// -------------------------------- BDD Node Structure --------------------------------------//
struct BDDNode {
    int id;
    string variable;
    BDDNode* low;
    BDDNode* high;

    BDDNode(string var, BDDNode* l, BDDNode* h) : variable(var), low(l), high(h) {
        static int counter = 0;
        id = counter++;
    }
};

// Terminal nodes (will be initialized later)
BDDNode* BDD_ZERO = nullptr;
BDDNode* BDD_ONE = nullptr;

// Unique table and node table
map<pair<string, pair<int, int>>, BDDNode*> uniqueTable;
map<int, BDDNode*> nodeTable;

// Forward declarations (used later)
class ROBDDBuilder;
BDDNode* rebuildROBDD(const string& verilogCode);

// -------------------------------- Create node with reduction --------------------------------------//
BDDNode* makeNode(const string& var, BDDNode* low, BDDNode* high) {
    if (low == high) return low;

    pair<string, pair<int, int>> key = make_pair(var, make_pair(low->id, high->id));

    auto it = uniqueTable.find(key);
    if (it != uniqueTable.end()) return it->second;

    BDDNode* node = new BDDNode(var, low, high);
    uniqueTable[key] = node;
    nodeTable[node->id] = node;
    return node;
}

// -------------------------------- Variable Ordering --------------------------------------//
vector<string> variableOrder;
map<string, int> variableIndex;

void setVariableOrder(const vector<string>& vars) {
    variableOrder = vars;
    variableIndex.clear();
    for (int i = 0; i < (int)vars.size(); i++) variableIndex[vars[i]] = i;
}

int getVariableIndex(const string& var) {
    auto it = variableIndex.find(var);
    if (it != variableIndex.end()) return it->second;
    return (int)variableOrder.size(); // constants go after vars
}

int computeBDDSize() {
    return (int)nodeTable.size();
}

// -------------------------------- BDD Operations --------------------------------------//
using OpFunc = function<bool(bool, bool)>;

OpFunc AndOp  = [](bool a, bool b) { return a && b; };
OpFunc OrOp   = [](bool a, bool b) { return a || b; };
OpFunc XorOp  = [](bool a, bool b) { return a ^ b; };
OpFunc NandOp = [](bool a, bool b) { return !(a && b); };
OpFunc NorOp  = [](bool a, bool b) { return !(a || b); };

inline bool isTerminal(BDDNode* n) { return n == BDD_ZERO || n == BDD_ONE; }
inline bool valueOf(BDDNode* n) { return n == BDD_ONE; }

BDDNode* apply(BDDNode* f, BDDNode* g, OpFunc op) {
    if (isTerminal(f) && isTerminal(g)) {
        return op(valueOf(f), valueOf(g)) ? BDD_ONE : BDD_ZERO;
    }

    string f_var = isTerminal(f) ? "" : f->variable;
    string g_var = isTerminal(g) ? "" : g->variable;

    int f_index = f_var.empty() ? (int)variableOrder.size() : getVariableIndex(f_var);
    int g_index = g_var.empty() ? (int)variableOrder.size() : getVariableIndex(g_var);

    string var;
    BDDNode *f_low, *f_high, *g_low, *g_high;

    if (f_index < g_index) {
        var = f_var;
        f_low = f->low;  f_high = f->high;
        g_low = g;       g_high = g;
    } else if (g_index < f_index) {
        var = g_var;
        f_low = f;       f_high = f;
        g_low = g->low;  g_high = g->high;
    } else {
        var = f_var;
        f_low = f->low;  f_high = f->high;
        g_low = g->low;  g_high = g->high;
    }

    BDDNode* low  = apply(f_low, g_low, op);
    BDDNode* high = apply(f_high, g_high, op);

    return makeNode(var, low, high);
}

BDDNode* bddNot(BDDNode* f) {
    if (isTerminal(f)) return (f == BDD_ONE) ? BDD_ZERO : BDD_ONE;
    return makeNode(f->variable, bddNot(f->low), bddNot(f->high));
}

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

        while (getline(ss, line)) {
            // Remove comments
            size_t commentPos = line.find("//");
            if (commentPos != string::npos) line = line.substr(0, commentPos);

            // Trim
            size_t first = line.find_first_not_of(" \t");
            if (first == string::npos) continue;
            size_t last = line.find_last_not_of(" \t");
            line = line.substr(first, last - first + 1);

            if (line.empty()) continue;

            if (line.rfind("input", 0) == 0) {
                parseInput(line);
            } else if (line.rfind("output", 0) == 0) {
                parseOutput(line);
            } else if (line.rfind("wire", 0) == 0) {
                parseWire(line);
            } else if (line.rfind("reg", 0) == 0) {
                parseReg(line);
            } else if (line.find("(") != string::npos && line.find(")") != string::npos) {
                parseGate(line);
            }
        }

        setVariableOrder(inputs);
        initializeInputBDDs();
    }

    void parseInput(const string& line) {
        size_t pos = line.find("input");
        string vars = line.substr(pos + 5);
        vars.erase(remove(vars.begin(), vars.end(), ';'), vars.end());
        stringstream ss(vars);
        string var;
        while (getline(ss, var, ',')) {
            // trim
            size_t f = var.find_first_not_of(" \t");
            if (f == string::npos) continue;
            size_t l = var.find_last_not_of(" \t");
            var = var.substr(f, l - f + 1);
            if (!var.empty()) inputs.push_back(var);
        }
    }

    void parseOutput(const string& line) {
        size_t pos = line.find("output");
        string vars = line.substr(pos + 6);
        vars.erase(remove(vars.begin(), vars.end(), ';'), vars.end());
        stringstream ss(vars);
        string var;
        while (getline(ss, var, ',')) {
            size_t f = var.find_first_not_of(" \t");
            if (f == string::npos) continue;
            size_t l = var.find_last_not_of(" \t");
            var = var.substr(f, l - f + 1);
            if (!var.empty()) outputs.push_back(var);
        }
    }

    void parseWire(const string& line) {
        size_t pos = line.find("wire");
        string vars = line.substr(pos + 4);
        vars.erase(remove(vars.begin(), vars.end(), ';'), vars.end());
        stringstream ss(vars);
        string var;
        while (getline(ss, var, ',')) {
            size_t f = var.find_first_not_of(" \t");
            if (f == string::npos) continue;
            size_t l = var.find_last_not_of(" \t");
            var = var.substr(f, l - f + 1);
            if (!var.empty()) wires.push_back(var);
        }
    }

    void parseReg(const string& line) {
        size_t pos = line.find("reg");
        string vars = line.substr(pos + 3);
        vars.erase(remove(vars.begin(), vars.end(), ';'), vars.end());
        stringstream ss(vars);
        string var;
        while (getline(ss, var, ',')) {
            size_t f = var.find_first_not_of(" \t");
            if (f == string::npos) continue;
            size_t l = var.find_last_not_of(" \t");
            var = var.substr(f, l - f + 1);
            if (!var.empty()) regs.push_back(var);
        }
    }

    void parseGate(const string& line) {
        Gate gate;

        size_t parentPos = line.find("(");
        if (parentPos == string::npos) return;

        gate.type = line.substr(0, parentPos);
        // trim gate.type
        size_t f = gate.type.find_first_not_of(" \t");
        size_t l = gate.type.find_last_not_of(" \t");
        gate.type = gate.type.substr(f, l - f + 1);

        string signals = line.substr(parentPos + 1);
        size_t closing = signals.find_last_of(")");
        if (closing != string::npos) signals = signals.substr(0, closing);

        stringstream ss(signals);
        string token;
        vector<string> signalList;
        while (getline(ss, token, ',')) {
            size_t ft = token.find_first_not_of(" \t");
            if (ft == string::npos) continue;
            size_t lt = token.find_last_not_of(" \t");
            token = token.substr(ft, lt - ft + 1);
            if (!token.empty()) signalList.push_back(token);
        }

        if (!signalList.empty()) {
            gate.output = signalList[0];
            for (size_t i = 1; i < signalList.size(); ++i) gate.inputs.push_back(signalList[i]);
            gates.push_back(gate);
        }
    }

    void initializeInputBDDs() {
        for (const string& input : inputs) {
            signalBDDs[input] = makeNode(input, BDD_ZERO, BDD_ONE);
        }
    }

    BDDNode* getSignalBDD(const string& signal) {
        auto it = signalBDDs.find(signal);
        if (it != signalBDDs.end()) return it->second;
        return nullptr;
    }

    void setSignalBDD(const string& signal, BDDNode* bdd) {
        signalBDDs[signal] = bdd;
    }

    vector<string> getInputs() const { return inputs; }
    vector<string> getOutputs() const { return outputs; }
    vector<Gate> getGates() const { return gates; }
    map<string, BDDNode*> getSignalBDDs() const { return signalBDDs; }
};

// -------------------------------- ROBDD Builder --------------------------------------//
class ROBDDBuilder {
private:
    VerilogParser parser;

public:
    BDDNode* buildROBDD(const string& verilogCode) {
        parser.parse(verilogCode);
        processGates();
        if (!parser.getOutputs().empty()) return parser.getSignalBDD(parser.getOutputs()[0]);
        return BDD_ZERO;
    }

    vector<string> getParserInputs() const { return parser.getInputs(); }
    vector<Gate> getParserGates() const { return parser.getGates(); }

    void processGates() {
        vector<Gate> gates = parser.getGates();
        set<string> processedSignals;

        for (const string& input : parser.getInputs()) processedSignals.insert(input);

        while (processedSignals.size() < gates.size() + parser.getInputs().size()) {
            bool progress = false;

            for (const Gate& gate : gates) {
                bool allInputsReady = true;
                for (const string& in : gate.inputs) {
                    if (processedSignals.find(in) == processedSignals.end() &&
                        find(parser.getInputs().begin(), parser.getInputs().end(), in) == parser.getInputs().end()) {
                        allInputsReady = false;
                        break;
                    }
                }

                if (processedSignals.find(gate.output) == processedSignals.end() && allInputsReady) {
                    BDDNode* result = evaluateGate(gate);
                    parser.setSignalBDD(gate.output, result);
                    processedSignals.insert(gate.output);
                    progress = true;
                }
            }

            if (!progress) {
                // last resort: attempt to process remaining gates anyway
                for (const Gate& gate : gates) {
                    if (processedSignals.find(gate.output) == processedSignals.end()) {
                        BDDNode* result = evaluateGate(gate);
                        parser.setSignalBDD(gate.output, result);
                        processedSignals.insert(gate.output);
                    }
                }
                break;
            }
        }
    }

    BDDNode* evaluateGate(const Gate& gate) {
        if (gate.inputs.empty()) return BDD_ZERO;

        if (gate.type == "not" || gate.type == "NOT") {
            BDDNode* in = parser.getSignalBDD(gate.inputs[0]);
            if (in) return bddNot(in);
        } else if (gate.type == "and" || gate.type == "AND") {
            BDDNode* result = parser.getSignalBDD(gate.inputs[0]);
            for (size_t i = 1; i < gate.inputs.size(); ++i) {
                BDDNode* in = parser.getSignalBDD(gate.inputs[i]);
                result = apply(result, in ? in : BDD_ZERO, AndOp);
            }
            return result;
        } else if (gate.type == "or" || gate.type == "OR") {
            BDDNode* result = parser.getSignalBDD(gate.inputs[0]);
            for (size_t i = 1; i < gate.inputs.size(); ++i) {
                BDDNode* in = parser.getSignalBDD(gate.inputs[i]);
                result = apply(result, in ? in : BDD_ZERO, OrOp);
            }
            return result;
        } else if (gate.type == "xor" || gate.type == "XOR") {
            BDDNode* result = parser.getSignalBDD(gate.inputs[0]);
            for (size_t i = 1; i < gate.inputs.size(); ++i) {
                BDDNode* in = parser.getSignalBDD(gate.inputs[i]);
                result = apply(result, in ? in : BDD_ZERO, XorOp);
            }
            return result;
        } else if (gate.type == "nand" || gate.type == "NAND") {
            BDDNode* result = parser.getSignalBDD(gate.inputs[0]);
            for (size_t i = 1; i < gate.inputs.size(); ++i) {
                BDDNode* in = parser.getSignalBDD(gate.inputs[i]);
                result = apply(result, in ? in : BDD_ZERO, NandOp);
            }
            return result;
        }
        return BDD_ZERO;
    }
};

// -------------------------------- Rebuild + Sifting --------------------------------------//

// Rebuild ROBDD using the current variableOrder. Returns the top node.
BDDNode* rebuildROBDD(const string& verilogCode) {
    uniqueTable.clear();
    nodeTable.clear();

    // Recreate terminal nodes (note: ids will keep increasing; acceptable for this simple implementation)
    BDD_ZERO = new BDDNode("0", nullptr, nullptr);
    BDD_ONE  = new BDDNode("1", nullptr, nullptr);

    ROBDDBuilder builder;
    return builder.buildROBDD(verilogCode);
}

// Sifting function: tries moving each variable up/down to find best position.
void siftVariables(const string& verilogCode) {
    if (variableOrder.empty()) return;

    // Initial build to populate tables
    rebuildROBDD(verilogCode);

    for (int i = 0; i < (int)variableOrder.size(); ++i) {
        string var = variableOrder[i];
        int bestPosition = i;
        int minSize = computeBDDSize();

        vector<string> originalOrder = variableOrder;

        // Move variable up (towards index 0)
        for (int j = i - 1; j >= 0; --j) {
            swap(variableOrder[j], variableOrder[j + 1]);
            rebuildROBDD(verilogCode);
            int size = computeBDDSize();
            if (size < minSize) {
                minSize = size;
                bestPosition = j;
            }
        }

        // Restore original before downward moves
        variableOrder = originalOrder;

        // Move variable down
        for (int j = i + 1; j < (int)variableOrder.size(); ++j) {
            swap(variableOrder[j], variableOrder[j - 1]);
            rebuildROBDD(verilogCode);
            int size = computeBDDSize();
            if (size < minSize) {
                minSize = size;
                bestPosition = j;
            }
        }

        // Place var at bestPosition
        // If bestPosition == i, no change. Otherwise reinsert.
        if (bestPosition != i) {
            variableOrder.erase(variableOrder.begin() + i);
            variableOrder.insert(variableOrder.begin() + bestPosition, var);
            // rebuild at chosen position
            rebuildROBDD(verilogCode);
        } else {
            // restore original if unchanged
            variableOrder = originalOrder;
        }
    }
}

// -------------------------------- BDD Printer --------------------------------------//
void printBDD(BDDNode* node, const string& indent = "", bool isLast = true) {
    if (!node) return;
    if (node == BDD_ZERO) {
        cout << indent << (isLast ? "└── " : "├── ") << "0" << endl;
        return;
    }
    if (node == BDD_ONE) {
        cout << indent << (isLast ? "└── " : "├── ") << "1" << endl;
        return;
    }

    cout << indent << (isLast ? "└── " : "├── ") << node->variable << endl;

    string newIndent = indent + (isLast ? "    " : "│   ");
    printBDD(node->low, newIndent, false);
    printBDD(node->high, newIndent, true);
}

// -------------------------------- Main --------------------------------------//
int main() {
    cout << "Enter combinational Verilog design (end with 'endmodule'):" << endl;

    string line;
    string verilogCode;

    while (getline(cin, line)) {
        verilogCode += line + "\n";
        if (line.find("endmodule") != string::npos) break;
    }

    // Initialize terminal nodes and tables
    uniqueTable.clear();
    nodeTable.clear();
    BDD_ZERO = new BDDNode("0", nullptr, nullptr);
    BDD_ONE  = new BDDNode("1", nullptr, nullptr);

    // Perform sifting to optimize variable order and build final ROBDD
    siftVariables(verilogCode);

    // Rebuild ROBDD using optimized variable order
    uniqueTable.clear();
    nodeTable.clear();
    BDD_ZERO = new BDDNode("0", nullptr, nullptr);
    BDD_ONE  = new BDDNode("1", nullptr, nullptr);

    ROBDDBuilder builder;
    BDDNode* finalRobdd = builder.buildROBDD(verilogCode);

    cout << "\nROBDD After Sifting (Optimized):" << endl;
    if (finalRobdd) printBDD(finalRobdd);
    else cout << "Failed to generate optimized ROBDD" << endl;

    return 0;
}

