//
// Created by tcyhost on 2021/6/30.
//

#ifndef COMPILER2021_GRAMMARNODE_H
#define COMPILER2021_GRAMMARNODE_H

#include "../tokenize/Token.h"

#include <memory>
#include <utility>
#include <vector>

namespace frontend::grammar_parse {
    using std::shared_ptr;
    using std::string;
    using std::vector;
    using namespace frontend::tokenize;

    enum class G_NODE_TYPE {
        COMP_UNIT, DECL, FUNC_DEF,
        CONST_DECL, VAR_DECL, CONST_DEF, VAR_DEF,
        CONST_INIT, VAR_INIT, CONST_EXPR,
        FUNC_TYPE, FUNC_F_PARAM, FUNC_F_PARAM_TABLE, BLOCK, BLOCK_ITEM,
        STMT, COND, EXPR, IF_STMT, WHILE_STMT,
        ADD_EXPR, MUL_EXPR, UNARY_EXPR, PRIMARY_EXPR,
        LOR_EXPR, LAND_EXPR, EQ_EXPR, REL_EXPR,
        L_VAL, FUNC_R_PARAM_TABLE, UNARY_OP,
        TOKEN
    };

    class GNode {
        G_NODE_TYPE type;

    public:
        void setType(G_NODE_TYPE _type) { type = _type; }

        G_NODE_TYPE getType() { return type; }

        virtual string toString();

        virtual string toOutput();

        [[maybe_unused]] virtual void addChild(const shared_ptr<GNode> child) {
            // do nothing
        }

    };

    typedef shared_ptr<GNode> GNodeSPtr;

    class LGNode : public GNode {
        Token tok;

    public:
        explicit LGNode(Token _tok) : tok(std::move(_tok)) {
            GNode::setType(G_NODE_TYPE::TOKEN);
        }

        string toString() override;

        string toOutput() override;
    };

    class BGNode : public GNode {
        vector<shared_ptr<GNode>> children;

    public:
        explicit BGNode(G_NODE_TYPE _type) {
            GNode::setType(_type);
        }

        void addChild(const shared_ptr<GNode> child) override {
            children.push_back(child);
        }

        string toString() override; // 用于显示自身语法成分
        string toOutput() override; // 递归输出整棵语法树
    };
}
#endif //COMPILER2021_GRAMMARNODE_H
