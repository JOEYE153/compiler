//
// Created by hujin on 2021/7/28.
//

#include <stack>
#include <queue>
#include <cassert>
#include "AnalyzeRegionPass.h"

void printNode(int d, AnalyzeRegionPass::NodeMax *node, ostream &out);

void printNode(int d, AnalyzeRegionPass::NodeMin *node, ostream &out) {
    for (int i = 0; i < d; ++i) {
        out << "\t";
    }
    out << "nodeMin--" << "ent:" << node->entrance->getName() << " exits:" << node->exit->getName() << std::endl;
    for (auto *n : node->children) printNode(d + 1, n, out);
}

void printNode(int d, AnalyzeRegionPass::NodeMax *node, ostream &out) {
    for (int i = 0; i < d; ++i) {
        out << "\t";
    }
    out << "nodeMax--" << node->entrance->getName() << " exits:" << node->exit->getName() << " depth:" << node->depth
        << std::endl;
    for (auto *n : node->children) printNode(d + 1, n, out);
}


//#define TEST

bool AnalyzeRegionPass::analyze() {
    endBlock = new BasicBlock("End");
    analyzeCfgPass.run();
    analyzeCfgPass.dfsSequence.clear();
    analyzeCfgPass.remainBlocks = fn.getBasicBlockSetPtrOrder();
    analyzeCfgPass.remainBlocks.insert(endBlock);
    dfs_build_cfg_with_end(fn.entryBlock, nullptr);
    analyzeCfgPass.result[endBlock].rear = {};
    analyzeDomTreePass.invalidate();
    analyzeDomTreePass.run();
    reverse_cfg();
    analyzeDomTreePass_rev->run();
    analyzeCfgPass.invalidate();
    //撤回对analyzeCfgPass的改动
    analyzeCfgPass.run();

    DomTreeNode *dom_root = analyzeDomTreePass.root;
    dfs_depth(dom_root, 0);
    dfs_rev(analyzeDomTreePass_rev->root, 0);
    dfs_max_route(dom_root);
    dfs_build_path(dom_root);
    dfs_link_max_min(dom_root, nullptr);
    root = max_single_path[dom_root->block];
    for (auto x: analyzeCfgPass.dfsSequence) {
        if (leaves[x]->father->depth == 0 && leaves[x]->father->father != nullptr) {
            leaves[x]->father->depth = leaves[x]->father->father->father->depth + 1;
        }
    }

#ifdef TEST

    for (auto &x : analyzeCFGPass.result) {
        for (auto &y : x.second.rear) {
            out << x.first->getName() << "->" << y->getName() << std::endl;
        }
    }
    out << "Dom:" << std::endl;
    analyzeDomTreePass.printTree(analyzeDomTreePass.root);
    out << "Rev_Dom:" << std::endl;
    analyzeDomTreePass_rev->printTree(analyzeDomTreePass_rev->root);
    printNode(0, root, out);
#endif
    return true;
}

void deleteNode(AnalyzeRegionPass::NodeMax *node);

void deleteNode(AnalyzeRegionPass::NodeMin *node) {
    for (auto *n : node->children) deleteNode(n);
    delete node;
}

void deleteNode(AnalyzeRegionPass::NodeMax *node) {
    for (auto *n : node->children) deleteNode(n);
    delete node;
}

void AnalyzeRegionPass::invalidate() {
    AnalyzeFunctionPass::invalidate();
    if (root != nullptr) {
        deleteNode(root);
        root = nullptr;
    }
    max_single_path.clear();
    min_single_path.clear();
    max_route.clear();
    depth.clear();
    depth_rev.clear();
    leaves.clear();
    revFather.clear();
    no_single_path_blocks.clear();
    analyzeCfgPass.invalidate();
    analyzeDomTreePass.invalidate();
    analyzeDomTreePass_rev->invalidate();

}

void AnalyzeRegionPass::reverse_cfg() {
    for (auto &node : analyzeCfgPass.result) {
        std::vector<BasicBlock *> temp = node.second.rear;
        node.second.rear.clear();
        for (BasicBlock *b : node.second.prev) {
            node.second.rear.push_back(b);
        }
        node.second.prev.clear();
        for (BasicBlock *b : temp) {
            node.second.prev.insert(b);
        }
    }
    analyzeCfgPass.dfsSequence.clear();
    analyzeCfgPass.remainBlocks = fn.getBasicBlockSetPtrOrder();
    analyzeCfgPass.remainBlocks.insert(endBlock);
    dfs_build_reverse_cfg(endBlock, nullptr);
}

void AnalyzeRegionPass::dfs_build_cfg_with_end(BasicBlock *block, BasicBlock *father) {
    auto iter = analyzeCfgPass.remainBlocks.find(block);
    if (iter == analyzeCfgPass.remainBlocks.end()) {
        return;
    }
    analyzeCfgPass.result[block].block = block;
    analyzeCfgPass.result[block].father = father;
    analyzeCfgPass.remainBlocks.erase(iter);
    analyzeCfgPass.dfsSequence.push_back(block);
    if (block == endBlock) {
        return;
    }
    auto rearVec = analyzeCfgPass.result[block].rear;
    if (rearVec.empty()) {
        analyzeCfgPass.result[block].rear = {endBlock};
    }
    for (auto rear : analyzeCfgPass.result[block].rear) {
        dfs_build_cfg_with_end(rear, block);
        analyzeCfgPass.result[rear].prev.insert(block);
    }
}

void AnalyzeRegionPass::dfs_build_reverse_cfg(BasicBlock *block, BasicBlock *father) {
    auto iter = analyzeCfgPass.remainBlocks.find(block);
    if (iter == analyzeCfgPass.remainBlocks.end()) {
        return;
    }
    analyzeCfgPass.result[block].block = block;
    analyzeCfgPass.result[block].father = father;
    analyzeCfgPass.remainBlocks.erase(iter);
    analyzeCfgPass.dfsSequence.push_back(block);
    auto &rearVec = analyzeCfgPass.result[block].rear;
    for (auto rear : rearVec) {
        dfs_build_reverse_cfg(rear, block);
    }
}


void AnalyzeRegionPass::dfs_depth(DomTreeNode *b, int d) {
    depth[b->block] = d;
    for (auto &x : b->children) {
        dfs_depth(x.get(), d + 1);
    }
}

void AnalyzeRegionPass::dfs_rev(DomTreeNode *b, int d) {
    DomTreeNode *pNode = b->father;
    depth_rev[b->block] = d;
    if (pNode != nullptr)revFather[b->block] = pNode->block;
    else revFather[b->block] = nullptr;
    for (auto &x : b->children) {
        dfs_rev(x.get(), d + 1);
    }
}

int AnalyzeRegionPass::dfs_max_route(DomTreeNode *b) {
    if (b->children.empty()) {
        max_route[b] = nullptr;
        return depth_rev[b->block];
    }
    int argmin = depth_rev[b->block];
    DomTreeNode *r = nullptr;
    for (auto &x : b->children) {
        int d = dfs_max_route(x.get());
        if (d < argmin) {
            argmin = d;
            r = x.get();
        }
    }
    max_route[b] = r;
    return argmin;
}

void AnalyzeRegionPass::createSingleNode(BasicBlock *block) {
    auto *nodeMin = new AnalyzeRegionPass::NodeMin();
    auto *nodeMax = new AnalyzeRegionPass::NodeMax();
    nodeMin->entrance = nodeMin->exit = nodeMax->entrance = nodeMax->exit = block;
    min_single_path[block] = nodeMin;
    max_single_path[block] = nodeMax;
    leaves[block] = nodeMin;
    nodeMin->setFather(nodeMax);
}

void AnalyzeRegionPass::createSingleNode(BasicBlock *block, NodeMin *father) {
    auto *nodeMin = new AnalyzeRegionPass::NodeMin();
    auto *nodeMax = new AnalyzeRegionPass::NodeMax();
    nodeMin->entrance = nodeMin->exit = nodeMax->entrance = nodeMax->exit = block;
    nodeMin->setFather(nodeMax);
    nodeMax->setFather(father);
    leaves[block] = nodeMin;
}

void AnalyzeRegionPass::dfs_build_path(DomTreeNode *b) {
    if (b->children.empty()) {
        //没有大小大于自身的极大极小区间
        createSingleNode(b->block);
        return;
    }
    int depth_rev_father = depth[revFather[b->block]];
    int depth_r = depth[b->block];
    NodeMin *nodeMin = nullptr;
    if (depth_rev_father > depth_r) {
        //revFather必为b的子树节点*[1]
        nodeMin = new AnalyzeRegionPass::NodeMin();
        nodeMin->entrance = b->block;
        nodeMin->exit = revFather[b->block];
        min_single_path[b->block] = nodeMin;
        createSingleNode(b->block, nodeMin);
    } else {
        //没有大小大于自身的极大极小区间
        no_single_path_blocks.insert(b->block);
        createSingleNode(b->block);
    }
    for (auto &x : b->children) {
        dfs_build_path(x.get());
        if (max_route[b] == x.get() && depth_rev_father > depth_r) {
            //存在后继极小节点。这时max_route[b]应表示后继极小区间
            nodeMin->next = min_single_path[x->block];
            assert(nodeMin->exit == min_single_path[x->block]->entrance);
        }
    }
    if (nodeMin != nullptr && nodeMin->next != nullptr) {
        //存在后继极小区间
        NodeMax *maxPath = max_single_path[nodeMin->next->entrance];
        maxPath->entrance = nodeMin->entrance;
        nodeMin->setFather(maxPath);
        max_single_path[b->block] = maxPath;
    } else if (!max_single_path.count(b->block)) {
        //不存在后继极小区间,且满足条件*[1]
        auto *maxPath = new NodeMax();
        maxPath->entrance = b->block;
        maxPath->exit = nodeMin->exit;
        nodeMin->setFather(maxPath);
        max_single_path[b->block] = maxPath;
    }
}

void AnalyzeRegionPass::dfs_link_max_min(DomTreeNode *b, NodeMin *father) {
    //max_single_path的子节点应已经被建立完毕
    if (father != nullptr) {
        max_single_path[b->block]->setFather(father);
        max_single_path[b->block]->depth = father->father->depth + 1;
    }
    for (auto &x : b->children) {
        if (min_single_path[b->block]->next == min_single_path[x->block] || no_single_path_blocks.count(b->block)) {
            //x为b极小区间的后继极小区间，其max_single_path与b相同
            dfs_link_max_min(x.get(), father);
        } else {
            dfs_link_max_min(x.get(), min_single_path[b->block]);
        }
    }
}
