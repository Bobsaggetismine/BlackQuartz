#pragma once

#include "surge.h"
#include "Evaluation.h"
#include "TranspositionTable.h"
#include "MoveOrdering.h"
#include <array>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <cstdlib>
namespace bq {
	struct PVLine {
		static constexpr int MAX = 64;
		std::array<Move, MAX> m{};
		int len = 0;

		void clear() { len = 0; }

		void set(Move first, const PVLine& child) {
			m[0] = first;
			len = 1;
			for (int i = 0; i < child.len && len < MAX; ++i)
				m[len++] = child.m[i];
		}
	};
	struct SearchStats {
		int qDepthReached = 0;
		long long ellapsedTime = 0;
		int depth = 0;
		int score = 0;
		long long nodesSearched = 0;
		bool mateFound = false;
		Move selectedMove;

		static constexpr int MAX_PV = 64;
		std::array<Move, MAX_PV> pv{};
		int pvLen = 0;
		void reset()
		{
			depth = 0;
			score = 0;
			ellapsedTime = 0;
			nodesSearched = 0;
			qDepthReached = 0;
			mateFound = false;
			selectedMove = Move{};
			pvLen = 0;
		}
	};
	constexpr int pieceValues[NPIECE_TYPES] = {
	100,
	300,
	305,
	500,
	900,
	2000000
	};
	class Search {

		bq::TranspositionTable m_transpositionTable;
		SearchStats m_searchStats;
		std::atomic<bool> m_stopping{ false };
		int m_maxSelDepth;
		const int m_checkmateScore = 100000;
	public:

		Search(int maxSelDepth)
			: m_maxSelDepth(maxSelDepth)
		{

		}

		void signalStop() { m_stopping.store(true, std::memory_order_relaxed); }
		static bool isLegalRt(Position& p, Color stm, Move m) {
			if (stm == WHITE) {
				MoveList<WHITE> ml(p);
				for (Move x : ml) if (x == m) return true;
			}
			else {
				MoveList<BLACK> ml(p);
				for (Move x : ml) if (x == m) return true;
			}
			return false;
		}

		static void playRt(Position& p, Color stm, Move m) {
			if (stm == WHITE) p.play<WHITE>(m);
			else              p.play<BLACK>(m);
		}

		template <Color RootUs>
		PVLine extractPvFromTt(Position& root, int maxPlies) {
			PVLine out;
			out.clear();

			Position tmp = root;
			Color stm = RootUs;

			for (int ply = 0; ply < maxPlies && out.len < PVLine::MAX; ++ply) {
				auto e = m_transpositionTable.lookup(tmp.get_hash());
				if (!e.valid || e.bestMove.is_null()) break;

				Move m = e.bestMove;
				if (!isLegalRt(tmp, stm, m)) break;

				out.m[out.len++] = m;
				playRt(tmp, stm, m);
				stm = ~stm;
			}

			return out;
		}
		template <Color us>
		SearchStats initiateIterativeSearch(Position& p, int depth)
		{
			m_searchStats.reset();
			m_stopping = false;

			for (int i = 1; i <= depth; ++i)
			{
				initiateSearch<us>(p, i);
				if (m_stopping.load(std::memory_order_relaxed)) break;
			}
			return m_searchStats;
		}


	private:
		
		template <Color us>
		void initiateSearch(Position& p, int depth)
		{
			auto start = std::chrono::steady_clock::now();

			PVLine pv;
			int score = pvs<us>(p, 0, depth,
				-m_checkmateScore, m_checkmateScore,
				false, pv);

			auto stop = std::chrono::steady_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);

			m_searchStats.ellapsedTime += duration.count();

			if (m_stopping.load(std::memory_order_relaxed))
				return;

			PVLine ttPv = extractPvFromTt<us>(p, depth); 
			if (ttPv.len > pv.len)
				pv = ttPv;

			m_searchStats.depth = depth;
			m_searchStats.score = score;

			m_searchStats.pvLen = pv.len;
			for (int i = 0; i < pv.len; ++i)
				m_searchStats.pv[i] = pv.m[i];

			m_searchStats.selectedMove = (pv.len > 0) ? pv.m[0] : Move{};
			m_searchStats.mateFound = (std::abs(score) >= m_checkmateScore - 256);
		}



		template <Color us>
		int quiescence(Position& p, int ply, int q_depth, int alpha, int beta, PVLine& pv)
		{
			pv.clear();
			m_searchStats.nodesSearched++;
			if (m_stopping.load(std::memory_order_relaxed)) {
				pv.clear();
				return alpha;
			}
			m_searchStats.qDepthReached = std::max(q_depth, m_searchStats.qDepthReached);
			int stand_pat = bq::Evaluation::ScoreBoard<us>(p);

			if (stand_pat >= beta)
				return beta;

			if (stand_pat > alpha)
				alpha = stand_pat;

			if (q_depth >= m_maxSelDepth)
				return alpha;

			MoveList<us> moves(p);
			const bool inCheck = p.in_check<us>();
			if (moves.size() == 0) {
				if (inCheck) return -m_checkmateScore + ply;
				return 0;
			}

			for (Move& move : moves)
			{
				bool is_capture = move.is_capture();
				bool is_promotion = move.is_promotion();
				if (!inCheck) {
					if (!is_capture && !is_promotion)
					{
						continue;
					}
				}
				if (is_capture)
				{
					Piece victim = p.at(move.to());
					if (victim != NO_PIECE)
					{
						int gain = pieceValues[type_of(victim)];
						if (stand_pat + gain + 100 < alpha)
							continue;
					}
				}

				p.play<us>(move);
				
				PVLine child;
				int score = -quiescence<~us>(
					p,
					ply + 1,
					q_depth + 1,
					-beta,
					-alpha,
					child
				);

				p.undo<us>(move);

				if (score >= beta)
					return beta;

				if (score > alpha) {
					alpha = score;
					pv.set(move, child);
				}
			}

			return alpha;
		}

		template <Color us>
		int pvs(Position& p, int ply, int depth, int alpha, int beta, bool reduced, PVLine& pv)
		{
			pv.clear();
			m_searchStats.nodesSearched++;
			if (m_stopping.load(std::memory_order_relaxed)) {
				pv.clear();
				return alpha;
			}

			int orig_alpha = alpha;
			int orig_beta = beta;

			if (depth <= 0)
				return quiescence<us>(p, ply, 0, alpha, beta, pv);

			auto tt_lookup = m_transpositionTable.lookup(p.get_hash());

			if (tt_lookup.valid && tt_lookup.depth >= depth)
			{
				int tt_score = tt_lookup.score;

				// Normalize mate scores to current ply
				if (std::abs(tt_score) >= m_checkmateScore - 1000)
				{
					tt_score = (tt_score > 0)
						? tt_score - ply
						: tt_score + ply;
				}

				bool pvNodeTT = (beta - alpha) > 1;
				if (tt_lookup.flag == tt_flag::EXACT && !pvNodeTT)
					return tt_score;

				if (tt_lookup.flag == tt_flag::LOWERBOUND)
					alpha = std::max(alpha, tt_score);
				else if (tt_lookup.flag == tt_flag::UPPERBOUND)
					beta = std::min(beta, tt_score);

				if (alpha >= beta)
					return alpha;
			}

			bool pvNode = (beta - alpha) > 1;

			if (!pvNode && depth <= 2 && !p.in_check<us>()) {
				int eval = bq::Evaluation::ScoreBoard<us>(p);

				if (eval + 220 * depth <= alpha) {
					return quiescence<us>(p, ply, 0, alpha, beta, pv);
				}

				if (eval - 150 * depth >= beta) {
					return beta;
				}
			}

			MoveList<us> moves(p);
			orderMoves<us>(p, moves, m_transpositionTable, depth > 1);

			if (moves.size() == 0)
			{
				if (p.in_check<us>()) return -m_checkmateScore + ply;
				else                  return 0;
			}

			PVLine bestLine;
			int bestScore = -m_checkmateScore - 1;
			bool haveBest = false;

			int moveNum = 0;
			Move bestMove{};
			for (Move& move : moves)
			{
				p.play<us>(move);

				int move_reduct = 0;
				if (!pvNode && moveNum > 3 && moves.size() >= 4 && depth >= 3 && !reduced && !move.is_capture())
					move_reduct = 1;

				int score = 0;
				PVLine childPV;

				if (moveNum == 0 || p.in_check<~us>())
				{
					score = -pvs<~us>(p, ply + 1, (depth - 1), -beta, -alpha, reduced, childPV);
				}
				else
				{
					PVLine tmp;
					score = -pvs<~us>(p, ply + 1, (depth - 1) - move_reduct,
						-alpha - 1, -alpha, move_reduct > 0, tmp);
					childPV = tmp;

					if (score > alpha && move_reduct > 0) {
						PVLine confirm;
						score = -pvs<~us>(p, ply + 1, depth - 1, -alpha - 1, -alpha, false, confirm);
						childPV = confirm;
					}

					if (score > alpha && score < beta) {
						PVLine full;
						score = -pvs<~us>(p, ply + 1, depth - 1, -beta, -alpha, false, full);
						childPV = full;
					}
				}

				p.undo<us>(move);

				if (m_stopping.load(std::memory_order_relaxed)) {
					pv.clear();
					return alpha;
				}

				if (!haveBest || score > bestScore) {
					haveBest = true;
					bestScore = score;
					bestLine.set(move, childPV);
				}

				if (score > alpha)
				{
					alpha = score;
					bestMove = move;
					pv.set(move, childPV);
				}

				if (score >= beta) {
					tt_entry e;
					e.valid = true;
					int s = score;
					if (std::abs(s) >= m_checkmateScore - 1000)
						s = (s > 0) ? s + ply : s - ply;
					e.score = s;
					e.depth = depth;
					e.flag = tt_flag::LOWERBOUND;
					e.bestMove = move;
					m_transpositionTable.insert(p.get_hash(), e);
					return score;
				}

				++moveNum;
			}

			if (pv.len == 0 && haveBest) {
				pv = bestLine;
			}

			tt_entry entry;
			entry.valid = true;
			int storeScore = alpha;

			if (std::abs(storeScore) >= m_checkmateScore - 1000)
			{
				storeScore = (storeScore > 0)
					? storeScore + ply
					: storeScore - ply;
			}
			if (bestMove.is_null() && haveBest && bestLine.len > 0) {
				bestMove = bestLine.m[0];
			}

			if (pv.len == 0 && haveBest) {
				pv = bestLine;
				if (bestMove.is_null() && pv.len > 0) bestMove = pv.m[0];
			}
			entry.bestMove = bestMove;
			entry.score = storeScore;
			entry.depth = depth;

			if (alpha <= orig_alpha) entry.flag = tt_flag::UPPERBOUND;
			else if (alpha >= orig_beta) entry.flag = tt_flag::LOWERBOUND;
			else entry.flag = tt_flag::EXACT;

			m_transpositionTable.insert(p.get_hash(), entry);
			return alpha;
		}

	};
}
