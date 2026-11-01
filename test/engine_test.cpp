// Offline checks for the Seuperclidean engine: patterns, bias, logic, timing.
#include "../src/EuclidEngine.hpp"
#include <cstdio>
#include <cassert>
using namespace ew;

static void show(const char* name, uint32_t m, int len) {
	printf("%-28s ", name);
	for (int i = 0; i < len; i++) putchar((m >> i) & 1 ? 'x' : '.');
	putchar('\n');
}
static int bits(uint32_t m) { int n = 0; while (m) { m &= m - 1; n++; } return n; }

int main() {
	char buf[64];
	show("E(3,8)", warpedPattern(8, 3, 0), 8);
	show("E(5,8)", warpedPattern(8, 5, 0), 8);
	show("E(7,16)", warpedPattern(16, 7, 0), 16);
	for (float b : {-1.f, -0.75f, -0.5f, -0.25f, 0.f, 0.25f, 0.5f, 0.75f, 1.f}) {
		snprintf(buf, sizeof buf, "E(4,16) bias %+.2f", b);
		show(buf, warpedPattern(16, 4, b), 16);
	}
	for (float b : {-1.f, 0.f, 1.f}) {
		snprintf(buf, sizeof buf, "E(13,16) bias %+.2f", b);
		show(buf, warpedPattern(16, 13, b), 16);
	}
	// every pattern keeps its hit count and its downbeat
	for (int len = 1; len <= 32; len++)
		for (int f = 0; f <= len; f++)
			for (int bi = -8; bi <= 8; bi++) {
				uint32_t m = warpedPattern(len, f, bi / 8.f);
				assert(bits(m) == f);
				if (f > 0) assert(m & 1u);
				if (len < 32) assert((m >> len) == 0);
			}
	printf("hit counts and downbeats OK\n");
	show("rotate E(3,8) by 2", rotate(warpedPattern(8, 3, 0), 8, 2), 8);

	// Run a clock: 120 BPM 16ths at 48 kHz, count output gates
	Engine e;
	e.ch[0].len = 16; e.ch[0].fill = 4;
	e.ch[1].len = 16; e.ch[1].fill = 5;
	e.ch[2].len = 13; e.ch[2].fill = 5; e.ch[2].swing = 1.f;
	e.ch[3].len = 7; e.ch[3].fill = 3; e.ch[3].mutate = 1.f;
	e.logicMode[0] = XOR;
	const double sr = 48000, dt = 1 / sr; long spt = 6000;
	int rises[6] = {0}, offR[6] = {0}, logicR = 0; bool last[6] = {0}, lastOff[6] = {0}, lastL = 0;
	int steps = 16 * 13 * 4;
	for (long s = 0; s < spt * steps + 100; s++) {
		e.process(dt, s % spt == 0, false);
		for (int c = 0; c < 6; c++) {
			if (e.ch[c].gate.high && !last[c]) rises[c]++;
			if (e.ch[c].off.high && !lastOff[c]) offR[c]++;
			last[c] = e.ch[c].gate.high; lastOff[c] = e.ch[c].off.high;
		}
		if (e.logic[0].high && !lastL) logicR++;
		lastL = e.logic[0].high;
	}
	printf("steps %d\n", steps);
	for (int c = 0; c < 4; c++) printf("ch%d hits %d offbeats %d (sum %d)\n", c + 1, rises[c], offR[c], rises[c] + offR[c]);
	printf("logic 1+2 XOR hits %d (expect %d per 16 steps)\n", logicR, bits(e.ch[0].mask ^ e.ch[1].mask));
	printf("ch4 mutation: dFill %d dRot %d dBias %.2f flips %x\n", e.ch[3].dFill, e.ch[3].dRot, e.ch[3].dBias, e.ch[3].flips);
	show("ch4 home", e.ch[3].homeMask, 7);
	show("ch4 now", e.ch[3].mask, 7);
	return 0;
}
