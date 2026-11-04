#include "scan.h"

#include <math.h>
#include <string.h>

namespace {

constexpr int N = cfg::NUM_BINS;

float sumF[N];
float sumB[N];
uint8_t cntF[N];
uint8_t cntB[N];

int binIndex(float angleDeg) {
  int i = static_cast<int>(floorf(angleDeg / cfg::BIN_DEG)) % N;
  if (i < 0) i += N;
  return i;
}

}  // namespace

void scan::reset() {
  memset(sumF, 0, sizeof(sumF));
  memset(sumB, 0, sizeof(sumB));
  memset(cntF, 0, sizeof(cntF));
  memset(cntB, 0, sizeof(cntB));
}

void scan::clearRange(float fromDeg, float toDeg, int dir) {
  const int a = constrain(static_cast<int>(fromDeg / cfg::BIN_DEG), 0, N - 1);
  const int b = constrain(static_cast<int>(toDeg / cfg::BIN_DEG), 0, N - 1);
  for (int i = a; i <= b; i++) {
    if (dir >= 0) { sumF[i] = 0; cntF[i] = 0; }
    else          { sumB[i] = 0; cntB[i] = 0; }
  }
}

void scan::addSample(float angleDeg, int dir, float tobj) {
  const int i = binIndex(angleDeg);
  if (dir >= 0) {
    if (cntF[i] < 255) { sumF[i] += tobj; cntF[i]++; }
  } else {
    if (cntB[i] < 255) { sumB[i] += tobj; cntB[i]++; }
  }
}

scan::Result scan::analyze(float ambient, bool verbose) {
  Result r{};

  // Two scratch arrays only (a, b) -- each is reused for several stages to save stack.
  float a[N];  // stage 1: merged bins (NAN = no data); stage 3+: smoothed bins
  float b[N];  // stage 2: gap-filled bins;            stage 4:  sorted copy for the median

  // 1. Merge forward/back bins; average the two directions where both exist so that
  //    sensor response lag (which shifts the bump opposite ways per direction) cancels.
  int have = 0;
  for (int i = 0; i < N; i++) {
    const bool hf = cntF[i] > 0;
    const bool hb = cntB[i] > 0;
    if (hf && hb)      a[i] = (sumF[i] / cntF[i] + sumB[i] / cntB[i]) * 0.5f;
    else if (hf)       a[i] = sumF[i] / cntF[i];
    else if (hb)       a[i] = sumB[i] / cntB[i];
    else               a[i] = NAN;
    if (hf || hb) have++;
  }
  if (have < N / 2) return r;  // too sparse to trust

  // 2. Fill missing bins from the nearest real bin on each side so smoothing has no holes.
  for (int i = 0; i < N; i++) {
    if (!isnan(a[i])) { b[i] = a[i]; continue; }
    int lo = i, hi = i;
    for (int k = 1; k <= N; k++) { lo = (i - k + N) % N; if (!isnan(a[lo])) break; }
    for (int k = 1; k <= N; k++) { hi = (i + k) % N;     if (!isnan(a[hi])) break; }
    b[i] = (a[lo] + a[hi]) * 0.5f;
  }

  // 3. 3-bin moving average on the ring. Result lives in a[].
  for (int i = 0; i < N; i++) {
    a[i] = (b[(i - 1 + N) % N] + b[i] + b[(i + 1) % N]) / 3.0f;
  }
  const float *smooth = a;

  // 4. Baseline = median of the smoothed bins (insertion sort on a copy in b[]).
  memcpy(b, a, sizeof(b));
  for (int i = 1; i < N; i++) {
    const float key = b[i];
    int j = i - 1;
    while (j >= 0 && b[j] > key) { b[j + 1] = b[j]; j--; }
    b[j + 1] = key;
  }
  const float baseline = (b[N / 2 - 1] + b[N / 2]) * 0.5f;
  r.baseline = baseline;

  bool above[N];
  for (int i = 0; i < N; i++) above[i] = (smooth[i] - baseline) >= cfg::RISE_THRESH_C;

  // 5. Find contiguous runs on the ring. Start at a bin that is NOT above, if one exists, so a
  //    run never gets split by the 0/360 wrap point.
  int startIdx = 0;
  for (int i = 0; i < N; i++) if (!above[i]) { startIdx = (i + 1) % N; break; }

  bool any = false;
  float arcL = 0, arcR = 0, maxDelta = 0;

  int i = 0;
  while (i < N) {
    const int idx = (startIdx + i) % N;
    if (!above[idx]) { i++; continue; }

    int runLen = 0;
    int peakIdx = idx;
    while (runLen < N && above[(startIdx + i + runLen) % N]) {
      const int cur = (startIdx + i + runLen) % N;
      if (smooth[cur] > smooth[peakIdx]) peakIdx = cur;
      runLen++;
    }
    const int firstIdx = idx;
    const int lastIdx = (startIdx + i + runLen - 1) % N;

    // Half-height edges, linearly interpolated between the bin centers.
    const float halfLevel = baseline + cfg::RISE_THRESH_C * 0.5f;
    const int beforeFirst = (firstIdx - 1 + N) % N;
    const int afterLast = (lastIdx + 1) % N;

    float leftDeg = firstIdx * static_cast<float>(cfg::BIN_DEG);
    const float a0 = smooth[beforeFirst], a1 = smooth[firstIdx];
    if (a1 != a0) {
      const float frac = constrain((halfLevel - a0) / (a1 - a0), 0.0f, 1.0f);
      leftDeg = beforeFirst * cfg::BIN_DEG + frac * cfg::BIN_DEG;
    }

    float rightDeg = lastIdx * static_cast<float>(cfg::BIN_DEG);
    const float b0 = smooth[lastIdx], b1 = smooth[afterLast];
    if (b1 != b0) {
      const float frac = constrain((halfLevel - b0) / (b1 - b0), 0.0f, 1.0f);
      rightDeg = lastIdx * cfg::BIN_DEG + frac * cfg::BIN_DEG;
    }
    rightDeg += cfg::BIN_DEG;  // right edge of the last bin's cell, not its center

    const float peakObj = smooth[peakIdx];
    const float delta = peakObj - ambient;
    const bool person = delta >= cfg::PERSON_DELTA_C;

    // Pull both edges inward by FOV/2: the sensor sees a person before it points at them.
    const float half = cfg::FOV_EFF_DEG * 0.5f;
    float innerL = leftDeg + half;
    float innerR = rightDeg - half;
    if (innerL > innerR) innerL = innerR = (leftDeg + rightDeg) * 0.5f;

    r.numGroups++;
    if (person) {
      r.numPeople++;
      // Swing arc covers every detected person; delta = hottest person.
      const float l = innerL - cfg::SWING_MARGIN_DEG;
      const float rr = innerR + cfg::SWING_MARGIN_DEG;
      if (!any) { arcL = l; arcR = rr; any = true; }
      else { arcL = min(arcL, l); arcR = max(arcR, rr); }
      maxDelta = max(maxDelta, delta);
    }

    if (verbose) {
      Serial.print(F("[group] edges="));
      Serial.print(leftDeg, 1);
      Serial.print('-');
      Serial.print(rightDeg, 1);
      Serial.print(F(" peak@"));
      Serial.print(peakIdx * cfg::BIN_DEG);
      Serial.print(F(" delta="));
      Serial.print(delta, 2);
      Serial.println(person ? F(" PERSON") : F(" (too cool)"));
    }

    i += runLen;
  }

  r.arcL = constrain(arcL, cfg::YAW_MIN_DEG, cfg::YAW_MAX_DEG);
  r.arcR = constrain(arcR, cfg::YAW_MIN_DEG, cfg::YAW_MAX_DEG);
  r.delta = maxDelta;
  r.valid = any;
  return r;
}

void scan::printProfile(float ambient) {
  Serial.println(F("bin,deg,fwdAvg,backAvg"));
  for (int i = 0; i < N; i++) {
    Serial.print(i);
    Serial.print(',');
    Serial.print(i * cfg::BIN_DEG);
    Serial.print(',');
    if (cntF[i]) Serial.print(sumF[i] / cntF[i], 2); else Serial.print('-');
    Serial.print(',');
    if (cntB[i]) Serial.print(sumB[i] / cntB[i], 2); else Serial.print('-');
    Serial.println();
  }
  Serial.print(F("ambient="));
  Serial.println(ambient, 2);
}
