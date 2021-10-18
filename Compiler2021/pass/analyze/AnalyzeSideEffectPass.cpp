//
// Created by 陈思言 on 2021/7/17.
//

#include "AnalyzeSideEffectPass.h"
#include "stack"
#include "queue"


void mergeInto(FunctionSideEffect *src,
               FunctionSideEffect *dst) {
    dst->callExternalFunction.insert(src->callExternalFunction.begin(), src->callExternalFunction.end());
    dst->writeGlobalVariable.insert(src->writeGlobalVariable.begin(), src->writeGlobalVariable.end());
    dst->readGlobalVariable.insert(src->readGlobalVariable.begin(), src->readGlobalVariable.end());
}

struct Tarjan_SCC_SideEffect {
    typedef Function *F;
    std::map<F, std::set<F>> &G;
    std::map<int, std::set<F>> SCC = {};

    std::set<F> inStack = {};
    std::map<F, int> dfn = {}, low = {}, belong = {};
    std::stack<F> stack = {};
    int index, scc;

    void tarjan(F u) {
        low[u] = dfn[u] = ++index;
        stack.push(u);
        inStack.insert(u);
        for (F v : G[u]) {
            if (!dfn.count(v)) {
                tarjan(v);
                if (low[u] > low[v])low[u] = low[v];
            } else if (inStack.count(v) && low[u] > dfn[v])
                low[u] = dfn[v];
        }
        if (low[u] == dfn[u]) {
            scc++;
            SCC[scc] = {};
            F v;
            do {
                v = stack.top();
                stack.pop();
                inStack.erase(v);
                belong[v] = scc;
                SCC[scc].insert(v);
            } while (v != u);
        }
    }

    void solve() {
        index = scc = 0;
        for (auto &p : G) {
            if (!dfn.count(p.first))
                tarjan(p.first);
        }
    }

};

struct VisitorSelf : MIR::Visitor {
    FunctionSideEffect &effects;
    Function *thiz;
    std::map<Variable *, int> argsMap = {};

    VisitorSelf(FunctionSideEffect &effects, Function *function) :
    effects(effects), thiz(function) {
        for (int i = 0; i < thiz->args.size(); i++) {
            argsMap[thiz->args[i]] = i;
        }
    }

    void operatePtr(Value *ptr, bool read, bool write) {
        auto *p = dynamic_cast<ArrayAddressingMIR *>(ptr);
        if (p != nullptr) {
            operateArr(p, read, write);
        }
        auto *q = dynamic_cast<ValueAddressingMIR *>(ptr);
        if (q != nullptr) {
            operateValuePtr(q, read, write);
        }
        auto *v = dynamic_cast<LoadVariableMIR *>(ptr);
        if (v != nullptr) {
            if (argsMap.count(v->src)) {
                if (read)effects.readByPointerArg.insert(argsMap[v->src]);
                if (write)effects.writeByPointerArg.insert(argsMap[v->src]);
            } else if (v->src->getLocation() == Value::Location::STATIC) {
                if (read)effects.readGlobalVariable.insert(v->src);
                if (write)effects.writeGlobalVariable.insert(v->src);
            }
        }
    }

    void operateValuePtr(ValueAddressingMIR *mir, bool read, bool write) {
        if (mir->base->getLocation() == Value::Location::STATIC &&
        mir->base->getType()->getId() != Type::ID::POINTER) {
            auto *v = (Variable *) mir->base;
            if (read)effects.readGlobalVariable.insert(v);
            if (write)effects.writeGlobalVariable.insert(v);
        } else operatePtr(mir->base, read, write);
    }

    void operateArr(ArrayAddressingMIR *mir, bool read, bool write) {
        if (mir->base->getLocation() == Value::Location::STATIC &&
        mir->base->getType()->getId() != Type::ID::POINTER) {
            auto *v = (Variable *) mir->base;
            if (read)effects.readGlobalVariable.insert(v);
            if (write)effects.writeGlobalVariable.insert(v);
        } else operatePtr(mir->base, read, write);
    }

    void visit(StoreVariableMIR *mir) override {
        if (mir->dst->getLocation() == Value::Location::STATIC) {
            effects.writeGlobalVariable.insert(mir->dst);
        }
    }

    void visit(LoadVariableMIR *mir) override {
        if (mir->src->getLocation() == Value::Location::STATIC) {
            effects.readGlobalVariable.insert(mir->src);
        }
    }

    void visit(StorePointerMIR *mir) override {
        operatePtr(mir->dst, false, true);
    }

    void visit(LoadPointerMIR *mir) override {
        operatePtr(mir->src, true, false);
    }

    void visit(MemoryCopyMIR *mir) override {
        operatePtr(mir->src, true, false);
        operatePtr(mir->dst, false, true);
    }


    void visit(MemoryFillMIR *mir) override {
        operatePtr(mir->dst, false, true);
    }

    void visit(ReturnMIR *mir) override {
        if (!effects.returnConstant) return;
        auto *cnst = dynamic_cast<LoadConstantMIR *>(mir->val);
        if (cnst == nullptr) effects.returnConstant = false;
        else if (effects.returnValue == nullptr) {
            effects.returnValue = cnst->src;
        } else if (effects.returnValue->getValueString() != cnst->src->getValueString()) {
            effects.returnConstant = false;
            effects.returnValue = nullptr;
        }
    }

    void visit(CallMIR *mir) override {
        if (mir->func->isExternal) effects.callExternalFunction.insert(mir->func);
    }

    void visit(CallWithAssignMIR *mir) override {
        visit(static_cast<CallMIR *>(mir));
    }

    void visit(MultiCallMIR *mir) override {
        visit(static_cast<CallMIR *>(mir));
        operatePtr(mir->atomic_var, true, true);
    }

    void visit(AtomicLoopCondMIR *mir) override {
        operatePtr(mir->atomic_var, true, true);
    }
};

struct VisitorCall : MIR::Visitor {
    std::map<Function *, FunctionSideEffect *> &sideEffects;
    std::map<Function *, std::set<Function *>> &returnDataFlow;
    FunctionSideEffect &effects;
    Function *thiz;
    std::map<Variable *, int> argsMap = {};

    VisitorCall(std::map<Function *, FunctionSideEffect *> &sideEffects, Function *function,
                std::map<Function *, std::set<Function *>> &returnDataFlow) :
                sideEffects(sideEffects), thiz(function), effects(*sideEffects[function]),
                returnDataFlow(returnDataFlow) {
        for (int i = 0; i < thiz->args.size(); i++) {
            argsMap[thiz->args[i]] = i;
        }
    }

    void operatePtr(Value *ptr, bool read, bool write) {
        auto *p = dynamic_cast<ArrayAddressingMIR *>(ptr);
        if (p != nullptr) {
            operateArr(p, read, write);
        }
        auto *q = dynamic_cast<ValueAddressingMIR *>(ptr);
        if (q != nullptr) {
            operateValuePtr(q, read, write);
        }
        auto *v = dynamic_cast<LoadVariableMIR *>(ptr);
        if (v != nullptr) {
            if (argsMap.count(v->src)) {
                if (read)effects.readByPointerArg.insert(argsMap[v->src]);
                if (write)effects.writeByPointerArg.insert(argsMap[v->src]);
            } else if (v->src->getLocation() == Value::Location::STATIC) {
                if (read)effects.readGlobalVariable.insert(v->src);
                if (write)effects.writeGlobalVariable.insert(v->src);
            }
        }
    }

    void operateValuePtr(ValueAddressingMIR *mir, bool read, bool write) {
        if (mir->base->getLocation() == Value::Location::STATIC &&
        mir->base->getType()->getId() != Type::ID::POINTER) {
            auto *v = (Variable *) mir->base;
            if (read)effects.readGlobalVariable.insert(v);
            if (write)effects.writeGlobalVariable.insert(v);
        } else operatePtr(mir->base, read, write);
    }

    void operateArr(ArrayAddressingMIR *mir, bool read, bool write) {
        if (mir->base->getLocation() == Value::Location::STATIC &&
        mir->base->getType()->getId() != Type::ID::POINTER) {
            auto *v = (Variable *) mir->base;
            if (read)effects.readGlobalVariable.insert(v);
            if (write)effects.writeGlobalVariable.insert(v);
        } else operatePtr(mir->base, read, write);
    }

    void visit(CallMIR *mir) override {
        for (int i = 0; i < mir->args.size(); i++) {
            auto arg = mir->args[i];
            if (arg->getType()->getId() != Type::ID::POINTER) continue;
            bool read = sideEffects[mir->func]->readByPointerArg.count(i);
            bool write = sideEffects[mir->func]->writeByPointerArg.count(i);
            if (read || write)operatePtr(arg, read, write);
        }
    }

    void visit(CallWithAssignMIR *mir) override {
        returnDataFlow[mir->func].insert(thiz);
        visit(static_cast<CallMIR *>(mir));
    }

    void visit(MultiCallMIR *mir) override {
        visit(static_cast<CallMIR *>(mir));
    }
};

bool AnalyzeSideEffectPass::analyze() {
    analyzeCallGraphPass.run();
    for (auto *func: md.getFunctionSetPtrOrder()) {
        auto *sideEffect = new FunctionSideEffect();
        sideEffects[func] = sideEffect;
        //sideEffects[func]->callExternalFunction = std::move(analyzeCallGraphPass.callGraph[func]);

        //函数内语句的影响
        auto visitor = VisitorSelf(*sideEffects[func], func);
        for (auto *b: func->getBasicBlockSetPtrOrder()) {
            for (auto &mir : b->mirTable) {
                mir->accept(visitor);
            }
        }
    }

    //设置外部函数
    sideEffects[md.getFunctionByName("getarray")]->writeByPointerArg.insert(0);
    sideEffects[md.getFunctionByName("putarray")]->readByPointerArg.insert(1);
    sideEffects[md.getFunctionByName("getarray")]->returnConstant = false;
    sideEffects[md.getFunctionByName("getch")]->returnConstant = false;
    sideEffects[md.getFunctionByName("getint")]->returnConstant = false;


    //强连通缩点
//    Tarjan_SCC_SideEffect tarjan = {analyzeCallGraphPass.callGraph};
//    tarjan.solve();
//    std::map<Function *, int> &belongs = tarjan.belong;
//    std::map<int, std::set<Function *>> &scc = tarjan.SCC;
//    std::map<int, FunctionSideEffect *> sccEffects;
    std::map<Function *, std::set<Function *>> dag = {};
    std::map<Function *, int> in = {};
    //建立DAG，合并SCC内的SideEffect
    for (auto &x : analyzeCallGraphPass.callGraph) {
        if (!dag.count(x.first))dag[x.first] = {};
        if (!in.count(x.first))in[x.first] = 0;
        for (Function *y: x.second) {
            if (y == x.first) continue;
            dag[x.first].insert(y);
            if (!in.count(y))in[y] = 0;
            in[y]++;
        }
    }

    //如果存在递归就把所有数组变量置为已经读写
    for (auto *f : md.getFunctionSetPtrOrder()) {
        if (analyzeCallGraphPass.recursive(f)) {
            for (int i = 0; i < f->args.size(); ++i) {
                if (f->args[i]->getType()->getId() != Type::ID::POINTER) continue;
                sideEffects[f]->readByPointerArg.insert(i);
                sideEffects[f]->writeByPointerArg.insert(i);
            }
        }
        if (f->isExternal) {
            sideEffects[f]->callExternalFunction.insert(f);
        }
    }
    //拓扑排序
    std::vector<Function *> topoSeq;
    std::queue<Function *> q;
    for (auto &p: in) {
        if (p.second == 0) q.push(p.first);
    }
    while (!q.empty()) {
        Function *u = q.front();
        q.pop();
        topoSeq.push_back(u);
        funcTopoSeq.push_back(u);
        for (Function *v : dag[u]) {
            in[v]--;
            if (in[v] == 0) q.push(v);
        }
    }
    //合并SCC之间的SideEffect
    for (int i = (int) topoSeq.size() - 1; i >= 0; i--) {
        for (auto j : dag[topoSeq[i]]) {
            //传递全局变量读写
            mergeInto(sideEffects[j], sideEffects[topoSeq[i]]);
        }
        //传递函数变量读写、返回值使用信息
        auto *f = topoSeq[i];
        auto visitor = VisitorCall(sideEffects, f, returnDataFlow);
        for (auto *b: f->getBasicBlockSetPtrOrder()) {
            for (auto &mir : b->mirTable) {
                mir->accept(visitor);
            }
        }
    }
    for (auto *f : md.getFunctionSetPtrOrder()) {
        sideEffects[f]->returnConstant = sideEffects[f]->returnConstant && sideEffects[f]->returnValue != nullptr;
    }
    //测试
//    for (auto *f : md.getFunctionSetPtrOrder()) {
//        std::cerr << f->getName() << ":" << std::endl;
//        std::cerr << "return cnst:" << sideEffects[f]->returnConstant << std::endl;
//        if (sideEffects[f]->returnConstant)std::cerr << sideEffects[f]->returnValue->getValue<int>() << std::endl;
//        for (auto *ff : sideEffects[f]->callExternalFunction) {
//            std::cerr << ff->getName() << ' ';
//        }
//        std::cerr << std::endl << "readArg:";
//        for (auto ff : sideEffects[f]->readByPointerArg) {
//            std::cerr << ff << ' ';
//        }
//        std::cerr << std::endl << "writeArg:";
//        for (auto ff : sideEffects[f]->writeByPointerArg) {
//            std::cerr << ff << ' ';
//        }
//        std::cerr << std::endl << "readGlobal:";
//        for (auto ff : sideEffects[f]->readGlobalVariable) {
//            std::cerr << ff->getName() << ' ';
//        }
//        std::cerr << std::endl << "writeGlobal:";
//        for (auto ff : sideEffects[f]->writeGlobalVariable) {
//            std::cerr << ff->getName() << ' ';
//        }
//        std::cerr << std::endl;
//        std::cerr << std::endl;
//    }
    return true;
}

void AnalyzeSideEffectPass::invalidate() {
    AnalyzePass::invalidate();
    for (auto &x: sideEffects) {
        auto &sideEffect = *x.second;
        sideEffect.readGlobalVariable.clear();
        sideEffect.writeGlobalVariable.clear();
        sideEffect.readByPointerArg.clear();
        sideEffect.writeByPointerArg.clear();
        sideEffect.callExternalFunction.clear();
        delete x.second;
    }
    sideEffects.clear();
    returnDataFlow.clear();
    funcTopoSeq.clear();
}
