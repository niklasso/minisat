
#include "ccnr.h"

using namespace CCNR;
using namespace std;


//-------------------
// in mersenne.cc
const int M = 397;
const unsigned int MATRIX_A = 0x9908b0dfUL;
const unsigned int UPPER_MASK = 0x80000000UL;
const unsigned int LOWER_MASK = 0x7fffffffUL;

//---------------------------
// functions in mersenne.h & mersenne.cpp

/*
  Notes on seeding

  1. Seeding with an integer
     To avoid different seeds mapping to the same sequence, follow one of
     the following two conventions:
     a) Only use seeds in 0..2^31-1     (preferred)
     b) Only use seeds in -2^30..2^30-1 (2-complement machines only)

  2. Seeding with an array (die-hard seed method)
     The length of the array, len, can be arbitrarily high, but for lengths greater
     than N, collisions are common. If the seed is of high quality, using more than
     N values does not make sense.
*/
Mersenne::Mersenne() { seed((int)std::time(0)); }
Mersenne::Mersenne(int s) { seed(s); }
Mersenne::Mersenne(unsigned int *init_key, int key_length) { seed(init_key, key_length); }
Mersenne::Mersenne(const Mersenne &copy) { *this = copy; }
Mersenne &Mersenne::operator=(const Mersenne &copy)
{
    for (int i = 0; i < N; i++) mt[i] = copy.mt[i];
    mti = copy.mti;
    return *this;
}
void Mersenne::seed(int se)
{
    unsigned int s = ((unsigned int)(se << 1)) + 1;
    // Seeds should not be zero. Other possible solutions (such as s |= 1)
    // lead to more confusion, because often-used low seeds like 2 and 3 would
    // be identical. This leads to collisions only for rarely used seeds (see
    // note in header file).
    mt[0] = s & 0xffffffffUL;
    for (mti = 1; mti < N; mti++) {
        mt[mti] = (1812433253UL * (mt[mti - 1] ^ (mt[mti - 1] >> 30)) + mti);
        mt[mti] &= 0xffffffffUL;
    }
}
void Mersenne::seed(unsigned int *init_key, int key_length)
{
    int i = 1, j = 0, k = (N > key_length ? N : key_length);
    seed(19650218UL);
    for (; k; k--) {
        mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1664525UL)) + init_key[j] + j;
        mt[i] &= 0xffffffffUL;
        i++;
        j++;
        if (i >= N) {
            mt[0] = mt[N - 1];
            i = 1;
        }
        if (j >= key_length) j = 0;
    }
    for (k = N - 1; k; k--) {
        mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1566083941UL)) - i;
        mt[i] &= 0xffffffffUL;
        i++;
        if (i >= N) {
            mt[0] = mt[N - 1];
            i = 1;
        }
    }
    mt[0] = 0x80000000UL;
}
unsigned int Mersenne::next32()
{
    unsigned int y;
    static unsigned int mag01[2] = { 0x0UL, MATRIX_A };
    if (mti >= N) {
        int kk;
        for (kk = 0; kk < N - M; kk++) {
            y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
            mt[kk] = mt[kk + M] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        for (; kk < N - 1; kk++) {
            y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
            mt[kk] = mt[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        y = (mt[N - 1] & UPPER_MASK) | (mt[0] & LOWER_MASK);
        mt[N - 1] = mt[M - 1] ^ (y >> 1) ^ mag01[y & 0x1UL];
        mti = 0;
    }
    y = mt[mti++];
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);
    return y;
}
int Mersenne::next31() { return (int)(next32() >> 1); }
double Mersenne::nextClosed()
{
    unsigned int a = next32() >> 5, b = next32() >> 6;
    return (a * 67108864.0 + b) * (1.0 / 9007199254740991.0);
}
double Mersenne::nextHalfOpen()
{
    unsigned int a = next32() >> 5, b = next32() >> 6;
    return (a * 67108864.0 + b) * (1.0 / 9007199254740992.0);
}
double Mersenne::nextOpen()
{
    unsigned int a = next32() >> 5, b = next32() >> 6;
    return (0.5 + a * 67108864.0 + b) * (1.0 / 9007199254740991.0);
}
int Mersenne::next(int bound)
{
    unsigned int value;
    do {
        value = next31();
    } while (value + (unsigned int)bound >= 0x80000000UL);
    // Just using modulo doesn't lead to uniform distribution. This does.
    return (int)(value % bound);
}
// functions in ls.cpp & ls.h
//-----------------------
// constructor with default setting.
ls_solver::ls_solver()
  : _num_vars(0)
  , _num_clauses(0)
  , _best_found_cost(0)
  , _best_cost_time(0)
  , _step(0)
  , _aspiration_score(0)
  , _avg_clause_weight(0)
  , _delta_total_clause_weight(0)
  , _init_unsat_nums(0)
{
    _additional_len = 10;
    _max_tries = 1;                 // 100
    _max_steps = 900 * 1000 * 1000; // 2 0000 0000
    _max_mems = 100 * 1000 * 1000;
    _random_seed = 1;
    // _time_limit = 3000;
    _swt_threshold = 50;
    _swt_p = 0.3;
    _swt_q = 0.7;
    _aspiration_active = true;
    _mems = 0;
    _prob_p = 0.001;
}
/******************************the top function******************************/
/**********************************build instance*******************************/
bool ls_solver::make_space()
{
    if (0 == _num_vars || 0 == _num_clauses) {
        cout << "c the formula size is zero. You may forgot to read the formula." << endl;
        return false;
    }
    // _vars.reserve(_num_vars+_additional_len);
    // _clauses.reserve(_num_clauses+_additional_len);
    // _solution.reserve(_num_vars+_additional_len);
    // _index_in_unsat_clauses.reserve(_num_clauses+_additional_len);
    // _index_in_unsat_vars.reserve(_num_vars+_additional_len);
    _vars.resize(_num_vars + _additional_len);
    _clauses.resize(_num_clauses + _additional_len);
    _solution.resize(_num_vars + _additional_len);
    _best_solution.resize(_num_vars + _additional_len);
    _index_in_unsat_clauses.resize(_num_clauses + _additional_len);
    _index_in_unsat_vars.resize(_num_vars + _additional_len);

    return true;
}
void ls_solver::build_neighborhood()
{
    size_t j, v;
    int c;
    vector<char> neighbor_flag(_num_vars + _additional_len);
    for (j = 0; j < neighbor_flag.size(); ++j) {
        neighbor_flag[j] = 0;
    }
    for (v = 1; v <= _num_vars; ++v) {
        variable *vp = &(_vars[v]);
        // vector<lit>& vp2=_vars[v].literals;
        for (lit lv : vp->literals) {
            c = lv.clause_num;
            for (lit lc : _clauses[c].literals) {
                if (!neighbor_flag[lc.var_num] && lc.var_num != v) {
                    neighbor_flag[lc.var_num] = 1;
                    vp->neighbor_var_nums.push_back(lc.var_num);
                }
            }
        }
        for (j = 0; j < vp->neighbor_var_nums.size(); ++j) {
            neighbor_flag[vp->neighbor_var_nums[j]] = 0;
        }
    }
}
bool ls_solver::build_instance(string inst)
{
    string line;
    istringstream iss;
    size_t c, v;
    int cur_lit;
    bool lit_redundent, clause_tautology;
    string tempstr1, tempstr2;
    vector<int> temp_clause;
    ifstream fin(inst.c_str());
    if (!fin.is_open()) return false;
    while (getline(fin, line)) {
        if (line.substr(0, 1) == "c")
            continue;
        else if (line.substr(0, 1) == "p") {
            // reassign_istringstream(iss, line);
            iss.clear();
            iss.str(line);
            iss.seekg(0, ios::beg);
            iss >> tempstr1 >> tempstr2 >> _num_vars >> _num_clauses;
            break;
        }
    }
    if (!make_space()) return false;
    // read clauses, each at a time
    for (c = 0; c < _num_clauses;) {
        clause_tautology = false;
        temp_clause.clear();
        vector<int>().swap(temp_clause);
        fin >> cur_lit;
        while (cur_lit != 0) {
            if (!clause_tautology) {
                lit_redundent = false;
                for (int item : temp_clause) {
                    if (cur_lit == item) {
                        lit_redundent = true;
                        break;
                    } else if (cur_lit == -item) {
                        clause_tautology = true;
                        break;
                    }
                }
                if (!lit_redundent) {
                    temp_clause.push_back(cur_lit);
                }
            }
            fin >> cur_lit;
        }
        if (!clause_tautology) {
            for (int item : temp_clause) {
                _clauses[c].literals.push_back(lit(item, c));
            }
            c++;
        } else {
            _num_clauses--;
            temp_clause.clear();
            vector<int>().swap(temp_clause);
        }
    }
    fin.close();
    /********finish reading the input file****************/
    // build variables data structure
    for (c = 0; c < _num_clauses; c++) {
        for (lit item : _clauses[c].literals) {
            v = item.var_num;
            _vars[v].literals.push_back(item);
        }
    }
    build_neighborhood();
    return true;
}
/****************local search**********************************/
// bool  *return value modified
bool ls_solver::local_search(const vector<char> *init_solution)
{
    bool result = false;
    int flipv;
    _random_gen.seed(_random_seed);
    _best_found_cost = _num_clauses;
    _best_cost_time = 0;
    conflict_ct = vector<int>(_num_vars + 10, 0);
    // for(int t=0; t < _max_tries; t++)
    // {
    initialize(init_solution);
    // cout<<"start ls"<<endl;
    _init_unsat_nums = _unsat_clauses.size();
    if (0 == _unsat_clauses.size()) {
        return true;
    } // 1
    for (_step = 0; _step < _max_steps; _step++) {
        if (_mems > _max_mems) {
            // std::cout << "c local search timeout with step "<< _step<< std::endl;
            return result;
        }
        flipv = pick_var();
        flip(flipv);
        for (int var_idx : _unsat_vars) ++conflict_ct[var_idx];
        if (_unsat_clauses.size() < _best_found_cost) {
            _best_found_cost = _unsat_clauses.size();
            for (size_t k = 0; k < _num_vars + 1; ++k) {
                _best_solution[k] = _solution[k];
            }
            // _best_cost_time = get_runtime();
        }
        if (0 == _unsat_clauses.size()) {
            result = true;
            break;
        } // 1
        // if (get_runtime() > _time_limit) {result = false;break;} //0
    }
    // if (0 == _unsat_clauses.size()) {result = true;}//1
    // if (get_runtime() > _time_limit) {result = false;break;}//0
    // }
    // _end_step = _step;
    return result;
}
/**********************************initialize*******************************/
void ls_solver::clear_prev_data()
{
    _unsat_clauses.clear();
    vector<int>().swap(_unsat_clauses);
    _ccd_vars.clear();
    vector<int>().swap(_ccd_vars);
    _unsat_vars.clear();
    vector<int>().swap(_unsat_vars);
    for (int &item : _index_in_unsat_clauses) item = 0;
    for (int &item : _index_in_unsat_vars) item = 0;
}
void ls_solver::initialize(const vector<char> *init_solution)
{
    size_t v, c;
    clear_prev_data();
    if (!init_solution) {
        // default random generation
        // cout<<"c using random initial solution"<<endl;
        for (v = 1; v <= _num_vars; v++) {
            _solution[v] = (_random_gen.next(2) == 0 ? 0 : 1);
        }
    } else {
        // cout<<"ok\n";
        if (init_solution->size() != _num_vars) {
            cout << "c Error: the init solution's size is not equal to the number of variables." << endl;
            exit(0);
        }
        for (v = 1; v <= _num_vars; v++) {
            _solution[v] = init_solution->at(v - 1);
            _best_solution[v] = _solution[v];
        }
        // cout<<"ok1\n";
    }
    // unsat_appears, will be updated when calling unsat_a_clause function.
    for (v = 1; v <= _num_vars; v++) {
        _vars[v].unsat_appear = 0;
    }
    // initialize data structure of clauses according to init solution
    for (c = 0; c < _num_clauses; c++) {
        _clauses[c].sat_count = 0;
        _clauses[c].sat_var = -1;
        _clauses[c].weight = 1;

        for (lit l : _clauses[c].literals) {
            if (_solution[l.var_num] == l.sense) {
                _clauses[c].sat_count++;
                _clauses[c].sat_var = l.var_num;
            }
        }
        if (0 == _clauses[c].sat_count) {
            unsat_a_clause(c);
        }
    }
    _avg_clause_weight = 1;
    _delta_total_clause_weight = 0;
    initialize_variable_datas();
}
void ls_solver::initialize_variable_datas()
{
    size_t v, c;
    variable *vp;
    // scores
    for (v = 1; v <= _num_vars; v++) {
        vp = &(_vars[v]);
        vp->score = 0;
        for (lit l : vp->literals) {
            c = l.clause_num;
            if (0 == _clauses[c].sat_count) {
                vp->score += _clauses[c].weight;
            } else if (1 == _clauses[c].sat_count && l.sense == _solution[l.var_num]) {
                vp->score -= _clauses[c].weight;
            }
        }
    }
    // last flip step
    for (v = 1; v <= _num_vars; v++) {
        _vars[v].last_flip_step = 0;
    }
    // cc datas
    for (v = 1; v <= _num_vars; v++) {
        vp = &(_vars[v]);
        vp->cc_value = 1;
        if (vp->score > 0) //&&_vars[v].cc_value==1
        {
            _ccd_vars.push_back(v);
            vp->is_in_ccd_vars = 1;
        } else {
            vp->is_in_ccd_vars = 0;
        }
    }
    // the virtual var 0
    vp = &(_vars[0]);
    vp->score = 0;
    vp->cc_value = 0;
    vp->is_in_ccd_vars = 0;
    vp->last_flip_step = 0;
}
/*********************end initialize functions*********************************/
/**********************pick variable*******************************************/
int ls_solver::pick_var()
{
    size_t i;
    int k, c, v;
    int best_var = 0;

    // if(_random_gen.nextClosed() < _prob_p){
    // 	c = _unsat_clauses[_random_gen.next(_unsat_clauses.size())];
    // 	clause * cp = &(_clauses[c]);
    // 	return cp->literals[_random_gen.next(cp->literals.size())].var_num;
    // }


    if (_ccd_vars.size() > 0) {
        _mems += _ccd_vars.size();
        best_var = _ccd_vars[0];
        for (int v : _ccd_vars) {
            if (_vars[v].score > _vars[best_var].score) {
                best_var = v;
            } else if (_vars[v].score == _vars[best_var].score && _vars[v].last_flip_step < _vars[best_var].last_flip_step) {
                best_var = v;
            }
        }
        return best_var;
    }

    // Aspriation Mode

    //----------------------------------------
    if (_aspiration_active) {
        // if(_random_gen.nextClosed() < _prob_p){
        // 	c = _unsat_clauses[_random_gen.next(_unsat_clauses.size())];
        // 	clause * cp = &(_clauses[c]);
        // 	return cp->literals[_random_gen.next(cp->literals.size())].var_num;
        // }

        _aspiration_score = _avg_clause_weight;
        for (i = 0; i < _unsat_vars.size(); ++i) {
            v = _unsat_vars[i];
            if (_vars[v].score > _aspiration_score) {
                best_var = v;
                break;
            }
        }
        for (++i; i < _unsat_vars.size(); ++i) {
            v = _unsat_vars[i];
            if (_vars[v].score > _vars[best_var].score)
                best_var = v;
            else if (_vars[v].score == _vars[best_var].score && _vars[v].last_flip_step < _vars[best_var].last_flip_step)
                best_var = v;
        }
        if (best_var != 0) return best_var;
    }
    //=========================================


    /**Diversification Mode**/
    update_clause_weights();

    /*focused random walk*/
    c = _unsat_clauses[_random_gen.next(_unsat_clauses.size())];
    clause *cp = &(_clauses[c]);
    best_var = cp->literals[0].var_num;
    int cp_lits_sz = cp->literals.size();
    for (k = 1; k < cp_lits_sz; k++) {
        v = cp->literals[k].var_num;
        if (_vars[v].score > _vars[best_var].score) {
            best_var = v;
        } else if (_vars[v].score == _vars[best_var].score && _vars[v].last_flip_step < _vars[best_var].last_flip_step) {
            best_var = v;
        }
    }
    return best_var;
}
/************************flip and update functions*****************************/
void ls_solver::flip(int flipv)
{
    _solution[flipv] = 1 - _solution[flipv];
    int org_flipv_score = _vars[flipv].score;
    _mems += _vars[flipv].literals.size();
    for (lit l : _vars[flipv].literals) {
        clause *cp = &(_clauses[l.clause_num]);
        if (_solution[flipv] == l.sense) {
            cp->sat_count++;
            if (1 == cp->sat_count) {
                sat_a_clause(l.clause_num);
                cp->sat_var = flipv;
                for (lit lc : cp->literals) {
                    _vars[lc.var_num].score -= cp->weight;
                }
            } else if (2 == cp->sat_count) {
                _vars[cp->sat_var].score += cp->weight;
            }
        } else {
            cp->sat_count--;
            if (0 == cp->sat_count) {
                unsat_a_clause(l.clause_num);
                for (lit lc : cp->literals) {
                    _vars[lc.var_num].score += cp->weight;
                }
            } else if (1 == cp->sat_count) {
                for (lit lc : cp->literals) {
                    if (_solution[lc.var_num] == lc.sense) {
                        _vars[lc.var_num].score -= cp->weight;
                        cp->sat_var = lc.var_num;
                        break;
                    }
                }
            }
        }
    }
    _vars[flipv].score = -org_flipv_score;
    _vars[flipv].last_flip_step = _step;
    // update cc_values
    update_cc_after_flip(flipv);
}
void ls_solver::update_cc_after_flip(int flipv)
{
    int index, v, last_item;
    variable *vp = &(_vars[flipv]);
    vp->cc_value = 0;
    for (index = _ccd_vars.size() - 1; index >= 0; index--) {
        v = _ccd_vars[index];
        if (_vars[v].score <= 0) {
            last_item = _ccd_vars.back();
            _ccd_vars.pop_back();
            _ccd_vars[index] = last_item;
            _mems++;
            _vars[v].is_in_ccd_vars = 0;
        }
    }
    // update all flipv's neighbor's cc to be 1
    for (int v : vp->neighbor_var_nums) {
        _vars[v].cc_value = 1;
        if (_vars[v].score > 0 && !(_vars[v].is_in_ccd_vars)) {
            _ccd_vars.push_back(v);
            _mems++;
            _vars[v].is_in_ccd_vars = 1;
        }
    }
}
/**********************end flip and update functions***************************/
/*********************functions for basic operations***************************/
void ls_solver::sat_a_clause(int the_clause)
{
    int index, last_item;
    // use the position of the clause to store the last unsat clause in stack
    last_item = _unsat_clauses.back();
    _unsat_clauses.pop_back();
    index = _index_in_unsat_clauses[the_clause];
    _unsat_clauses[index] = last_item;
    _index_in_unsat_clauses[last_item] = index;
    // update unsat_appear and unsat_vars
    for (lit l : _clauses[the_clause].literals) {
        _vars[l.var_num].unsat_appear--;
        if (0 == _vars[l.var_num].unsat_appear) {
            last_item = _unsat_vars.back();
            _unsat_vars.pop_back();
            index = _index_in_unsat_vars[l.var_num];
            _unsat_vars[index] = last_item;
            _index_in_unsat_vars[last_item] = index;
        }
    }
}
void ls_solver::unsat_a_clause(int the_clause)
{
    _index_in_unsat_clauses[the_clause] = _unsat_clauses.size();
    _unsat_clauses.push_back(the_clause);
    // update unsat_appear and unsat_vars
    for (lit l : _clauses[the_clause].literals) {
        _vars[l.var_num].unsat_appear++;
        if (1 == _vars[l.var_num].unsat_appear) {
            _index_in_unsat_vars[l.var_num] = _unsat_vars.size();
            _unsat_vars.push_back(l.var_num);
        }
    }
}
/************************clause weighting********************************/
void ls_solver::update_clause_weights()
{
    for (int c : _unsat_clauses) {
        _clauses[c].weight++;
    }
    _mems += _unsat_vars.size();
    for (int v : _unsat_vars) {
        _vars[v].score += _vars[v].unsat_appear;
        if (_vars[v].score > 0 && 1 == _vars[v].cc_value && !(_vars[v].is_in_ccd_vars)) {
            _ccd_vars.push_back(v);
            _vars[v].is_in_ccd_vars = 1;
        }
    }
    _delta_total_clause_weight += _unsat_clauses.size();
    if (_delta_total_clause_weight >= _num_clauses) {
        _avg_clause_weight += 1;
        _delta_total_clause_weight -= _num_clauses;
        if (_avg_clause_weight > _swt_threshold) {
            smooth_clause_weights();
        }
    }
}
void ls_solver::smooth_clause_weights()
{
    size_t v, c;
    for (v = 1; v <= _num_vars; v++) {
        _vars[v].score = 0;
    }
    int scale_avg = _avg_clause_weight * _swt_q;
    _avg_clause_weight = 0;
    _delta_total_clause_weight = 0;
    clause *cp;
    for (c = 0; c < _num_clauses; ++c) {
        cp = &(_clauses[c]);
        cp->weight = cp->weight * _swt_p + scale_avg;
        if (cp->weight < 1) cp->weight = 1;
        _delta_total_clause_weight += cp->weight;
        if (_delta_total_clause_weight >= _num_clauses) {
            _avg_clause_weight += 1;
            _delta_total_clause_weight -= _num_clauses;
        }
        if (0 == cp->sat_count) {
            for (lit l : cp->literals) {
                _vars[l.var_num].score += cp->weight;
            }
        } else if (1 == cp->sat_count) {
            _vars[cp->sat_var].score -= cp->weight;
        }
    }
    // reset ccd_vars
    _ccd_vars.clear();
    vector<int>().swap(_ccd_vars);
    variable *vp;
    for (v = 1; v <= _num_vars; v++) {
        vp = &(_vars[v]);
        if (vp->score > 0 && 1 == vp->cc_value) {
            _ccd_vars.push_back(v);
            vp->is_in_ccd_vars = 1;
        } else {
            vp->is_in_ccd_vars = 0;
        }
    }
}
/***parse parameters*****************/
bool ls_solver::parse_arguments(int argc, char **argv)
{
    bool flag_inst = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-inst") == 0) {
            i++;
            if (i >= argc) return false;
            _inst_file = argv[i];
            flag_inst = true;
            continue;
        } else if (strcmp(argv[i], "-seed") == 0) {
            i++;
            if (i >= argc) return false;
            sscanf(argv[i], "%d", &_random_seed);
            continue;
        }
    }
    if (flag_inst)
        return true;
    else
        return false;
}
/*****print solutions*****************/
void ls_solver::print_solution(bool need_verify)
{
    if (0 == get_cost())
        cout << "s SATISFIABLE" << endl;
    else
        cout << "s UNKNOWN" << endl;
    bool sat_flag = false;
    if (need_verify) {
        for (size_t c = 0; c < _num_clauses; c++) {
            sat_flag = false;
            for (lit l : _clauses[c].literals) {
                if (_solution[l.var_num] == l.sense) {
                    sat_flag = true;
                    break;
                }
            }
            if (!sat_flag) {
                cout << "c Error: verify error in clause " << c << endl;
                return;
            }
        }
        cout << "c Verified." << endl;
    }
    cout << "v";
    for (size_t v = 1; v <= _num_vars; v++) {
        cout << ' ';
        if (_solution[v] == 0) cout << '-';
        cout << v;
    }
    cout << endl;
}


void ls_solver::simple_print() { cout << '\t' << _best_found_cost << '\t' << _best_cost_time << endl; }
//=========================


// bool callUnCom(string inst){
// 	class ls_solver ccnr;

// 	start_timing();

// 	//string inst = argv[1];
// 	int	ret = ccnr.build_instance(inst);
// 	if (!ret) { cout << "Invalid filename: " << inst << endl; return 1;}

// 	vector<bool> * vp = 0;  //can set initial solution vector here.

// 	cout<<inst;
// 	//初始解在这里处理
// 	if(ccnr.local_search(vp)){
// 		ccnr.print_solution();
// 	}else{
// 		cout<<"unknown\n";
// 	}


// 	return 0;
// }


// main function

// int main(int argc, char* argv[]) {

// 	ls_solver ccnr;

// 	// start_timing();

// 	string inst = argv[1];
// 	int	ret = ccnr.build_instance(inst);
// 	if (!ret) { cout << "Invalid filename: " << argv[1] << endl; return 1;}

// 	vector<bool> * vp = 0;  //can set initial solution vector here.

// 	cout<<inst;
// 	//初始解在这里处理
// 	if(ccnr.local_search(vp)){
// 		ccnr.print_solution();
// 	}else{
// 		cout<<"unknown\n";
// 	}

// 	//bool need_verify = true;
// 	//ccnr.print_solution(need_verify);


// 	return 0;
// }
//=========================
