#include "cdbdirect.h"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>

#include "external/chess.hpp"
using namespace chess;


std::vector<std::vector<std::pair<uint64_t, Move>>> graph;
std::vector<std::vector<std::pair<uint64_t, Move>>> rev_graph;
std::vector<int> visited;
std::vector<uint64_t> group;

std::vector<uint64_t> post;
std::map<PackedBoard, uint64_t> pos_to_idx;
uint64_t tot_pos = 0;


void CreateGraph(Board &board, const std::uintptr_t& handle) {
    auto db_lookup = cdbdirect_get(handle, board.getFen(false));
    if (db_lookup.size() < 2) {
        return;
    }

    uint64_t cur_idx = tot_pos-1;

    auto& best_move = db_lookup[0];
    for (auto& cur_move : db_lookup) {
        if (cur_move.first == "a0a0") {
            break;
        }
        if (cur_move.second != best_move.second) {
            continue;
        }

        Move move = uci::uciToMove(board, cur_move.first);
        board.makeMove<true>(move);

        PackedBoard packed_board = Board::Compact::encode(board);
        if (pos_to_idx.contains(packed_board)) {
            graph[cur_idx].push_back({pos_to_idx[packed_board], move});
            rev_graph[pos_to_idx[packed_board]].push_back({cur_idx, move});
            board.unmakeMove(move);
            continue;
        }
        pos_to_idx[packed_board] = tot_pos;
        graph.push_back({});
        rev_graph.push_back({});
        graph[cur_idx].push_back({tot_pos, move});
        rev_graph[tot_pos].push_back({cur_idx, move});
        tot_pos++;

        CreateGraph(board, handle);
        board.unmakeMove(move);
    }
}


void PDFS(uint64_t cur) {
    if (visited[cur] != 0) {
        return;
    }
    visited[cur] = 1;
    for (auto next : graph[cur]) {
        PDFS(next.first);
    }
    post.push_back(cur);
}


void RDFS(uint64_t cur, uint64_t idx) {
    if (visited[cur] != 0) {
        return;
    }
    visited[cur] = 1;
    group[cur] = idx;
    for (auto next : rev_graph[cur]) {
        RDFS(next.first, idx);
    }
}

uint64_t SCC(uint64_t n) {
    group.assign(n, 0);
    visited.assign(n, 0);
    for (uint64_t i = 0; i < n; i++) {
        if (visited[i] != 0) {
            continue;
        }
        PDFS(i);
    }
    visited.assign(n, 0);
    uint64_t idx = 0;
    for (int64_t i = n-1; i >= 0; i--) {
        if (visited[post[i]] != 0) {
            continue;
        }
        RDFS(post[i], idx);
        idx++;
    }
    return idx;
}


int main(int argc, char **argv) {
    // DB
    std::uintptr_t handle = cdbdirect_initialize(CHESSDB_PATH);
    std::uint64_t db_size = cdbdirect_size(handle);
    std::cout << "DB count: " << db_size << std::endl;

    // fen
    std::string fen;
    fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq -"; // Startpos
    if (argc == 2) {
        fen = argv[1];
    }
    Board board(fen);
    std::cout << "Looking at fen: " << fen << std::endl;
    PackedBoard packed_board = Board::Compact::encode(board);
    pos_to_idx[packed_board] = tot_pos++;
    graph.push_back({});
    rev_graph.push_back({});

    // create graph
    CreateGraph(board, handle);
    std::cout << "Number of position in best_move graph: " << tot_pos << std::endl;

    uint64_t scc_cnt = SCC(tot_pos);
    std::cout << "Total number of components: " << scc_cnt << std::endl;


    std::vector<Board> representitive_board(scc_cnt);
    for (auto pti : pos_to_idx) {
        representitive_board[pti.second] = Board::Compact::decode(pti.first);
    }

    for (uint64_t i = 0; i < representitive_board.size(); i++) {
        std::cout << "component " << i << ": " << representitive_board[i].getFen(false) << std::endl;
    }

    cdbdirect_finalize(handle);
    return 0;
}
