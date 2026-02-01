#pragma once

#include "surge.h"
#include "Evaluation.h"
#include "TranspositionTable.h"
#include "MoveOrdering.h"

#include <array>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <cstdlib> // std::abs(int)
namespace bq {

	struct PVLine {
		static constexpr int MAX = 64;
		std::array<Move, MAX> m{};
		int len = 0;

		void clear() { len = 0; }
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

			// repetition / cycle guard
			std::array<std::uint64_t, PVLine::MAX + 1> seen{};
			int seenLen = 0;

			for (int ply = 0; ply < maxPlies && out.len < PVLine::MAX; ++ply) {
				const std::uint64_t h = tmp.get_hash();

				// break on repetition/cycle
				for (int i = 0; i < seenLen; ++i) {
					if (seen[i] == h) return out;
				}
				seen[seenLen++] = h;

				auto e = m_transpositionTable.lookup(h);
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
			// Aspiration tuning knobs
			constexpr int ASP_START = 35;     // centipawns-ish
			constexpr int ASP_GROW = 2;      // multiply delta by this on each fail
			constexpr int ASP_TRIES = 6;      // max retries before full window
			constexpr int MATE_GUARD = 2000;  // if score is near mate, skip aspiration

			const int INF = m_checkmateScore;

			// Use previous iteration score as the center (only if meaningful)
			const int prevScore = m_searchStats.score;

			bool useAsp =
				(depth >= 2) &&
				(std::abs(prevScore) < INF - MATE_GUARD);

			int alpha = -INF;
			int beta = +INF;

			int center = prevScore;
			int delta = ASP_START;

			if (useAsp) {
				alpha = std::max(-INF, center - delta);
				beta = std::min(+INF, center + delta);
				if (alpha >= beta) { alpha = -INF; beta = +INF; useAsp = false; }
			}

			int score = 0;

			for (int attempt = 0; attempt < (useAsp ? ASP_TRIES : 1); ++attempt)
			{
				const auto start = std::chrono::steady_clock::now();

				score = pvs<us>(p, 0, depth, alpha, beta, false);

				const auto stop = std::chrono::steady_clock::now();
				const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
				m_searchStats.ellapsedTime += duration.count();

				if (m_stopping.load(std::memory_order_relaxed))
					return;

				if (!useAsp)
					break;

				// Fail-low / fail-high: widen and retry
				if (score <= alpha || score >= beta)
				{
					delta *= ASP_GROW;

					// If we've basically widened to "infinite", just do full window once
					if (delta >= INF) {
						alpha = -INF;
						beta = +INF;
						useAsp = false;
						// loop continues; next iteration will run full window exactly once
						continue;
					}

					alpha = std::max(-INF, center - delta);
					beta = std::min(+INF, center + delta);
					continue;
				}

				// Success: score inside window
				break;
			}

			// If we fell back to full window, run it once (when useAsp was disabled by widening)
			if (!useAsp && (alpha != -INF || beta != +INF)) {
				const auto start = std::chrono::steady_clock::now();
				score = pvs<us>(p, 0, depth, -INF, +INF, false);
				const auto stop = std::chrono::steady_clock::now();
				const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
				m_searchStats.ellapsedTime += duration.count();

				if (m_stopping.load(std::memory_order_relaxed))
					return;
			}

			// PV from TT (as you already do)
			PVLine pv = extractPvFromTt<us>(p, depth);

			m_searchStats.depth = depth;
			m_searchStats.score = score;

			m_searchStats.pvLen = pv.len;
			for (int i = 0; i < pv.len; ++i)
				m_searchStats.pv[i] = pv.m[i];

			m_searchStats.selectedMove = (pv.len > 0) ? pv.m[0] : Move{};
			m_searchStats.mateFound = (std::abs(score) >= INF - 256);
		}


		template <Color us>
		int quiescence(Position& p, int ply, int q_depth, int alpha, int beta) 
		{
			auto& stats = m_searchStats;
			++stats.nodesSearched;

			if (m_stopping.load(std::memory_order_relaxed))
				return alpha;

			if (q_depth > stats.qDepthReached)
				stats.qDepthReached = q_depth;

			const int stand_pat = bq::Evaluation::ScoreBoard<us>(p);

			if (stand_pat >= beta)
				return beta;

			if (stand_pat > alpha)
				alpha = stand_pat;

			if (q_depth >= m_maxSelDepth)
				return alpha;

			const bool inCheck = p.in_check<us>();

			// In check: must consider all evasions (quiet king moves, blocks, etc.)
			if (inCheck) {
				MoveList<us> moves(p);

				if (moves.size() == 0)
					return -m_checkmateScore + ply;

				for (const Move move : moves)
				{
					p.play<us>(move);

					const int score = -quiescence<~us>(
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

			// Not in check: only generate tacticals (captures, promotions, ep)
			TacticalMoveList<us> moves(p);

			// Not in check and no tacticals: stand-pat result already in alpha
			if (moves.size() == 0)
				return alpha;

			for (const Move move : moves)
			{
				// Optional (same logic you already had): cheap delta pruning for captures
				if (move.is_capture()) {
					const Piece victim = p.at(move.to());
					if (victim != NO_PIECE) {
						const int gain = pieceValues[type_of(victim)];
						if (stand_pat + gain + 100 < alpha)
							continue;
					}
				}

				p.play<us>(move);

				const int score = -quiescence<~us>(
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


		template <Color us>
		int pvs(Position& p, int ply, int depth, int alpha, int beta, bool reduced) 
		{
			auto& stats = m_searchStats;
			stats.nodesSearched++;
			if (m_stopping.load(std::memory_order_relaxed)) {
				return alpha;
			}

			

			if (depth <= 0)
				return quiescence<us>(p, ply, 0, alpha, beta);


			const int orig_alpha = alpha;
			const int orig_beta = beta;


			const std::uint64_t key = p.get_hash();

			auto tt_lookup = m_transpositionTable.lookup(key);

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
			const bool usInCheck = p.in_check<us>();
			const bool pvNode = (beta - alpha) > 1;

			if (!pvNode && depth <= 2 && !usInCheck) {
				int eval = bq::Evaluation::ScoreBoard<us>(p);

				if (eval + 220 * depth <= alpha) {
					return quiescence<us>(p, ply, 0, alpha, beta);
				}

				if (eval - 150 * depth >= beta) {
					return beta;
				}
			}

			MoveList<us> moves(p);
			const Move ttMove = (tt_lookup.valid ? tt_lookup.bestMove : Move{});
			orderMoves<us>(moves,ttMove);

			if (moves.size() == 0)
			{
				if (usInCheck) return -m_checkmateScore + ply;
				else                  return 0;
			}

			bool haveBest = false;
			int bestScore = -m_checkmateScore - 1;
			Move bestMove{};
			
			int moveNum = 0;
			for (Move& move : moves)
			{
				p.play<us>(move);

				int move_reduct = 0;
				// const int moveCount = int(moves.size()); // if you need it
				if (!pvNode && moveNum > 3 && depth >= 3 && !reduced && !move.is_capture())
					move_reduct = 1;


				int score = 0;

				if (moveNum == 0 || p.in_check<~us>())
				{
					score = -pvs<~us>(p, ply + 1, (depth - 1), -beta, -alpha, reduced);
				}
				else
				{
					score = -pvs<~us>(p, ply + 1, (depth - 1) - move_reduct,
						-alpha - 1, -alpha, move_reduct > 0);

					if (score > alpha && move_reduct > 0) {
						score = -pvs<~us>(p, ply + 1, depth - 1, -alpha - 1, -alpha, false);
					}

					if (score > alpha && score < beta) {
						score = -pvs<~us>(p, ply + 1, depth - 1, -beta, -alpha, false);
					}
				}

				p.undo<us>(move);

				if (m_stopping.load(std::memory_order_relaxed)) {
					return alpha;
				}

				if (!haveBest || score > bestScore) {
					haveBest = true;
					bestScore = score;
					bestMove = move;
				}

				if (score > alpha)
				{
					alpha = score;
					bestMove = move;
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

					m_transpositionTable.insert(key, e);
					return score;
				}

				++moveNum;
			}

			tt_entry entry;
			entry.valid = true;

			entry.bestMove = haveBest ? bestMove : Move{};

			int storeScore = alpha;
			if (std::abs(storeScore) >= m_checkmateScore - 1000)
			{
				storeScore = (storeScore > 0)
					? storeScore + ply
					: storeScore - ply;
			}

			entry.score = storeScore;
			entry.depth = depth;

			if (alpha <= orig_alpha) entry.flag = tt_flag::UPPERBOUND;
			else if (alpha >= orig_beta) entry.flag = tt_flag::LOWERBOUND;
			else entry.flag = tt_flag::EXACT;

			m_transpositionTable.insert(key, entry);
			return alpha;
		}
	};
}