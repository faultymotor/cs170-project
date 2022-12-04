#include "rpg.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <random>
#include <algorithm>
#include <vector>
#include <cmath>
#include <tuple>

#include "graph_utils.h"

#define K_COEFFICIENT 100
#define B_EXP 70
#define K_EXP 0.5

//helper function declerations
double norm(double *array, size_t length);
double update_cw(graph_t *G, double cw, int32_t u, int32_t i, int32_t j);
double* update_b(double *b, int32_t v, int32_t i, int32_t j, double bnorm);
std::tuple<double, double> update_cp(double *b, double bnorm, int32_t v, int32_t i, int32_t j);
double calculate_ck(int32_t k);
int32_t calculate_k_bound(graph_t *G);
bool compare_cost(const std::vector<double>& v1, const std::vector<double>& v2);

int myisnan(double x) {
  return std::isnan(x);
}

/*
G is a graph
K is the number of teams
stale is the number of times the graph has to be unchanged to terminate
epsilon is the weight multiplier
decay is what scales epsilon each timestep.
*/
std::tuple<int32_t*, double> solver(graph_t *G, int32_t k, int32_t stale, double epsilon, double decay){
    
    //assigns random teams to the vertices
    // printf("STARTED SOLVER on %d nodes\n", G->num_nodes);
    // printf("k=%i\n", k);
    std::vector<int32_t> scramble(G->num_nodes);
    for (int32_t i = 0; i < G->num_nodes; i++){
        // printf("pushing scramble %d", i);
        scramble[i] = i;
    }
    std::random_shuffle(scramble.begin(), scramble.end());

    for (int32_t i = 0; i < G->num_nodes; i++){
        int32_t curr = scramble.back();
        scramble.pop_back();
        G->nodes[curr].team = i%k;
    }
    // printf("Initial Scramble done\n");
    // printf("k=%i\n", k);
    double best_cost = std::numeric_limits<double>::infinity();
    int32_t *B = NULL;
    int32_t k_list[k];
    for(int32_t i = 0; i < k; i++) k_list[i] = i;
    int32_t count = 0;
    int32_t counter = 0;
    double last_cost = 0;
    // printf("First update start\n");
    // printf("k=%i\n", k);
    auto[total_cost, ck, cw, cp, bnorm, b] = first_update_score(G);
    // printf("first update done\n");
    double weights[k];
    // std::tuple<double, double, double, double, double*>
    // printf("Declared cost creation\n");
    // std::vector<std::vector<double>> cost;
    // printf("Starting cost Initialization\n");
    // for(int i = 0; i < k; i ++){
    //     printf("Creating cost for k = %i\n", i);
    //     cost.push_back(std::vector<double>(5));
    // }
    double cost[k][5];
    // printf("Starting While\n");
    while((count < stale) && (counter < G->num_nodes)){
        // printf("Started While\n");
        counter++;
        
        for(int i = 0; i < k; i++){
            weights[i] = pow(epsilon, k_list[i]);
        }
        last_cost = total_cost;

        for(int u = 0; u < G->num_nodes; u++){
            // printf("Going through nodes\n");
            int32_t i = G->nodes[u].team;
            for(int32_t j = 0; j < k; j++){
                if (i != j){
                    double cw_j = update_cw(G, cw, u, i, j);
                    auto[cp_j, bnorm_j] = update_cp(b, bnorm, G->num_nodes, i, j);
                    double delta_cost = (cw_j - cw) + (cp_j - cp);
                    cost[j][0] = delta_cost;
                    cost[j][1] = cw_j;
                    cost[j][2] = cp_j;
                    cost[j][3] = bnorm_j;
                    cost[j][4] = j;
                }else{
                    cost[j][0] = 0;
                    cost[j][1] = cw;
                    cost[j][2] = cp;
                    cost[j][3] = bnorm;
                    cost[j][4] = j;
                }
                
            }
            // printf("Started sort\n");

            // std::sort(cost.begin(), cost.end(), compare_cost);
            std::qsort(cost, k, 5 * sizeof(double),
                [](const void *arg1, const void *arg2)->int
                {
                    double const *lhs = static_cast<double const*>(arg1);
                    double const *rhs = static_cast<double const*>(arg2);
                    return (lhs[0] < rhs[0]) ? -1 : 1;
                });
            double sum = 0;
            for(int32_t i = 0; i < k; i++){
                sum += weights[i];
            }
            
            double rnd = (double)rand()/(RAND_MAX/sum);
            // printf("RANDOM: %f\n", rnd);
            int32_t best_team = 0;
            int32_t best_index = 0;
            for(int32_t i = 0; i < k; i++){
                rnd -= weights[i];
                best_index = i;
                best_team = (int32_t) cost[i][4];
                if(rnd <= 0) break;
            }
            // int32_t best_team = cost[0][4];
            int32_t last_team = G->nodes[u].team;
            G->nodes[u].team = best_team;

            total_cost += cost[best_index][0];
            // auto real_cost = first_update_score(G);
            
            
            

            cw = cost[best_index][1];
            cp = cost[best_index][2];
            // double old = bnorm;
            bnorm = cost[best_index][3];
            

            b = update_b(b, G->num_nodes, last_team, best_team, bnorm);
            // bnorm = norm(b, k);
            // if(abs(std::get<0>(real_cost)- total_cost) > 0.01){
            //     printf("==============================\n");
            //     printf("total_cost = %f, should be = %f\n", total_cost, std::get<0>(real_cost));
            // }
            // if(abs(std::get<2>(real_cost) - cw) > 0.01){
            //     printf("bad cw is: %f, should be: %f\n", cost[best_index][1], std::get<2>(real_cost));
            // }
            // if(abs(std::get<3>(real_cost) - cp) > 0.01){
            //     printf("bad cp is: %f, should be: %f\n", cost[best_index][2], std::get<3>(real_cost));
            //     // auto[cp_j, bnorm_j] = update_cp(b, bnorm, G->num_nodes, last_team, best_team);
            // }
            // if(abs(std::get<4>(real_cost) - bnorm) > 0.00001){
            //     printf("bad bnorm is: %f, should be: %f\n", bnorm, std::get<4>(real_cost));
            //     update_cp(b, old, G->num_nodes, last_team, best_team);

            // }

            // for(int32_t i = 0; i < k; i++){
            //     if(abs(std::get<5>(real_cost)[i] - b[i]) > 0.00001){
            //         printf("b at %i is wrong. Is %f, should be %f\n", i, b[i], std::get<5>(real_cost)[i]);
            //     }
            // }
            // }
            
            
        }

        epsilon = epsilon/decay;
        if(total_cost < best_cost){

            if(B != NULL){
                // printf("freeing B\n");
                free(B);
                // printf("finished freeing B\n");
            }
            // printf("making copy of B\n");
            B = copy_teams(G);
            best_cost = total_cost;
        }

        if(last_cost == total_cost){
            count += 1;
        }else{
            count = 0;
        }

    }
    // printf("count %d\n", count);
    // printf("counter %d\n", counter);
    free(b);
    return {B, best_cost};
}

std::tuple<int32_t*, double> test_on_all_k(graph_t *G, int32_t repeats, bool verbose){
    double best_score = std::numeric_limits<double>::infinity();
    int32_t *B = NULL;
    int32_t bound = calculate_k_bound(G);
    // printf("Note that the k bound is %i.\n",bound);
    
    for(int k = 1; k < bound; k++){
        
        double lower_bound = calculate_ck(k) + 1.0;
        if(verbose){
            printf("===========================================================\n");
            printf("Now trying k=%i, with best_score=%f...\n", k, best_score);
            printf("Note that the lower bound of k=%i is %f.\n", k, lower_bound);
        }

        if(lower_bound > best_score){
            return {B, best_score};
        }
        // printf("Graph number of nodes in test_on_all_k = %d\n", G->num_nodes);
        for(int i = 0; i < repeats; i++){
            // printf("k=%i\n", k);
            auto[G_new, curr_score] = solver(G, k, 3, 0.5, 1.2);
            // printf("Graph number of nodes after solver = %d\n", G->num_nodes);
            // printf("Curr score = %f, Best score = %f\n", curr_score, best_score);
            for(int32_t i = 0; i < G->num_nodes; i++){
                // printf("i = %i\n", i);
                G->nodes[i].team = G_new[i];
            }
            auto real_cost = first_update_score(G);
            // printf("total_cost = %f, should be = %f\n", curr_score, std::get<0>(real_cost));
            if(curr_score < best_score){
                free(B);
                best_score = curr_score;
                B = copy_teams(G_new, G->num_nodes);
                
                if(verbose){
                    // printf("Found a new best score %f with k= %i.\n", best_score, k);
                }
            }
            // printf("freeing G_new with %i num of nodes\n", G_new->num_nodes);
            free(G_new);
            // printf("finished freeing G_new\n");
        }
        
    }
    return {B, best_score};    
}

bool compare_cost(const std::vector<double>& v1, const std::vector<double>& v2){
    return v1[0] < v2[0];
}


int32_t calculate_k_bound(graph_t *G){
    double bound = (2 * log((G->sum_weights/100) + 1.6487212707));
    return (int32_t) bound;
}

double calculate_ck(int32_t k){
    return K_COEFFICIENT * exp(K_EXP * k);
}

/*
v is the number of nodes
i is original team 0 indexed
j is new team 0 indexed

returns cp, bnorm
*/
std::tuple<double, double> update_cp(double *b, double bnorm, int32_t v, int32_t i, int32_t j){
    // have this only update bnorm and cp then I can do b after finding the best one
    double temp_bnorm = sqrt(pow(bnorm, 2) - pow(b[i], 2) - pow(b[j], 2) + pow(b[i] - (double)1/v, 2) + pow(b[j] + (double)1/v, 2)); 
    // b_new[i] -= 1/v;
    // b_new[j] += 1/v;
    if(std::isnan(temp_bnorm)){
        temp_bnorm = 0;
    }
    double cp = exp(B_EXP*temp_bnorm);

    return {cp, temp_bnorm};
}

double* update_b(double *b, int32_t v, int32_t i, int32_t j, double bnorm){
    // if(bnorm == 0){
    //     b[i] = 0;
    //     b[j] = 0;
    // }else{
        b[i] -= (double)1/v;
        b[j] += (double)1/v;
    // }
    
    return b;
}

double update_cw(graph_t *G, double cw, int32_t u, int32_t i, int32_t j){
    double delta = 0;
    if(i == j) return cw;
    for(int32_t n = 0; n < G->nodes[u].num_neighbors; n++){
        int32_t v = G->nodes[u].neighbors[n].target;
        if(G->nodes[v].team == i){
            delta -= G->nodes[u].neighbors[n].weight;
        }else if(G->nodes[v].team == j){
            delta += G->nodes[u].neighbors[n].weight;
        }
    }

    return cw + delta;
}

//return is cost, ck, cw, cp, bnorm, b
std::tuple<double, double, double, double, double, double*> first_update_score(graph_t *G){
    int32_t k = 0;
    int32_t counts[25] = {0};
    int32_t max;
    for(int32_t i = 0; i < G->num_nodes; i++){
        if(counts[G->nodes[i].team] == 0){
            k++;
        }
        counts[G->nodes[i].team]++;
        if(counts[G->nodes[i].team] > max){
            max = counts[G->nodes[i].team];
        }
    }
    double *b = (double *) calloc(k, sizeof(double));
    for(int32_t i = 0; i < k; i++){
        b[i] =  ((double)counts[i] /G->num_nodes) - ((double)1/k);
    }
    
    double bnorm = norm(b, k);

    double cw = 0;

    for(int32_t i = 0; i < G->num_nodes; i++){
        for(int32_t j = 0; j < G->nodes[i].num_neighbors; j++){
            int32_t source = G->nodes[i].neighbors[j].source;
            int32_t target = G->nodes[i].neighbors[j].target;
            int32_t weight = G->nodes[i].neighbors[j].weight;
            if(G->nodes[source].team == G->nodes[target].team){
                //divided by two to account for double counting
                cw += (double)weight/2; 
            }
        }
    }

    double ck = K_COEFFICIENT * exp(K_EXP * k);
    double cp = exp(B_EXP * bnorm);

    return {(cw + ck + cp), ck, cw, cp, bnorm, b};
}

double norm(double *array, size_t length){
    double sum = 0;
    for(int i = 0; i < length; i++){
        sum += pow(array[i], 2);
    }
    return sqrt(sum);
}