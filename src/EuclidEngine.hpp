// EuclidEngine.hpp
// The rhythm maths for Euclid Warp. No VCV Rack dependencies, so it can be
// tested offline (see test/engine_test.cpp).
//
// Six channels, each a Euclidean rhythm (Fill hits spread over Len steps) with:
//   - Bias: warps the bar so the hits crowd early (negative) or late
//     (positive). The first hit always stays on the pattern's downbeat.
//   - Rotation, applied after Bias.
//   - Mutation: at the end of each cycle the pattern may nudge itself
//     (rotation, bias, fill, or a single flipped step), and sometimes drifts
//     back towards home, so it evolves without wandering off for good.
//   - Swing and humanise timing per channel.
//   - Gate, off-beat (the gaps) and accent (the downbeat hit) outputs.
// Plus three logic outputs combining channel pairs 1+2, 3+4, 5+6.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace ew {

static const int NUM_CH = 6;
static const int MAX_LEN = 32;
static const int NUM_LOGIC = 3;

enum LogicMode { AND = 0, OR, XOR, ANOTB, NUM_LOGIC_MODES };

struct Rng {
	uint64_t s = 0x9E3779B97F4A7C15ull;
	void setSeed(uint64_t seed) { s = seed ? seed : 0x9E3779B97F4A7C15ull; }
	uint64_t next() {
		s ^= s << 13; s ^= s >> 7; s ^= s << 17;
		return s;
	}
	double uniform() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
	int below(int n) { return n <= 0 ? 0 : int(uniform() * n) % n; }
	int sign() { return (next() >> 63) ? 1 : -1; }
};

// The Euclidean pattern with Bias, before rotation. Bit i = step i hits.
// bias -1..+1. 0 gives the classic evenly spread rhythm (E(3,8) = x..x..x.).
inline uint32_t warpedPattern(int len, int fill, float bias) {
	len = std::max(1, std::min(MAX_LEN, len));
	fill = std::max(0, std::min(len, fill));
	if (fill == 0) return 0;
	if (fill == len) return len == 32 ? 0xFFFFFFFFu : ((1u << len) - 1);
	// The warp curve is t^g: g > 1 pulls hits towards the start of the bar,
	// g < 1 pushes them towards the end. Full bias = g of 4 or 1/4.
	double g = std::pow(2.0, -2.0 * (double) bias);
	uint32_t mask = 0;
	int prev = -1;
	for (int k = 0; k < fill; k++) {
		double t = (double) k / fill;
		double x = std::pow(t, g) * len;
		int pos = (int) std::ceil(x - 1e-9);
		// keep hits on separate steps, and leave room for the ones still to come
		pos = std::max(pos, prev + 1);
		pos = std::min(pos, len - (fill - k));
		mask |= 1u << pos;
		prev = pos;
	}
	return mask;
}

inline uint32_t rotate(uint32_t mask, int len, int rot) {
	rot %= len;
	if (rot < 0) rot += len;
	if (rot == 0) return mask;
	uint32_t out = 0;
	for (int i = 0; i < len; i++)
		if (mask & (1u << i))
			out |= 1u << ((i + rot) % len);
	return out;
}

// One timed output: a gate that can be scheduled a little after the clock
struct Out {
	double pendingIn = -1.0; // seconds until it fires, < 0 = nothing pending
	double gateLeft = 0.0;   // seconds the gate stays high
	double gateLen = 0.0;
	bool high = false;

	void fire(double len) {
		gateLeft = len;
		high = true;
	}
	void schedule(double delay, double len) {
		if (pendingIn >= 0.0) fire(gateLen); // never drop a hit
		gateLen = len;
		if (delay <= 0.0) { pendingIn = -1.0; fire(len); }
		else pendingIn = delay;
	}
	void process(double dt) {
		if (pendingIn >= 0.0) {
			pendingIn -= dt;
			if (pendingIn < 0.0) { pendingIn = -1.0; fire(gateLen); }
		}
		if (high) {
			gateLeft -= dt;
			if (gateLeft <= 0.0) high = false;
		}
	}
	void clear() { pendingIn = -1.0; high = false; gateLeft = 0.0; }
};

struct Channel {
	// settings (written by the module)
	int len = 16;
	int fill = 4;
	int rot = 0;
	float bias = 0.f;
	float swing = 0.f;  // 0..1 = 50%..75%
	float human = 0.f;  // 0..1 = 0..30 ms random lateness
	float mutate = 0.f; // 0..1 chance per cycle

	// mutation offsets (the pattern's drift away from the knobs)
	int dFill = 0;
	int dRot = 0;
	float dBias = 0.f;
	uint32_t flips = 0;

	// state
	int step = -1;
	uint32_t homeMask = 0; // pattern from the knobs alone
	uint32_t mask = 0;     // pattern actually playing (with mutation)
	int accentStep = 0;
	bool hit = false;

	Out gate, off, acc;

	void clearMutation() { dFill = 0; dRot = 0; dBias = 0.f; flips = 0; }
};

struct Engine {
	Channel ch[NUM_CH];
	Out logic[NUM_LOGIC];
	int logicMode[NUM_LOGIC] = {AND, XOR, OR};

	// global settings
	float density = 0.f;    // -1..1, adds or removes hits on every channel
	float mutateAmt = 1.f;  // 0..1, scales every channel's Mutate
	bool locked = false;    // freeze mutation
	bool fillActive = false;
	float fillAmt = 0.5f;   // 0..1
	enum GateMode { TRIGGER = 0, HALF_STEP };
	int gateMode = HALF_STEP;

	// clock
	double period = 0.125; // seconds per step, measured
	double sinceClock = 1e9;
	int64_t globalStep = -1;
	Rng rng;

	void reset() {
		for (int c = 0; c < NUM_CH; c++) ch[c].step = -1;
		globalStep = -1;
	}

	void home() {
		for (int c = 0; c < NUM_CH; c++) ch[c].clearMutation();
		updatePatterns();
	}

	// Recompute every channel's pattern from settings + mutation offsets.
	void updatePatterns() {
		for (int c = 0; c < NUM_CH; c++) {
			Channel& k = ch[c];
			int len = std::max(1, std::min(MAX_LEN, k.len));
			int fill = (int) std::lround(k.fill + density * len);
			float bias = k.bias;
			if (fillActive && fillAmt > 0.f) {
				fill = std::max(0, std::min(len, fill));
				fill += (int) std::lround(fillAmt * (len - fill));
				bias += 0.6f * fillAmt;
			}
			int homeFill = std::max(0, std::min(len, fill));
			float homeBias = std::max(-1.f, std::min(1.f, bias));
			k.homeMask = rotate(warpedPattern(len, homeFill, homeBias), len, k.rot);

			int mFill = std::max(0, std::min(len, fill + k.dFill));
			float mBias = std::max(-1.f, std::min(1.f, bias + k.dBias));
			int mRot = k.rot + k.dRot;
			uint32_t lenMask = len == 32 ? 0xFFFFFFFFu : ((1u << len) - 1);
			k.mask = (rotate(warpedPattern(len, mFill, mBias), len, mRot) ^ k.flips) & lenMask;
			int a = mRot % len;
			k.accentStep = a < 0 ? a + len : a;
		}
	}

	// One mutation event for channel c (called at the end of its cycle).
	void mutate(Channel& k) {
		int len = std::max(1, k.len);
		bool goHome = rng.uniform() < 0.35;
		int what = rng.below(4);
		if (goHome) {
			// drift one thing back towards the knob settings
			if (what == 0 && k.dRot) k.dRot -= (k.dRot > 0 ? 1 : -1);
			else if (what == 1 && k.dBias != 0.f) k.dBias = std::fabs(k.dBias) <= 0.25f ? 0.f : k.dBias - (k.dBias > 0 ? 0.25f : -0.25f);
			else if (what == 2 && k.dFill) k.dFill -= (k.dFill > 0 ? 1 : -1);
			else if (k.flips) {
				// clear the lowest flipped step
				k.flips &= k.flips - 1;
			}
			return;
		}
		switch (what) {
			case 0: k.dRot = std::max(-len, std::min(len, k.dRot + rng.sign())); break;
			case 1: k.dBias = std::max(-1.f, std::min(1.f, k.dBias + 0.25f * rng.sign())); break;
			case 2: {
				int lim = std::max(1, len / 4);
				k.dFill = std::max(-lim, std::min(lim, k.dFill + rng.sign()));
				break;
			}
			default: {
				// flip one step, but leave the downbeat alone
				int s = rng.below(len);
				if (s != k.accentStep) k.flips ^= 1u << s;
				break;
			}
		}
	}

	double stepGate() const {
		return gateMode == TRIGGER ? 0.005 : std::max(0.002, 0.5 * period);
	}

	// Call once per sample. Returns true on the samples where a clock was handled.
	void process(double dt, bool clockEdge, bool resetEdge) {
		sinceClock += dt;
		if (resetEdge) reset();
		if (clockEdge) {
			// measure tempo (ignore long gaps when the clock was stopped)
			if (sinceClock < 2.0 && sinceClock > 0.001) period = sinceClock;
			sinceClock = 0.0;
			globalStep++;
			bool oddStep = (globalStep & 1) != 0;

			// advance each channel; mutate when a channel starts a new cycle
			for (int c = 0; c < NUM_CH; c++) {
				Channel& k = ch[c];
				int len = std::max(1, std::min(MAX_LEN, k.len));
				int prev = k.step;
				k.step = (k.step + 1) % len;
				if (k.step == 0 && prev >= 0 && !locked) {
					double p = (double) k.mutate * mutateAmt;
					if (p > 0.0 && rng.uniform() < p) mutate(k);
				}
			}
			updatePatterns();

			double gl = stepGate();
			for (int c = 0; c < NUM_CH; c++) {
				Channel& k = ch[c];
				k.hit = (k.mask >> k.step) & 1u;
				double swingDelay = oddStep ? 0.5 * k.swing * period : 0.0;
				double human = k.human * 0.030 * rng.uniform();
				double d = swingDelay + human;
				if (k.hit) {
					k.gate.schedule(d, gl);
					if (k.step == k.accentStep) k.acc.schedule(d, gl);
				}
				else {
					k.off.schedule(d, gl);
				}
			}
			for (int i = 0; i < NUM_LOGIC; i++) {
				bool a = ch[2 * i].hit, b = ch[2 * i + 1].hit;
				bool o = false;
				switch (logicMode[i]) {
					case AND: o = a && b; break;
					case OR: o = a || b; break;
					case XOR: o = a != b; break;
					default: o = a && !b; break;
				}
				if (o) {
					double swingDelay = oddStep ? 0.5 * ch[2 * i].swing * period : 0.0;
					logic[i].schedule(swingDelay, gl);
				}
			}
		}
		for (int c = 0; c < NUM_CH; c++) {
			ch[c].gate.process(dt);
			ch[c].off.process(dt);
			ch[c].acc.process(dt);
		}
		for (int i = 0; i < NUM_LOGIC; i++) logic[i].process(dt);
	}
};

} // namespace ew
