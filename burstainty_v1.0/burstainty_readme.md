# burstainty - Probability Burst Generator - version 1.0

Trigger length-controlled burst generator with probability-based outputs for Uncertainty.

## Outputs

| Gate | Bursts | Probability | Description |
|------|--------|-------------|-------------|
| 1 | 2 | 100% | Always fires (2 bursts) |
| 2 | 3 | 100% | Always fires (3 bursts) |
| 3 | 4 | 100% | Always fires (4 bursts) |
| 4 | 5 | 50% | 50% chance to fire 5 bursts |
| 5 | 6 | 30% | 30% chance to fire 6 bursts |
| 6 | 7 | 15% | 15% chance to fire 7 bursts |
| 7 | 8 | 8% | 8% chance to fire 8 bursts |
| 8 | 12 | 3% | 3% chance to fire 12 bursts |

## Control

**Gate Length = Burst Speed**
- **Short gates** (quick taps) = Fast bursts (80ms intervals)
- **Long gates** (hold 200ms+) = Slow bursts (250ms intervals)

## Usage

1. **Send triggers** to CV input from sequencer/clock
2. **Vary gate length** to control burst speed
3. **Gates 1-3** provide reliable foundation rhythm
4. **Gates 4-8** add probabilistic excitement

## Customization

Edit these arrays to change behavior:

```cpp
// Change burst counts per output
const int BURST_COUNTS[8] = {2, 3, 4, 5, 6, 7, 8, 12};

// Change firing probabilities (0-100%)
const int BASE_PROBABILITIES[8] = {100, 100, 100, 50, 30, 15, 8, 3};
```
