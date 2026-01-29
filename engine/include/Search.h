#pragma once

#include "surge.h"
#include "Evaluation.h"
#include "TranspositionTable.h"
#include "MoveOrdering.h"

#include <chrono>

namespace bq {
	
	struct SearchStats {
		int qDepthReached = 0;
		long long ellapsedTime = 0;
		int depth = 0;
		int score = 0;
		long long nodesSearched = 0;
		bool mateFound = false;
		Move selectedMove;
		void reset()
		{
			depth = 0;
			score = 0;
			ellapsedTime = 0;
			nodesSearched = 0;
			qDepthReached = 0;
			mateFound = false;
		}
	};
	constexpr int pieceValues[NPIECE_TYPES] = {
	100,    // PAWN
	300,    // KNIGHT
	305,    // BISHOP
	500,    // ROOK
	900,    // QUEEN
	2000000 // KING
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

		template <Color us>
		SearchStats initiateIterativeSearch(Position& p, int depth)
		{
			m_searchStats.reset();
			m_stopping = false;
			for (int i = 1; i <= depth; ++i)
			{
				m_searchStats.depth = i;
				initiateSearch<us>(p, i);
				if (m_stopping.load(std::memory_order_relaxed) || m_searchStats.mateFound) break;
			}
			return m_searchStats;
		}

	private:

		template <Color us>
		void initiateSearch(Position& p, int depth)
		{
			m_searchStats.depth = depth;
			auto start = std::chrono::steady_clock::now();
			pvs<us>(p, 0, depth, -m_checkmateScore, m_checkmateScore, false);
			auto stop = std::chrono::steady_clock::now();
			auto duration = duration_cast<std::chrono::microseconds>(stop - start);
			m_searchStats.ellapsedTime += duration.count();
		}

		template <Color us>
		int quiescence(Position& p, int ply, int q_depth, int alpha, int beta)
		{
			m_searchStats.nodesSearched++;
			if (m_stopping.load(std::memory_order_relaxed))
				return 0;
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
				// Delta pruning (safe)
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
				

				int score = -quiescence<~us>(
					p,
					ply + 1,
					q_depth + 1,
					-beta,
					-alpha
				);

				p.undo<us>(move);

				if (score >= beta)
					return beta;

				if (score > alpha)
					alpha = score;
			}

			return alpha;
		}



		//chad 1990s search
		template <Color us>
		int pvs(Position& p, int ply, int depth, int alpha, int beta, bool reduced)
		{
			m_searchStats.nodesSearched++;
			if (m_stopping.load(std::memory_order_relaxed)) return 0;

			int orig_alpha = alpha;
			int orig_beta = beta;

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

				if (tt_lookup.flag == tt_flag::EXACT)
					return tt_score;

				if (tt_lookup.flag == tt_flag::LOWERBOUND)
					alpha = std::max(alpha, tt_score);
				else if (tt_lookup.flag == tt_flag::UPPERBOUND)
					beta = std::min(beta, tt_score);

				if (alpha >= beta)
					return alpha;
			}
			if (depth <= 0) return quiescence<us>(p, ply, 0, alpha, beta);

			bool is_pv = (beta - alpha) > 1;

			if (!is_pv && depth <= 2 && !p.in_check<us>()) {
				int eval = bq::Evaluation::ScoreBoard<us>(p);

				if (eval + 220 * depth <= alpha) {
					return quiescence<us>(p, ply, 0, alpha, beta);
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
				else 				  return 0;
			}

			int moveNum = 0;
			Move bestMove{}; // NEW
			for (Move& move : moves)
			{
				
				p.play<us>(move);
				
				int move_reduct = 0;
				if (!is_pv && moveNum > 3 && moves.size() >= 4 && depth >= 3 && !reduced && !move.is_capture()) move_reduct = 1;
				int score;
				if (moveNum == 0 || p.in_check<us>())
				{
					score = -pvs<~us>(p, ply + 1, (depth - 1), -beta, -alpha, reduced);
				}
				else
				{
					score = -pvs<~us>(p, ply + 1, (depth - 1) - move_reduct, -alpha - 1, -alpha, move_reduct > 0);
					if (score > alpha && move_reduct > 0) score = -pvs<~us>(p, ply + 1, (depth - 1), -alpha - 1, -alpha, reduced);
					if (score > alpha && score < beta) 	  score = -pvs<~us>(p, ply + 1, (depth - 1), -beta, -alpha, reduced);
				}
				p.undo<us>(move);

				if (m_stopping.load(std::memory_order_relaxed)) return 0;

				if (score > alpha)
				{
					alpha = score;
					bestMove = move;

					if (depth == m_searchStats.depth)
					{
						m_searchStats.selectedMove = move;
						m_searchStats.score = score;
						if (std::abs(score) >= m_checkmateScore - 256)
							m_searchStats.mateFound = true;
					}
				}
				if (score >= beta) return beta;
				++moveNum;
			}
			tt_entry entry;
			entry.valid = true;
			int storeScore = alpha;

			// Convert mate scores to TT-relative ply
			if (std::abs(storeScore) >= m_checkmateScore - 1000)
			{
				storeScore = (storeScore > 0)
					? storeScore + ply
					: storeScore - ply;
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
