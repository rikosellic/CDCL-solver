#pragma once
#include "cnf.hpp"
#include <iterator>
#include <sstream>
#include <valarray>
#include <vector>
#include <map>
#include <stack>
#include <set>
#include <list>
#include <queue>

struct ClauseWrapper;

class VariableWrapper;

class ImpNode;

class ImpRel;

class ImpGraph;

struct Assign;

struct CDCL;

struct CRef;

using Variable = int;

enum Value
// Value: Possible value of a variable
{
    Free,
    True,
    False
};

struct Assign
// Assign: an object reflect an assignment to a single variable
{
    int variable_index; // The index of the variable
    Value value;        // The value of the variable
};

class Lit
{
    int lit;

public:
    Lit(int, bool);
    int get_var();
    bool is_neg();

    bool operator==(const Lit &a) { return a.lit == lit; }
    bool operator!=(const Lit &a) { return a.lit != lit; }
};

class VariableWrapper
{
    Variable var;
    CDCL &cdcl;
    std::list<CRef> pos_watcher, neg_watcher;

public:
    VariableWrapper(Variable, CDCL &);
    CRef update_watchlist(Assign);
    std::list<CRef> &get_watchlist(Assign);
    void watchlist_pushback(CRef, Lit);
    Value get_value();
};

const Lit nullLit = Lit(0, true);

struct ClauseWrapper
// ClauseWrapper: Object wraps a raw clause
// that contains useful information in DCL solving
{
    int index;

    std::vector<Lit> literals;

    /*
     * ClauseWrapper::find_pure_lit(): Find a pure literal in clause and return
     * its ptr Pure literal: The only literal that hasn't been picked in a
     * unsatisfied yet clause Return Value: ptr to the literal when pure literal
     * exists. Otherwise nullptr
     */
    Lit update_watcher(Variable, CDCL &);

    Lit get_blocker(Variable);

    /*
     * ClauseWrapper::update():
     * Update this.lits_value this.satisfied and this.picked_lits_number
     * according to the given assign
     * Return Value:
     * true when the given assign triggers a conflict in the clause.
     * Otherwise return false
     */
    bool update(Assign);

    bool is_unit(Lit, CDCL *);

    ClauseWrapper(Clause &,
                  int); // Initialize a new ClauseWrapper with raw clause
    void drop();

    void debug();
};

class ClauseDatabase
{
    friend class LRef;
    friend class CRef;
    std::vector<Lit> literal;
    std::vector<ClauseWrapper> clause;

public:
    ClauseDatabase(CNF &);
    void parse();
    void add_clause(Clause);
    CRef get_clause(int);
    CRef get_last_cls();
    int size();
};

struct CRef
{
    int cid;
    ClauseDatabase &db;

    CRef(int, ClauseDatabase &);
    bool isnull();

    ClauseWrapper &operator*() { return db.clause.at(cid); }
    bool operator==(const CRef &a) { return cid == a.cid; }
    bool operator!=(const CRef &a) { return cid != a.cid; }
    void operator=(const CRef &a)
    {
        cid = a.cid;
        db = a.db;
        return;
    }
};

class ImpRelation
// ImpRelation: Object describe a edge in the CDCL's Implication graph
{
    CRef relation_clause;
    // The clause according to which this->conclusion holds if this->premise
    // holds
    ImpNode *premise;
    ImpNode *conclusion;

public:
    void drop();
    ImpNode *get_conclusion(), *get_premise();
    ImpRelation(CRef, ImpNode *, ImpNode *);
};

class ImpGraph
// ImpGraph: object that describe a CDCL Implication graph
{
    CDCL *cdcl;
    // Pointing to CDCL object the graph belongs to
    // std::vector<ImpNode *> fixed_var_nodes;
    // std::vector<ImpRelation *> relations;
    std::vector<ImpNode *> vars_to_nodes;
    std::vector<std::vector<ImpNode *>> trail;
    std::vector<int> assigned_order;
    // Map from variable to node. If a variable is not assigned (i.e.
    // Value::Free), then map_from_vars_to_nodes[var_index] = nullptr.

public:
    void init(CDCL *);

    /*
     * ImpGraph::construct(std::pair<clause, literal>):
     * Will construct a new node when we assign value to a variable
     * Input: std::pair of a ClauseWrapper and Literal object,
     * The second is the newly assigned variable and the first is the clause by
     * which the variable's value is determined.
     */
    void pick_var(CRef, Lit);

    /*
     * ImpGraph::add_node(assign, rank):
     * Add a new node to the implcation graph with the given assign and rank
     * Return Value:
     * ptr pointing to the node
     */
    ImpNode *add_node(Assign, int);

    /*
     * ImpGraph::add_rel(premise, conclusion, clause):
     * Add a new edge to the implcation graph with the given two nodes
     * (premise and conclusion) and the clause
     * Return Value:
     * ptr pointing to the edge
     */

    /*
     * ImpGraph::conflict_clause_gen(clause):
     * If the clause given cause a conflict, this function will generate a
     * conflict raw clause and return it.
     * Attention: When the given clause do not cause a conflict, this function's
     * behavior is undefined. If the variable whose value is Value::Free is
     * encountered while calculating, the whole program will abort()
     * Return Value:
     * The generated raw clause
     */
    Clause conflict_clause_gen(CRef);

    /*
     * ImpGraph::drop_to(rank)
     * This will drop all nodes with ranks bigger or equal to parameter 'rank'
     * also drop relevant edge
     */
    void drop_to(int);

    /*
     * ImpNode::drop()
     * drop all nodes and edges
     */
    void drop();
    void add_reason(ImpNode *, ImpNode *, CRef);

    void debug();
};

class ImpNode
// ImpNode: Object describe a node in the CDCL's Implication graph
{
    std::vector<ImpNode *> in_node, out_node;
    std::vector<CRef> in_reason, out_reason;
    // Set of edges. Either conclusion or premise in these ImpRelation objects
    // is self
    Assign assign;
    int rank;
    // rank: describe when a node is generated
    // More accurately, the rank of a node depends on the number of the picked
    // variable when it is generated
    // rank is useful when we need to drop some node in the graph due to a new
    // generated clause
    bool fixed = false;

public:
    ImpNode(Assign, int, bool = false);
    friend void ImpGraph::add_reason(ImpNode *, ImpNode *, CRef);
    int get_rank();
    int get_var_index();
    Assign get_assign();

    const std::vector<ImpNode *> &get_in_node();

    void drop();
    void debug();
};

struct CDCL
{
    int variable_number, clause_size;
    ClauseDatabase clausedb;
    std::vector<VariableWrapper> vars;
    std::vector<Assign> assignment;
    std::stack<Assign> pick_stack;
    std::queue<std::pair<Lit, CRef>> unchecked_queue;

    std::vector<int> vars_rank;

    CRef confl, nullCRef;

    ImpGraph *graph = nullptr;
    bool satisfiable = false, solved = false;
    // variable CDCL::solved will be set true when CDCL::solve() is called
    // CDCL::satisfiable implies CDCL::solved

    CDCL(CNF &);
    ~CDCL();

    void analyze(CRef);

    void solve();
    /*
     * CDCL::update(assign, do_return)
     * update every clauses and conflict clauses in the CDCL object
     * When do_return is set to true, the function will return once a conflict
     * is encountered in updating process. Otherwise will update all clause
     * regardless of conflict
     * Return Value: std::pair<ClauseWrapper*, bool>
     * If conflict is encountered then return pair<ptr to conflict clause, true>
     * Else return pair<nullptr, false>
     */
    CRef update(Assign);

    /*
     * CDCL::add_clause(clause)
     * add a clause to the CDCL clause set. This method will update the member
     * 'vars_contained_clause' and, if the new clause is pickable, add the
     * clause to pickable_clause.
     */
    void add_clause(ClauseWrapper *, std::vector<ClauseWrapper *> &);

    /*
     * CDCL::unit_propogation():
     * Do unit propagation. If there is conflict automatically generate conflict
     * clause and push it into self::conflict_clause.
     * Return Value:
     * The first element of std::pair indicate which variables are assigned in
     * propagation The second element of std::pair indicate whether there is a
     * conflict
     */
    bool unit_propogation();

    /*
     * CDCL::choose_variable()
     * Choose a variable that is not picked yet and return a ptr to it.
     * Return Value:
     * ptr to the selected variable's literal
     */
    Lit choose_variable();

    Lit insert_literal(Variable, bool);

    Value get_lit_value(Lit);
    void init(CNF &);

    void debug();

    void print();
    void print_dimacs();
    void stream_dimacs(std::ostringstream &, std::ostringstream &);
};