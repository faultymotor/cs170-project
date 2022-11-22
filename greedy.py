from typing import Callable

import itertools
import networkx as nx

from starter import *
from test import *
from shared import *

# given G and a subset S, returns a fn(v) which sums weights from v to S 
# def sum_weights_to_subset(G: nx.graph, S: list[int]) -> Callable[[int], int]:
#     def from_vertex(v: int) -> int:
#         edges = [G[v][u]['weight'] for u in G.neighbors(v) if u in S]
#         return sum(edges)
#     return from_vertex

# Adds vertex v to team team, and updates team sums to connected vertices
def add_to_team(G: nx.graph, teams: list[list[int]], v: int, team: int):
    G.nodes[v]['team'] = team + 1
    teams[team].append(v)
    for u in G.neighbors(v):
        if team not in G.nodes[u]:
            G.nodes[u][team] = 0
        # Tracks sum from team to u        
        G.nodes[u][team] += G[u][v]['weight']


def solver(G: nx.graph, sources: list[int] = [40, 10, 20, 30, 0]) -> nx.Graph:    
    k = len(sources)
    teams = [[] for _ in sources]
    to_add = list(range(k))

    team = 0
    for u in sources:
        add_to_team(G, teams, u, team)
        team += 1
    
    for _ in range(k, G.number_of_nodes()):
        if len(to_add) == 0: to_add = list(range(k))
        next, team = min((
            min(
                filter(lambda v: not 'team' in G.nodes[v], G.nodes), 
                key=lambda v: G.nodes[v][team] if team in G.nodes[v] else 0), 
            team)
            for team in to_add)
        to_add.remove(team)
        add_to_team(G, teams, next, team)

    return G


def test_on_all_combinations(G):
    best_score, B = float('inf'), None

    for k in range(1, get_k_bound(G)):
        print('Now trying k =', k)
        curr_score, G_last, Ck, Cw, Cp, b, bnorm = None, None, None, None, None, None, None
        
        for sources in itertools.combinations(G.nodes, k):
            # print(sources)
            D = solver(G.copy(), sources)
            curr_score, Ck, Cw, Cp, b, bnorm = fast_update_score(G_last, D, Ck, Cw, Cp, b, bnorm)
            
            # assert abs(curr_score - score(D)) < 0.000001

            G_last = D
            if curr_score < best_score:
                best_score, B = curr_score, D.copy()
                print(k, best_score)
    
    return B

test_vs_output(test_on_all_combinations)

