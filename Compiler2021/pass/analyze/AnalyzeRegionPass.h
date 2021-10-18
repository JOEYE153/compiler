//
// Created by hujin on 2021/7/28.
//

#ifndef COMPILER2021_ANALYZEREGIONPASS_H
#define COMPILER2021_ANALYZEREGIONPASS_H

#include "../Pass.h"
#include "AnalyzeCFGPass.h"
#include "AnalyzeDomTreePass.h"

/*
 * 查找单输入单输出区间，复杂度O(V * log V)
 */
class AnalyzeRegionPass : public AnalyzeFunctionPass {
public:

    struct NodeMax;

    //极小单输入单输出区间。不可被分割为->A->B->形式的两个单输入单输出区间AB的区间。
    //以每个Block为入口的极小区间至少有一个，即其本身，至多有两个。
    struct NodeMin {
        BasicBlock *entrance = nullptr; //区间入口
        BasicBlock *exit = nullptr; //区间出口, next!=nullptr时极大极小区间的区间出口为next的入口
        NodeMax *father = nullptr;
        std::set<NodeMax *> children = {};
        NodeMin *next = nullptr;//同属于一个极大区间下一个极小区间, 不存在则为null。
        void setFather(NodeMax *f) {
            father = f;
            f->children.insert(this);
        };
    };

    //极大单输入单输出区间。由极小区间序列-->A-...->B-->组成极大序列
    struct NodeMax {
        BasicBlock *entrance = nullptr; //区间入口
        BasicBlock *exit = nullptr; //区间出口
        NodeMin *father = nullptr;
        std::set<NodeMin *> children = {};
        int depth = 0;

        void setFather(NodeMin *f) {
            father = f;
            f->children.insert(this);
        };
    };

    AnalyzeRegionPass(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out) :
            AnalyzeFunctionPass(fn, out),
            analyzeCfgPass(*dependency.analyzeCFGPass), analyzeDomTreePass(*dependency.analyzeDomTreePass) {
        analyzeDomTreePass_rev = new AnalyzeDomTreePass(fn, dependency, out);
    };


    AnalyzeCFGPass &analyzeCfgPass;
    AnalyzeDomTreePass &analyzeDomTreePass, *analyzeDomTreePass_rev;
    NodeMax *root = nullptr;
    BasicBlock *endBlock = nullptr;
    std::map<BasicBlock *, NodeMin *> leaves = {};//每个block的极小极小区间


    void invalidate() override;

protected:

/* 计算支配树和反向支配树
 * 问题为对于B1，找到相应的【对应点】B2满足:
 *    a.支配树上B1为B2的祖先 b.反向支配树上B2为B1的祖先
 * 在支配树上：
 * 1. 支配树叶子节点B1的【对应点】B2 = B1
 * 2. 设B为B1在反向支配树上的父节点，支配树上：
 *      若B为B1祖先，则B1与B构成闭环，B深度<B1
 *      若B不为B1祖先，且B1不为B祖先，设公共祖先为A，则由B反向支配B1，
 *          得到CFG上存在A..->X..->B,A..->B1..->B, 且X为A在支配树上的子节点
 *          则支配树上A为B的直接祖先，B深度≤B1
 *      因此若B在支配树上的深度>B1, 则B1为B祖先, 则B1【对应点】B2=B，满足条件ab
 *      若B≠B1且在支配树上的深度≤B1，则B1不存在大于1的单输入输出区间，令B1【对应点】B2=B1
 * 所得到B1的对应点表示以B1点为开始的极大极小区间
 * 串联后得到极大区间。
 * */
    bool analyze() override;


private:


    void dfs_build_cfg_with_end(BasicBlock *block, BasicBlock *father);

    void dfs_build_reverse_cfg(BasicBlock *block, BasicBlock *father);

    void reverse_cfg();

    void dfs_depth(DomTreeNode *b, int d);

    void dfs_rev(DomTreeNode *b, int d);

    std::map<BasicBlock *, BasicBlock *> revFather;
    std::map<BasicBlock *, int> depth;
    std::map<BasicBlock *, int> depth_rev;
    std::set<BasicBlock *> no_single_path_blocks;

    //max_route[x] = y 表示支配树上y为x子节点且反向支配树上y为x的祖先[1]，满足这样条件深度最小的y
    //y必为【以x为入口的极大极小区间】的后继极大极小区间节点的入口
    //同样地，若y为【以x为入口的极大极小区间】的后继极大极小区间的入口，则max_route[x] = y。
    //对于每一个x可能不存在y与之对应，这时记max_route = null
    std::map<DomTreeNode *, DomTreeNode *> max_route;
    std::map<BasicBlock *, NodeMin *> min_single_path;//以block为入口的极大Node_min
    std::map<BasicBlock *, NodeMax *> max_single_path;//以block为入口的极大Node_min的父节点

    //若支配树上y为x子节点，必有x-->y路径，因此反向支配树上y支配x 或 y的idom支配x
    //因此若y在反向支配树上深度小于x，那么满足条件1。
    int dfs_max_route(DomTreeNode *b);

    void dfs_build_path(DomTreeNode *b);

    void dfs_link_max_min(DomTreeNode *b, NodeMin *father);

    void createSingleNode(BasicBlock *block);

    void createSingleNode(BasicBlock *block, NodeMin *father);
};

typedef AnalyzeRegionPass::NodeMax RegionMax;
typedef AnalyzeRegionPass::NodeMin RegionMin;

#endif //COMPILER2021_ANALYZEREGIONPASS_H
