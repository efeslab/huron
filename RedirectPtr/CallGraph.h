//
// Created by yifanz on 7/19/18.
//

#ifndef LLVM_CALLGRAPH_H
#define LLVM_CALLGRAPH_H

#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "Utils.h"

template <typename T>
raw_ostream &operator << (raw_ostream &os, const std::set<T> &set) {
    os << "{";
    bool comma = false;
    for (const auto &t: set) {
        if (comma)
            os << ", ";
        else
            comma = true;
        os << t;
    }
    os << "}";
    return os;
}

class CallGraphT {
    struct GraphNode {
        Function *func;
        std::set<size_t> threads;
        std::vector<GraphNode *> callees, callers;

        GraphNode(Function *f): func(f) {}

        std::unordered_set<Function *> getCallees() const {
            std::unordered_set<Function *> ret;
            for (auto insb = inst_begin(this->func), inse = inst_end(func); 
                 insb != inse; ++insb) {
                if (CallInst *ci = dyn_cast<CallInst>(&*insb))
                    ret.insert(ci->getCalledFunction());
                else if (InvokeInst *ii = dyn_cast<InvokeInst>(&*insb))
                    ret.insert(ii->getCalledFunction());
            }
            return ret;
        }

        void addCallee(GraphNode *node) {
            callees.push_back(node);
            node->callers.push_back(this);
        }

        void mergeThreadInfo(const std::set<size_t> &threads) {
            this->threads.insert(threads.begin(), threads.end());
        }

        void cleanup(const std::unordered_set<GraphNode *> &drop) {
            auto remove_pred = [&](GraphNode * n) { return drop.find(n) != drop.end(); };
            callees.erase(remove_if(callees.begin(), callees.end(), remove_pred), callees.end());
            callers.erase(remove_if(callers.begin(), callers.end(), remove_pred), callers.end());
        }
    };

    struct DFSNode {
        bool downward;

        DFSNode(bool downward): downward(downward) {}

        void operator()(GraphNode *node) const {
            std::unordered_set<GraphNode *> visited;
            goNext(node, visited);
        }

        void goNext(GraphNode *node, std::unordered_set<GraphNode *> &visited) const {
            if (visited.find(node) != visited.end())
                return;
            visited.insert(node);
            const std::vector<GraphNode *> &edges = 
                downward ? node->callees : node->callers;
            for (GraphNode *next: edges) {
                next->mergeThreadInfo(node->threads);
                goNext(next, visited);
            }
        }
    };

public:
    CallGraphT() {}

    void addStartFunc(Function *f) {
        addFunc(f, nullptr);
    }

    void hintFuncThreads(Function *f, const std::set<size_t> &threads) {
        auto it = graph.find(f);
        if (it != graph.end()) {
            it->second->mergeThreadInfo(threads);
            hinted.emplace_back(it->second);
        }
    }

    void propagateFuncThreads() {
        for (GraphNode *hint: hinted)
            DFSNode(false)(hint);
        std::unordered_set<GraphNode *> cleanups;
        for (auto it = graph.begin(); it != graph.end(); ) {
            if (it->second->threads.empty()) {
                cleanups.insert(it->second);
                it = graph.erase(it);
            }
            else
                ++it;
        }
        for (auto &p: graph)
            p.second->cleanup(cleanups);
        for (const auto &n: cleanups)
            delete n;
    }

    std::unordered_set<Function *> getFunctions() const {
        std::unordered_set<Function *> ret;
        for (const auto &p: graph)
            ret.insert(p.first);
        return ret;
    }

    std::set<size_t> getFuncThreads(Function *f) const {
        auto it = graph.find(f);
        // All unfound functions by default is run by thread 0.
        return it == graph.end() ? std::set<size_t>{0} : it->second->threads;
    }

    friend raw_ostream &operator << (raw_ostream &os, const CallGraphT &graph) {
        for (const auto &p: graph.graph) {
            auto name1 = p.first->getName();
            for (const auto &n: p.second->callees) {
                auto name2 = n->func->getName();
                os << name1 << " -> " << name2 << " " << n->threads << '\n';
            }
        }
        return os;
    }

private:
    void addFunc(Function *f, GraphNode *parent) {
        if (f->isDeclaration())
            return;
        auto it = graph.find(f);
        GraphNode *node;
        if (it == graph.end()) {
            node = new GraphNode(f);
            graph.emplace(f, node);
            auto callees = node->getCallees();
            for (Function *f0: callees)
                addFunc(f0, node);
        }
        else
            node = it->second;
        if (parent)
            parent->addCallee(node);
    }

    std::vector<GraphNode *> hinted;

    std::unordered_map<Function *, GraphNode *> graph;
};

#endif
