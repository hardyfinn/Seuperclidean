# Seuperclidean

A VCV Rack 2 module: six Euclidean rhythms (Toussaint / Bjorklund) with a
**Bias** warp, self-mutation, density and fills, swing and humanise, and logic
outputs that combine channel pairs.

## Per channel (rows 1 to 6)

| Control | What it does |
|---|---|
| **LEN** | Pattern length, 1 to 32 steps |
| **FILL** | Number of hits, spread as evenly as possible (E(3,8) = `x..x..x.`) |
| **ROT** | Rotates the whole pattern later by whole steps |
| **BIAS** | Warps the bar. 0 = classic Euclidean. Negative crowds the hits early, leaving a gap at the end. Positive pushes them late, rolling into the next downbeat. The first hit always stays on the pattern's downbeat, and the number of hits never changes |
| **SWG** | Swing, 50% (straight) to 75%. Delays every second 16th |
| **HUM** | Humanise: each hit lands randomly 0 to 30 ms late |
| **MUT** | Chance the pattern mutates at the end of each cycle |
| **FILL / ROT / BIAS CV** | Fill: 10 V adds a full length of hits. Rot: 10 V = one full turn. Bias: +-5 V sweeps the whole range |
| **GATE** out | The rhythm |
| **OFF** out | The gaps: every step the rhythm doesn't play |
| **ACC** out | Only the pattern's downbeat hit |

E(4,16) with different Bias settings:

```
bias -100%  xxx...x.........
bias  -50%  xx..x....x......
bias    0   x...x...x...x...
bias  +50%  x.......x...x.x.
bias +100%  x...........x.xx
```

## Mutation

When a channel's pattern comes round to its first step, it may (MUT x MUTATE
chance) change one thing: rotation by a step, bias by 25%, one hit more or
less, or one step flipped on or off. About a third of mutations move a change
back towards the knob settings instead, so patterns wander around home rather
than drifting off for good. In the display, orange is a normal hit, cyan is a
hit that mutation added, and dark brown is a hit that mutation removed.

| Control | What it does |
|---|---|
| **MUTATE** + CV | Scales every channel's MUT (default 100%) |
| **LOCK** button / gate | Freezes the current mutated patterns |
| **HOME** | Clears all mutation, back to the knob settings |

Mutated patterns are saved with the patch.

## Density and fills

| Control | What it does |
|---|---|
| **DENSITY** + CV | Adds hits to (right) or removes them from (left) every channel at once, as a share of each channel's length. +-5 V sweeps the whole range |
| **FILL** button / gate | While held, every channel fills up its empty steps by AMT and leans its Bias late, so it rolls into the next bar |
| **AMT** | How strong the fill is |

## Logic

Each output compares two channels step by step: 1+2, 3+4 and 5+6. Press the
button to choose the mode:

- **AND**: both channels hit
- **OR**: either channel hits
- **XOR**: one hits but not both
- **A NOT B**: the first channel hits and the second doesn't

The logic outputs use the first channel's swing.

## Clock

**CLOCK** takes 16th notes. **RESET** sends every channel back to step 1 on
the next clock. Right-click to choose trigger (5 ms) or gate (half a step)
outputs.

## Building

GitHub Actions builds the Apple Silicon version on every push (see
`.github/workflows/build.yml`). Locally: download the Rack SDK and run
`make dist RACK_DIR=/path/to/Rack-SDK`.

The panel is drawn by `res/make_panel.py` (needs `pip install fonttools`).
The rhythm engine (`src/EuclidEngine.hpp`) has no Rack dependencies, and
`test/engine_test.cpp` checks it offline.
