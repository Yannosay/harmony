export const pitchNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

export const scales = {
  Major: [0, 2, 4, 5, 7, 9, 11],
  Minor: [0, 2, 3, 5, 7, 8, 10],
  Dorian: [0, 2, 3, 5, 7, 9, 10],
  'Harmonic Minor': [0, 2, 3, 5, 7, 8, 11]
};

export const progressions = [
  { name: 'Pop', degrees: [1, 5, 6, 4] },
  { name: 'Emotional', degrees: [6, 4, 1, 5] },
  { name: 'Epic', degrees: [1, 6, 3, 7] },
  { name: 'Jazz', degrees: [2, 5, 1, 1] },
  { name: 'Royal Road', degrees: [4, 5, 3, 6] },
  { name: 'Andalusian', degrees: [1, 7, 6, 5] }
];

const qualities = [
  { suffix: '', intervals: [0, 4, 7] },
  { suffix: 'm', intervals: [0, 3, 7] },
  { suffix: 'dim', intervals: [0, 3, 6] },
  { suffix: 'aug', intervals: [0, 4, 8] },
  { suffix: 'maj7', intervals: [0, 4, 7, 11] },
  { suffix: 'm7', intervals: [0, 3, 7, 10] },
  { suffix: '7', intervals: [0, 4, 7, 10] },
  { suffix: 'm7b5', intervals: [0, 3, 6, 10] }
];

export function chordPitches(root, scale, degree, size, baseKey) {
  const intervals = scales[scale];
  const n = intervals.length;
  const start = (degree - 1) % n;
  const pitches = [];
  for (let i = 0; i < size; i++) {
    const idx = start + i * 2;
    pitches.push(baseKey + root + intervals[idx % n] + 12 * Math.floor(idx / n));
  }
  return pitches;
}

export function chordName(pitches) {
  const classes = [...new Set(pitches.map((p) => ((p % 12) + 12) % 12))].sort((a, b) => a - b);
  const root = ((pitches[0] % 12) + 12) % 12;
  for (const q of qualities) {
    const shape = [...new Set(q.intervals.map((i) => (root + i) % 12))].sort((a, b) => a - b);
    if (shape.length === classes.length && shape.every((v, i) => v === classes[i])) return pitchNames[root] + q.suffix;
  }
  return pitchNames[root];
}

export function leadVoicing(previous, chord, low, high) {
  if (!previous || previous.length !== chord.length) return chord;
  let best = chord;
  let bestCost = Infinity;
  const n = chord.length;
  for (let inv = 0; inv < n; inv++) {
    const voicing = [];
    for (let i = 0; i < n; i++) {
      let p = chord[(inv + i) % n];
      while (voicing.length && p <= voicing[voicing.length - 1]) p += 12;
      voicing.push(p);
    }
    for (let shift = -3; shift <= 3; shift++) {
      const candidate = voicing.map((p) => p + shift * 12);
      if (candidate[0] < low || candidate[n - 1] > high) continue;
      const cost = candidate.reduce((sum, p, i) => sum + Math.abs(p - previous[i]), 0);
      if (cost < bestCost) {
        bestCost = cost;
        best = candidate;
      }
    }
  }
  return best;
}

export function generate({ root, scale, progression, size }) {
  const baseKey = 60;
  let previous = null;
  return progressions[progression].degrees.map((degree, index) => {
    const raw = chordPitches(root, scale, degree, size, baseKey);
    const voiced = leadVoicing(previous, raw, baseKey - 7, baseKey + 24);
    previous = voiced;
    return { index, name: chordName(raw), notes: voiced, bass: raw[0] - 12 };
  });
}

export const drumStyles = {
  Trap: ['x.....x...x....-', '........x.......', 'x.x.x.x.x.x.x.x.', '..............-.'],
  House: ['x...x...x...x...', '....x.......x...', '-.-.-.-.-.-.-.-.', '..x...x...x...x.'],
  'Boom Bap': ['x.....-.-.x.....', '....x.......x...', 'x.o.x.o.x.o.x.o.', '..........-.....'],
  Reggaeton: ['x...x...x...x...', '...x..x....x..x.', 'x.x.x.x.x.x.x.x.', '..............-.'],
  'Drum & Bass': ['x.........x.....', '....x..-.-..x...', 'x.x.x.x.x.x.x.x.', '......-.......-.']
};

export function drumGrid(style, density, seed) {
  let state = seed >>> 0 || 1;
  const random = () => {
    state ^= state << 13;
    state ^= state >>> 17;
    state ^= state << 5;
    return ((state >>> 0) % 10000) / 10000;
  };
  return drumStyles[style].map((row) =>
    [...row].map((c) => {
      if (c === 'x') return 1;
      if (c === 'o') return random() < 0.3 + 0.65 * density ? 0.7 : 0;
      if (c === '-') return random() < 0.08 + 0.5 * density ? 0.55 : 0;
      return 0;
    })
  );
}
