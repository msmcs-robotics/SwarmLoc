# UWB Multilateration/Trilateration Implementation for Drone Positioning

**Project**: SwarmLoc - GPS-Denied Positioning System
**Hardware**: Arduino Uno + DW1000 UWB Radio (PCL298336 v1.3 Shield)
**Target Application**: Autonomous drone swarm positioning without GPS
**Date**: 2026-01-11
**Status**: Research and Implementation Guide

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Multilateration Fundamentals](#1-multilateration-fundamentals)
3. [Practical Algorithms](#2-practical-algorithms)
4. [Anchor Placement and Geometry](#3-anchor-placement-and-geometry)
5. [Arduino Implementation Considerations](#4-arduino-implementation-considerations)
6. [Accuracy Expectations](#5-accuracy-expectations)
7. [Code Examples](#6-code-examples)
8. [Testing and Calibration](#7-testing-and-calibration)
9. [References](#8-references)

---

## Executive Summary

### Key Findings

**What is Multilateration?**
- Position determination using distance measurements to multiple reference points
- Essential for GPS-denied drone navigation
- Minimum 3 anchors for 2D, 4 anchors for 3D positioning

**Best Algorithm for Arduino Uno:**
- **Linear Least Squares** (recommended)
- Computational complexity: O(n) where n = number of anchors
- Memory: ~200 bytes for 4 anchors
- Execution time: ~5-10ms on Arduino Uno

**Expected Performance:**
- **Position Accuracy**: ±20-50 cm (with ±10-20cm ranging error)
- **Update Rate**: 2-5 Hz (3-4 anchors)
- **Computational Load**: Manageable on Arduino Uno
- **Recommendation**: Offload to flight controller for advanced filtering

### Quick Reference Table

| Aspect | Value | Notes |
|--------|-------|-------|
| Minimum anchors (2D) | 3 | 4 recommended for redundancy |
| Minimum anchors (3D) | 4 | 5-6 recommended for redundancy |
| Best algorithm | Linear Least Squares | Good accuracy, low computation |
| Arduino RAM usage | ~200 bytes | For 4 anchors + matrix ops |
| Computation time | 5-10 ms | Per position update |
| Position accuracy | ±20-50 cm | With ±10-20cm ranging error |
| Update rate | 2-5 Hz | Limited by ranging protocol |

---

## 1. Multilateration Fundamentals

### 1.1 Trilateration vs Multilateration

**Trilateration** (exactly 3 reference points):
- Solves for position using circle intersection
- Produces 0-2 solutions (geometric ambiguity)
- Used when exactly 3 anchors available
- Closed-form solution available

**Multilateration** (4+ reference points):
- Overdetermined system (more equations than unknowns)
- Uses optimization to find best-fit position
- Provides redundancy and error correction
- Recommended for real-world applications

**Key Differences:**

```
Trilateration (3 anchors):
┌─────────────────────────────────────┐
│    Anchor 1                         │
│      (x1,y1)                        │
│        ○                            │
│       ╱ ╲  d1                       │
│      ╱   ╲                          │
│     ╱     ╲                         │
│    ╱       ╲                        │
│   ╱         ╲                       │
│  ○───────────○  Anchor 2,3          │
│               d2, d3                │
│                                     │
│  Two circles → One intersection     │
│  (or two, resolve with 3rd circle)  │
└─────────────────────────────────────┘

Multilateration (4+ anchors):
┌─────────────────────────────────────┐
│    ○ A1    ○ A2                     │
│                                     │
│         ⊗  ← Best fit position      │
│                                     │
│    ○ A3    ○ A4                     │
│                                     │
│  Multiple circles → Optimization    │
│  Minimizes sum of squared errors    │
└─────────────────────────────────────┘
```

### 1.2 Minimum Number of Anchors

**Mathematical Requirements:**

**2D Positioning** (x, y):
- **Minimum**: 3 anchors (exactly determined)
- **Recommended**: 4 anchors (overdetermined, error correction)
- **Optimal**: 5-6 anchors (redundancy for outlier rejection)

**3D Positioning** (x, y, z):
- **Minimum**: 4 anchors (exactly determined)
- **Recommended**: 5-6 anchors (overdetermined)
- **Optimal**: 8+ anchors (full spatial coverage)

**Why More is Better:**
1. **Redundancy**: System continues working if anchor fails
2. **Error Correction**: Outliers can be detected and rejected
3. **Accuracy**: Overdetermined system averages out measurement noise
4. **GDOP**: Better geometric diversity reduces dilution of precision

**Practical Recommendation for Drone Swarms:**
- Start with **4 anchors** (3D positioning with one redundant)
- Expand to 6 anchors for production systems
- Arduino Uno can handle 4-6 anchors (RAM constraint)

### 1.3 2D vs 3D Positioning

**2D Positioning (Planar)**:
- Assumes all devices at same altitude
- Useful for ground robots or fixed-altitude drones
- Requires 3+ anchors
- Simpler computation

**2.5D Positioning** (Known Altitude):
- Measures (x, y) with barometric altitude for z
- Hybrid approach for drones
- More accurate than pure 2D
- Recommended for SwarmLoc

**3D Positioning (Full Volumetric)**:
- Measures (x, y, z) from UWB alone
- Requires 4+ anchors with vertical separation
- Most flexible but computationally intensive
- Best accuracy when anchors surround target

**Coordinate Systems:**

```cpp
// 2D: Cartesian (x, y)
struct Position2D {
    float x;  // East-West (meters)
    float y;  // North-South (meters)
};

// 3D: Cartesian (x, y, z)
struct Position3D {
    float x;  // East-West (meters)
    float y;  // North-South (meters)
    float z;  // Altitude above reference (meters)
};

// Alternative: Relative positioning
struct RelativePosition {
    float range;      // Distance from origin (m)
    float bearing;    // Angle from north (degrees)
    float elevation;  // Angle from horizontal (degrees)
};
```

### 1.4 Error Propagation and Uncertainty

**Sources of Error:**

1. **Ranging Error** (σ_range = ±10-20 cm on Arduino Uno)
   - Multipath reflections
   - Clock drift
   - Antenna delay
   - NLOS (Non-Line-of-Sight) conditions

2. **Anchor Position Error** (σ_anchor)
   - GPS measurement error: ±1-5 m (outdoor)
   - Manual measurement error: ±5-10 cm (indoor)
   - Anchor drift over time

3. **Geometric Dilution of Precision (GDOP)**
   - Poor anchor geometry amplifies ranging errors
   - Collinear anchors → high GDOP → poor accuracy
   - Optimal geometry → low GDOP → good accuracy

**Error Propagation Formula:**

Position error is amplified by GDOP:

```
σ_position = GDOP × σ_range

Where GDOP depends on anchor geometry:
- Ideal geometry: GDOP ≈ 1.0-2.0 → σ_position ≈ 10-40 cm
- Poor geometry: GDOP ≈ 5.0-10.0 → σ_position ≈ 50-200 cm
- Collinear (worst): GDOP → ∞ → solution undefined
```

**Example Calculation:**

```
Given:
- Ranging accuracy: σ_range = ±15 cm
- GDOP = 2.5 (typical indoor)

Expected position error:
σ_position = 2.5 × 15 cm = ±37.5 cm (horizontal)
σ_altitude = 3.0 × 15 cm = ±45 cm (vertical, typically worse)
```

**Covariance Matrix** (for advanced implementations):

```cpp
// Position uncertainty as 3×3 covariance matrix
struct PositionCovariance {
    float cov_xx, cov_xy, cov_xz;
    float cov_yy, cov_yz;
    float cov_zz;

    // Ellipse of uncertainty
    float getCEP() {  // Circular Error Probable (50%)
        return 0.59 * (sqrt(cov_xx) + sqrt(cov_yy));
    }
};
```

**Mitigation Strategies:**
1. Optimize anchor placement (minimize GDOP)
2. Use 4+ anchors (redundancy)
3. Filter outlier measurements (reject if error > 3σ)
4. Apply Kalman filter (temporal smoothing)
5. Use motion model (predict + correct)

---

## 2. Practical Algorithms

### 2.1 Algorithm Comparison

| Algorithm | Pros | Cons | Arduino Fit |
|-----------|------|------|-------------|
| **Linear Least Squares** | Fast, simple, deterministic | Assumes linearized model | ⭐⭐⭐⭐⭐ Best |
| **Nonlinear Least Squares** | Most accurate | Iterative, slow, may not converge | ⭐⭐ Marginal |
| **Weighted Least Squares** | Handles varying measurement quality | Requires quality metrics | ⭐⭐⭐⭐ Good |
| **Kalman Filter** | Smooth, predictive | Complex state management | ⭐⭐⭐ Feasible |
| **Particle Filter** | Handles nonlinearity well | Very computationally intensive | ⭐ Not feasible |

**Recommendation**: **Linear Least Squares** for Arduino Uno

### 2.2 Linear Least Squares Method

**Mathematical Formulation:**

Given:
- N anchors at positions (x_i, y_i, z_i)
- Measured distances d_i to each anchor
- Unknown position (x, y, z)

Linearize around an initial guess (x₀, y₀, z₀):

```
Distance equation (nonlinear):
d_i = √[(x - x_i)² + (y - y_i)² + (z - z_i)²]

Linearized equation (Taylor expansion):
d_i ≈ d_i⁰ + ∂d_i/∂x·Δx + ∂d_i/∂y·Δy + ∂d_i/∂z·Δz

Where:
d_i⁰ = √[(x₀ - x_i)² + (y₀ - y_i)² + (z₀ - z_i)²]
∂d_i/∂x = (x₀ - x_i) / d_i⁰
Δx = x - x₀ (correction to be solved)
```

**Matrix Form:**

```
H · Δp = Δd

Where:
H = Jacobian matrix (N × 3)
Δp = [Δx, Δy, Δz]ᵀ (position correction)
Δd = [d_1 - d_1⁰, ..., d_N - d_N⁰]ᵀ (residuals)

Solution (least squares):
Δp = (HᵀH)⁻¹ · Hᵀ · Δd

Update position:
p_new = p₀ + Δp

Iterate until convergence (usually 2-5 iterations)
```

**Step-by-Step Algorithm:**

```cpp
// Least Squares Multilateration
Position3D multilaterate(Position3D anchors[], float distances[], int N) {

    // Step 1: Initialize guess (centroid of anchors)
    Position3D pos = {0, 0, 0};
    for (int i = 0; i < N; i++) {
        pos.x += anchors[i].x;
        pos.y += anchors[i].y;
        pos.z += anchors[i].z;
    }
    pos.x /= N;
    pos.y /= N;
    pos.z /= N;

    // Step 2: Iterate to refine (Gauss-Newton)
    for (int iter = 0; iter < 5; iter++) {

        // Build Jacobian H and residual vector Δd
        float H[N][3];      // N×3 matrix
        float deltaD[N];    // N×1 vector

        for (int i = 0; i < N; i++) {
            // Estimated distance from current guess
            float dx = pos.x - anchors[i].x;
            float dy = pos.y - anchors[i].y;
            float dz = pos.z - anchors[i].z;
            float d_est = sqrt(dx*dx + dy*dy + dz*dz);

            // Jacobian row (partial derivatives)
            H[i][0] = dx / d_est;  // ∂d/∂x
            H[i][1] = dy / d_est;  // ∂d/∂y
            H[i][2] = dz / d_est;  // ∂d/∂z

            // Residual (measured - estimated)
            deltaD[i] = distances[i] - d_est;
        }

        // Step 3: Solve (Hᵀ·H)·Δp = Hᵀ·Δd
        float HTH[3][3];    // 3×3 matrix
        float HTdeltaD[3];  // 3×1 vector

        // Compute Hᵀ·H
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                HTH[i][j] = 0;
                for (int k = 0; k < N; k++) {
                    HTH[i][j] += H[k][i] * H[k][j];
                }
            }
        }

        // Compute Hᵀ·Δd
        for (int i = 0; i < 3; i++) {
            HTdeltaD[i] = 0;
            for (int k = 0; k < N; k++) {
                HTdeltaD[i] += H[k][i] * deltaD[k];
            }
        }

        // Step 4: Invert 3×3 matrix and solve
        float deltaP[3];
        solve3x3(HTH, HTdeltaD, deltaP);

        // Step 5: Update position
        pos.x += deltaP[0];
        pos.y += deltaP[1];
        pos.z += deltaP[2];

        // Check convergence
        float correction = sqrt(deltaP[0]*deltaP[0] +
                               deltaP[1]*deltaP[1] +
                               deltaP[2]*deltaP[2]);
        if (correction < 0.001) break;  // Converged (1mm threshold)
    }

    return pos;
}
```

**Computational Complexity:**
- Matrix operations: O(N) for N anchors
- 3×3 matrix inversion: O(1) (fixed size)
- Iterations: 2-5 typical
- **Total**: ~5-10 ms on Arduino Uno @ 16 MHz

### 2.3 Weighted Least Squares (WLS)

**Motivation:**
Not all distance measurements are equally reliable. Weight measurements by quality.

**Quality Metrics:**
1. **Signal Strength (RSSI)**: Strong signal → higher weight
2. **Measurement Variance**: Low variance → higher weight
3. **Known Anchor Quality**: Precise anchor position → higher weight

**Modified Algorithm:**

```cpp
// Weighted Least Squares
Position3D weightedMultilaterate(Position3D anchors[],
                                  float distances[],
                                  float weights[],  // NEW: quality weights
                                  int N) {

    // Build weighted system: W·H·Δp = W·Δd
    // Where W is diagonal matrix of weights

    for (int i = 0; i < N; i++) {
        // Scale Jacobian row by weight
        H[i][0] *= weights[i];
        H[i][1] *= weights[i];
        H[i][2] *= weights[i];

        // Scale residual by weight
        deltaD[i] *= weights[i];
    }

    // Solve weighted system (same process as unweighted)
    // ... (rest of algorithm unchanged)
}
```

**Weight Selection Strategies:**

```cpp
// Strategy 1: RSSI-based (signal strength)
float computeRSSIWeight(float rssi_dBm) {
    // Strong signal → weight = 1.0
    // Weak signal → weight → 0.0
    float rssi_min = -95.0;  // Threshold
    float rssi_max = -60.0;  // Good signal
    return constrain((rssi_dBm - rssi_min) / (rssi_max - rssi_min), 0.1, 1.0);
}

// Strategy 2: Variance-based
float computeVarianceWeight(float variance) {
    // Low variance → high weight
    return 1.0 / (1.0 + variance);
}

// Strategy 3: Distance-based (closer anchors more reliable)
float computeDistanceWeight(float distance) {
    // Closer → higher weight
    float d_max = 50.0;  // Max reliable range (meters)
    return exp(-distance / d_max);
}
```

**When to Use WLS:**
- Indoor environments with multipath (vary by location)
- Mixed anchor types (some more precise than others)
- Dynamic scenarios (moving anchors, varying signal quality)

**Overhead**: ~10-20% more computation than standard LS

### 2.4 Kalman Filtering

**Purpose:**
- Smooth noisy position measurements over time
- Predict future position using motion model
- Optimal for tracking moving drones

**Basic Kalman Filter for Position Tracking:**

```cpp
// State vector: [x, y, z, vx, vy, vz]
// Measurements: [x_meas, y_meas, z_meas] from multilateration

struct KalmanFilter {
    // State estimate
    float x[6];      // [x, y, z, vx, vy, vz]

    // State covariance
    float P[6][6];   // 6×6 matrix (uncertainty)

    // Process noise
    float Q[6][6];   // 6×6 matrix (model uncertainty)

    // Measurement noise
    float R[3][3];   // 3×3 matrix (sensor uncertainty)

    float dt;        // Time step (seconds)
};

void kalmanPredict(KalmanFilter* kf) {
    // State transition: x_k = F·x_{k-1}
    // F = [1  0  0  dt 0  0 ]
    //     [0  1  0  0  dt 0 ]
    //     [0  0  1  0  0  dt]
    //     [0  0  0  1  0  0 ]
    //     [0  0  0  0  1  0 ]
    //     [0  0  0  0  0  1 ]

    // Predict position using velocity
    kf->x[0] += kf->x[3] * kf->dt;  // x += vx·dt
    kf->x[1] += kf->x[4] * kf->dt;  // y += vy·dt
    kf->x[2] += kf->x[5] * kf->dt;  // z += vz·dt
    // Velocity remains constant (simple model)

    // Predict covariance: P_k = F·P_{k-1}·Fᵀ + Q
    // (Matrix operations - simplified here)
    // ... update P matrix
}

void kalmanUpdate(KalmanFilter* kf, Position3D measurement) {
    // Innovation (measurement residual)
    float y[3];
    y[0] = measurement.x - kf->x[0];
    y[1] = measurement.y - kf->x[1];
    y[2] = measurement.z - kf->x[2];

    // Kalman gain: K = P·Hᵀ·(H·P·Hᵀ + R)⁻¹
    // H = [1 0 0 0 0 0]  (measurement extracts position from state)
    //     [0 1 0 0 0 0]
    //     [0 0 1 0 0 0]

    // Update state: x = x + K·y
    // Update covariance: P = (I - K·H)·P

    // (Full matrix math omitted for brevity - see full implementation)
}
```

**Arduino Feasibility:**
- **Memory**: ~500 bytes for 6-state Kalman filter
- **Computation**: ~10-20 ms per update
- **Recommendation**: Use **simplified 3-state filter** (position only, no velocity)

**Simplified Kalman (Position-Only):**

```cpp
// Exponential moving average (simplified Kalman)
struct SimpleFilter {
    Position3D estimate;
    float alpha;  // Smoothing factor (0-1)
};

void simpleFilterUpdate(SimpleFilter* filter, Position3D measurement) {
    // α close to 1 → fast response, noisy
    // α close to 0 → smooth, lagged
    filter->estimate.x = filter->alpha * measurement.x +
                         (1 - filter->alpha) * filter->estimate.x;
    filter->estimate.y = filter->alpha * measurement.y +
                         (1 - filter->alpha) * filter->estimate.y;
    filter->estimate.z = filter->alpha * measurement.z +
                         (1 - filter->alpha) * filter->estimate.z;
}

// Typical α = 0.2-0.5 for drone positioning
```

**Recommendation for Arduino Uno:**
- Use **simple exponential filter** initially
- Migrate to **full Kalman** on flight controller or ESP32

### 2.5 Particle Filters

**Concept:**
- Represent position distribution with particles (samples)
- Propagate particles using motion model
- Weight particles by measurement likelihood
- Resample based on weights

**Particle Filter Steps:**

```
1. Initialize: Scatter N particles around initial guess
2. Predict: Move each particle according to motion model
3. Update: Compute weight for each particle based on measurement
4. Resample: Replace low-weight particles with high-weight ones
5. Estimate: Compute weighted average of particles
```

**Pros:**
- Handles nonlinear, non-Gaussian distributions
- Robust to outliers
- Can track multiple hypotheses

**Cons:**
- Computationally expensive (N particles)
- Requires many particles (100-1000) for good accuracy
- Memory intensive

**Arduino Uno Feasibility:**
- **NOT RECOMMENDED** for Arduino Uno
- Requires ESP32 or higher-end processor
- Each particle: ~16 bytes → 1000 particles = 16 KB RAM (exceeds Uno's 2 KB)

**When to Use:**
- Highly nonlinear systems
- Environments with severe multipath
- When Kalman filter diverges

---

## 3. Anchor Placement and Geometry

### 3.1 Optimal Anchor Geometry

**Goal:** Minimize GDOP (Geometric Dilution of Precision)

**Best Practices:**

1. **Surround the Target Volume**
   - Anchors should form a convex hull around operational area
   - Avoid all anchors on one side of target

2. **Maximize Angular Separation**
   - For 3 anchors: 120° separation (equilateral triangle)
   - For 4 anchors: Tetrahedral arrangement (3D) or square (2D)

3. **Vertical Diversity** (3D)
   - Place anchors at different heights
   - At least one anchor above and below target altitude

4. **Avoid Collinearity**
   - Never place anchors in a straight line
   - Minimum angle between anchors: 30° (from target perspective)

**Example Configurations:**

**2D Indoor (4 Anchors):**
```
Anchor placement for 10m × 10m room:

A1 ○──────────────────────○ A2
   │                      │
   │      Flight zone     │
   │         (Tag)        │
   │          ⊗           │
   │                      │
   │                      │
A3 ○──────────────────────○ A4

Coordinates:
A1: (0, 10, 2.5)   ← 2.5m height
A2: (10, 10, 2.5)
A3: (0, 0, 2.5)
A4: (10, 0, 2.5)

GDOP ≈ 1.5-2.0 (good)
```

**3D Outdoor (5 Anchors):**
```
Tetrahedral + center configuration:

        A5 (top)
         ○
        /│\
       / | \
      /  |  \
     /   ⊗   \  ← Target at center
    /  (Tag)  \
   /   /   \   \
  ○───────────○
 A1           A2
  \           /
   \         /
    \       /
     \     /
      \   /
       \ /
        ○
       A3

A1: (0, 0, 0)
A2: (10, 0, 0)
A3: (5, 8.66, 0)  ← Equilateral triangle base
A4: (5, 2.89, 0)  ← Center-ish
A5: (5, 2.89, 8)  ← Elevated

GDOP ≈ 1.0-1.5 (excellent)
```

### 3.2 GDOP Calculation

**Geometric Dilution of Precision (GDOP):**

```cpp
// Compute GDOP for given anchor geometry
float computeGDOP(Position3D anchors[], Position3D target, int N) {
    // Build Jacobian H at target position
    float H[N][3];

    for (int i = 0; i < N; i++) {
        float dx = target.x - anchors[i].x;
        float dy = target.y - anchors[i].y;
        float dz = target.z - anchors[i].z;
        float d = sqrt(dx*dx + dy*dy + dz*dz);

        H[i][0] = dx / d;
        H[i][1] = dy / d;
        H[i][2] = dz / d;
    }

    // Compute (Hᵀ·H)⁻¹
    float HTH[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            HTH[i][j] = 0;
            for (int k = 0; k < N; k++) {
                HTH[i][j] += H[k][i] * H[k][j];
            }
        }
    }

    float HTH_inv[3][3];
    invert3x3(HTH, HTH_inv);

    // GDOP = √(trace(HTH⁻¹))
    float gdop = sqrt(HTH_inv[0][0] + HTH_inv[1][1] + HTH_inv[2][2]);

    return gdop;
}
```

**GDOP Interpretation:**

| GDOP Value | Rating | Expected Position Error |
|------------|--------|-------------------------|
| 1.0-2.0 | Excellent | 1× ranging error |
| 2.0-4.0 | Good | 2-4× ranging error |
| 4.0-6.0 | Moderate | 4-6× ranging error |
| 6.0-10.0 | Poor | 6-10× ranging error |
| > 10.0 | Unacceptable | Position unreliable |

**Example:**
```
Ranging error: σ_range = ±15 cm
GDOP = 2.5

Expected position error:
σ_position = 2.5 × 15 cm = ±37.5 cm
```

### 3.3 Indoor vs Outdoor Configurations

**Indoor (Warehouse, Factory):**

**Challenges:**
- Multipath reflections from walls, metal objects
- NLOS (non-line-of-sight) conditions
- Limited anchor mounting locations

**Best Practices:**
- Mount anchors high (2-3m) to reduce ground reflections
- Use corners of room for maximum separation
- Add anchors if GDOP > 3.0 in operational area
- Test GDOP at multiple points in flight zone

**Example: 15m × 15m × 5m warehouse**
```
6 Anchors recommended:
- 4 in upper corners at 4.5m height
- 2 in center of long walls at 2.5m height

GDOP < 2.5 across entire flight volume
```

**Outdoor (Open Field):**

**Challenges:**
- Larger operational area (need more anchors or longer range)
- Wind and weather effects on anchor stability
- Ground reflections (less severe than indoor)

**Best Practices:**
- Use ground stakes or tripods (stable anchors critical)
- Increase anchor separation (10-30m apart)
- Elevate at least one anchor (pole or tripod)
- GPS-synchronize anchor positions if possible

**Example: 30m × 30m × 20m flight zone**
```
4-5 Anchors sufficient:
- 4 in square pattern at 30m spacing
- Optional 5th at center, elevated 5m

GDOP < 3.0 in flight zone
```

### 3.4 Handling Moving Anchors (Peer-to-Peer)

**Challenge:**
In peer-to-peer drone swarms, anchors are other drones (not stationary).

**Strategies:**

**Strategy 1: Designated Anchor Drones (Semi-Static)**
```
- 3-4 drones hover at fixed positions (anchors)
- Remaining drones use anchors for positioning
- Anchors periodically update positions via GPS or visual landmarks
- Switch anchor role when battery low
```

**Pros:** Simpler, similar to fixed anchors
**Cons:** Reduces number of active drones

**Strategy 2: Distributed Ranging with Consensus**
```
- Every drone ranges with neighbors
- Each drone computes position estimate
- Share estimates via mesh network
- Use consensus algorithm (e.g., weighted average)
```

**Pros:** Fully decentralized, robust
**Cons:** Complex, requires high-bandwidth communication

**Strategy 3: Virtual Anchors (Cooperative Positioning)**
```
- Use motion model to predict anchor positions
- Apply EKF (Extended Kalman Filter) with both ranging and motion
- Periodically re-anchor to ground truth (GPS, visual markers)
```

**Pros:** Most flexible
**Cons:** Very computationally intensive (not for Arduino Uno)

**Arduino Uno Recommendation:**
- Use **Strategy 1** (designated anchor drones)
- Keep anchor drones stationary or slow-moving
- Update anchor positions at 0.1-1 Hz (much slower than ranging)

**Position Update Protocol:**
```cpp
// Anchor broadcasts position update
struct AnchorBeacon {
    uint8_t anchor_id;
    Position3D position;
    uint32_t timestamp;
    float uncertainty;  // Position error estimate
};

// Tag receives and updates anchor table
void updateAnchorPosition(AnchorBeacon beacon) {
    for (int i = 0; i < N; i++) {
        if (anchors[i].id == beacon.anchor_id) {
            anchors[i].position = beacon.position;
            anchors[i].last_update = beacon.timestamp;
            break;
        }
    }
}
```

---

## 4. Arduino Implementation Considerations

### 4.1 Can Arduino Uno Handle Multilateration Math?

**Answer: YES, but with constraints**

**Computational Requirements:**

Operation | Time | RAM | Feasible? |
|---------|------|-----|-----------|
| 3×3 matrix inversion | ~2 ms | 50 bytes | ✓ Yes |
| Least squares (4 anchors) | ~5 ms | 200 bytes | ✓ Yes |
| Least squares (6 anchors) | ~8 ms | 300 bytes | ✓ Yes (tight) |
| Simple Kalman (3-state) | ~3 ms | 100 bytes | ✓ Yes |
| Full Kalman (6-state) | ~15 ms | 500 bytes | ⚠ Marginal |
| Particle filter (100 particles) | ~50 ms | 1.6 KB | ✗ No |

**Conclusion:**
- Linear Least Squares: **Fully feasible**
- Up to 6 anchors: **Supported**
- Simple filtering: **Feasible**
- Advanced filtering: **Offload to flight controller**

### 4.2 Memory Requirements

**RAM Budget (Arduino Uno: 2048 bytes total):**

```
Bootloader & Arduino Core:        ~400 bytes
DW1000 library state:             ~200 bytes
─────────────────────────────────────────────
Available for application:        ~1448 bytes

Multilateration Components:
- Anchor positions (6 × 16 bytes):  ~96 bytes
- Distance measurements (6 × 4):     ~24 bytes
- Working matrices (3×3, temp):     ~100 bytes
- Position state:                     ~20 bytes
- Filter state (simple):              ~50 bytes
─────────────────────────────────────────────
Subtotal:                            ~290 bytes
Remaining for other code:          ~1158 bytes
```

**Optimization Techniques:**

```cpp
// 1. Use PROGMEM for constants
const float SPEED_OF_LIGHT PROGMEM = 299792458.0;

// 2. Avoid String class (uses heap)
char buffer[32];  // Stack-allocated

// 3. Reuse buffers
float tempMatrix[3][3];  // Shared workspace

// 4. Use fixed-point math for non-critical values
int16_t x_cm;  // Store position in cm (int) vs meters (float)

// 5. Minimize global variables
// Pass by reference instead
```

### 4.3 Computation Time

**Benchmark on Arduino Uno @ 16 MHz:**

```cpp
void benchmark() {
    unsigned long t0, t1;

    // Test 1: 3×3 matrix inversion
    t0 = micros();
    invert3x3(testMatrix, resultMatrix);
    t1 = micros();
    Serial.print("Matrix inversion: ");
    Serial.print(t1 - t0);
    Serial.println(" μs");  // Expect: 1500-2000 μs

    // Test 2: Full multilateration (4 anchors, 3 iterations)
    t0 = micros();
    Position3D pos = multilaterate(anchors, distances, 4);
    t1 = micros();
    Serial.print("Multilateration: ");
    Serial.print(t1 - t0);
    Serial.println(" μs");  // Expect: 4000-6000 μs

    // Test 3: Simple filter update
    t0 = micros();
    simpleFilterUpdate(&filter, pos);
    t1 = micros();
    Serial.print("Filter update: ");
    Serial.print(t1 - t0);
    Serial.println(" μs");  // Expect: 50-100 μs
}
```

**Expected Results:**
- Multilateration: ~5 ms (allows 200 Hz theoretical max)
- Ranging cycle: ~100 ms (TDMA with 3-4 anchors)
- **Position update rate**: 10 Hz (ranging is bottleneck, not computation)

### 4.4 Libraries Available

**1. No dedicated Arduino multilateration library found**
   - Must implement from scratch
   - Code provided in this document

**2. Matrix libraries (optional):**
   - `BasicLinearAlgebra` (lightweight, 3×3 support)
   - Overhead: ~5-10 KB flash, ~100 bytes RAM
   - **Recommendation**: Implement 3×3 manually (smaller footprint)

**3. Filtering libraries:**
   - `SimpleKalmanFilter` (1D only)
   - Not suitable for 3D position
   - **Recommendation**: Implement simplified filter manually

### 4.5 Should We Offload to Flight Controller?

**Offloading Considerations:**

| Task | Arduino Uno | Flight Controller | Recommendation |
|------|-------------|-------------------|----------------|
| UWB ranging | ✓ Yes | N/A | Arduino (dedicated) |
| Distance measurement | ✓ Yes | N/A | Arduino |
| Basic multilateration | ✓ Yes | ✓ Yes | Arduino (low latency) |
| Kalman filtering | ⚠ Limited | ✓ Yes (better) | Flight controller |
| Sensor fusion | ✗ No | ✓ Yes | Flight controller |
| Path planning | ✗ No | ✓ Yes | Flight controller |
| Motor control | ✗ No | ✓ Yes | Flight controller |

**Recommended Architecture:**

```
┌─────────────────────┐
│   Arduino Uno       │
│   + DW1000 UWB      │
│                     │
│  - Ranging protocol │
│  - Distance meas    │
│  - Simple multilat  │
│  - Output position  │
└──────────┬──────────┘
           │ UART (9600 baud)
           │ Position: x, y, z
           ▼
┌─────────────────────┐
│  Flight Controller  │
│  (Pixhawk, etc)     │
│                     │
│  - Sensor fusion    │
│  - Kalman filter    │
│  - Navigation       │
│  - Motor control    │
└─────────────────────┘
```

**Communication Protocol (Arduino → FC):**

```cpp
// Send position update to flight controller via UART
void sendPositionToFC(Position3D pos, float uncertainty) {
    // Simple ASCII protocol
    Serial.print("POS ");
    Serial.print(pos.x, 2);  // 2 decimal places
    Serial.print(" ");
    Serial.print(pos.y, 2);
    Serial.print(" ");
    Serial.print(pos.z, 2);
    Serial.print(" ");
    Serial.print(uncertainty, 2);
    Serial.println();
    // Example output: "POS 1.23 4.56 2.10 0.35"
}

// Or binary protocol (more efficient)
struct PositionPacket {
    uint8_t header;      // 0xAA (sync byte)
    float x, y, z;       // Position (meters)
    float uncertainty;   // Error estimate (meters)
    uint8_t checksum;    // XOR of all bytes
} __attribute__((packed));

void sendPositionBinary(Position3D pos, float uncertainty) {
    PositionPacket pkt;
    pkt.header = 0xAA;
    pkt.x = pos.x;
    pkt.y = pos.y;
    pkt.z = pos.z;
    pkt.uncertainty = uncertainty;
    pkt.checksum = computeChecksum(&pkt, sizeof(pkt) - 1);

    Serial.write((uint8_t*)&pkt, sizeof(pkt));
}
```

**Benefits of Offloading:**
1. Arduino focuses on low-level UWB (real-time critical)
2. Flight controller handles high-level navigation (more resources)
3. Cleaner separation of concerns
4. Easier to upgrade (replace Arduino with ESP32 later)

**Recommendation:**
- **Phase 1**: Implement full position solution on Arduino (prove concept)
- **Phase 2**: Offload filtering to flight controller (production)

---

## 5. Accuracy Expectations

### 5.1 Position Accuracy with ±10-20cm Ranging Error

**Error Propagation Model:**

```
Position error (1σ) ≈ GDOP × Ranging error

Best case (GDOP = 1.5):
σ_position = 1.5 × 10 cm = 15 cm (horizontal)
σ_altitude = 1.5 × 15 cm = 22 cm (vertical, usually worse)

Typical case (GDOP = 2.5):
σ_position = 2.5 × 15 cm = 37.5 cm (horizontal)
σ_altitude = 3.0 × 15 cm = 45 cm (vertical)

Worst case (poor geometry, GDOP = 5.0):
σ_position = 5.0 × 20 cm = 100 cm (1 meter)
```

**Circular Error Probable (CEP) - 50% Confidence:**

```
CEP ≈ 0.59 × (σ_x + σ_y)

With GDOP = 2.5, ranging error = 15 cm:
CEP = 0.59 × (37.5 + 37.5) = 44 cm

Interpretation: 50% of position fixes within 44 cm of true position
```

**95% Confidence Circle:**

```
CEP95 ≈ 2.45 × σ_position

With σ_position = 37.5 cm:
CEP95 = 2.45 × 37.5 = 92 cm

Interpretation: 95% of fixes within 92 cm of true position
```

### 5.2 Effect of Anchor Geometry

**GDOP Impact on Accuracy:**

| Anchor Configuration | GDOP | Ranging Error | Position Error (1σ) |
|---------------------|------|---------------|---------------------|
| Optimal (tetrahedral) | 1.0-1.5 | ±15 cm | ±15-22 cm |
| Good (square, elevated) | 1.5-2.5 | ±15 cm | ±22-37 cm |
| Moderate (square, planar) | 2.5-4.0 | ±15 cm | ±37-60 cm |
| Poor (triangle, planar) | 4.0-6.0 | ±15 cm | ±60-90 cm |
| Bad (collinear) | > 10.0 | ±15 cm | > 150 cm (unusable) |

**Anchor Placement Guidelines:**

```cpp
// Check if anchor geometry is acceptable
bool isGeometryGood(Position3D anchors[], int N, Position3D target) {
    float gdop = computeGDOP(anchors, target, N);

    if (gdop < 2.0) {
        Serial.println("Excellent geometry");
        return true;
    } else if (gdop < 4.0) {
        Serial.println("Good geometry");
        return true;
    } else if (gdop < 6.0) {
        Serial.println("Moderate geometry - consider adding anchors");
        return true;
    } else {
        Serial.println("Poor geometry - reposition anchors!");
        return false;
    }
}
```

### 5.3 How Many Measurements Needed?

**Single-Epoch Accuracy** (one set of measurements):
- 3 anchors: ±30-60 cm (typical)
- 4 anchors: ±25-50 cm (better)
- 6 anchors: ±20-40 cm (best, with outlier rejection)

**Filtered Accuracy** (averaged over time):

```
After N measurements:
σ_filtered ≈ σ_single / √N

Examples:
1 measurement: σ = 40 cm
4 measurements (0.4 sec @ 10 Hz): σ = 40 / √4 = 20 cm
16 measurements (1.6 sec @ 10 Hz): σ = 40 / √16 = 10 cm
```

**Moving Average Filter:**

```cpp
// Rolling average of last N positions
#define FILTER_SIZE 8
Position3D positionBuffer[FILTER_SIZE];
int bufferIndex = 0;

Position3D getFilteredPosition() {
    Position3D avg = {0, 0, 0};
    for (int i = 0; i < FILTER_SIZE; i++) {
        avg.x += positionBuffer[i].x;
        avg.y += positionBuffer[i].y;
        avg.z += positionBuffer[i].z;
    }
    avg.x /= FILTER_SIZE;
    avg.y /= FILTER_SIZE;
    avg.z /= FILTER_SIZE;
    return avg;
}

void updateFilter(Position3D newPos) {
    positionBuffer[bufferIndex] = newPos;
    bufferIndex = (bufferIndex + 1) % FILTER_SIZE;
}
```

**Trade-offs:**
- More averaging → smoother, more lag
- Less averaging → responsive, noisier
- Recommended: 4-8 samples for drone control

### 5.4 Real-World Performance Expectations

**Indoor (Line-of-Sight):**
- Single-epoch: ±30-50 cm
- Filtered (1 sec): ±15-25 cm
- Sufficient for: Collision avoidance, coarse navigation

**Indoor (with Obstacles/Multipath):**
- Single-epoch: ±50-100 cm
- Filtered (1 sec): ±25-50 cm
- Challenges: NLOS bias, multipath reflections

**Outdoor (Open Field):**
- Single-epoch: ±20-40 cm
- Filtered (1 sec): ±10-20 cm
- Best case scenario (minimal multipath)

**Dynamic (Drone Moving):**
- Additional error from motion during ranging cycle
- Lag from filtering (position is 0.1-0.5 sec old)
- Kalman filter with motion model helps

**Comparison to Other Systems:**

| System | Accuracy | Update Rate | Cost |
|--------|----------|-------------|------|
| GPS (outdoor) | ±2-5 m | 1-10 Hz | $ |
| RTK GPS | ±2-5 cm | 1-20 Hz | $$$ |
| UWB (this system) | ±20-50 cm | 2-10 Hz | $$ |
| Motion Capture | ±1 mm | 100+ Hz | $$$$ |
| Visual SLAM | ±5-20 cm | 10-30 Hz | $$ |

**Conclusion:** UWB is practical middle-ground for GPS-denied drone navigation.

---

## 6. Code Examples

### 6.1 Complete Arduino Multilateration Implementation

```cpp
/**
 * Multilateration Implementation for Arduino Uno
 * Supports 3-6 anchors, 2D or 3D positioning
 */

#include <Arduino.h>

// ===== DATA STRUCTURES =====

struct Position3D {
    float x, y, z;  // Meters
};

struct Anchor {
    Position3D pos;
    uint8_t id;
    bool active;
};

// ===== CONFIGURATION =====

#define MAX_ANCHORS 6
#define MAX_ITERATIONS 5
#define CONVERGENCE_THRESHOLD 0.001  // meters

Anchor anchors[MAX_ANCHORS];
int numAnchors = 0;

// ===== MATH UTILITIES =====

// 3×3 matrix inversion (Cramer's rule)
bool invert3x3(float A[3][3], float A_inv[3][3]) {
    // Compute determinant
    float det = A[0][0] * (A[1][1]*A[2][2] - A[1][2]*A[2][1])
              - A[0][1] * (A[1][0]*A[2][2] - A[1][2]*A[2][0])
              + A[0][2] * (A[1][0]*A[2][1] - A[1][1]*A[2][0]);

    if (fabs(det) < 1e-10) {
        return false;  // Singular matrix
    }

    float invDet = 1.0 / det;

    // Compute adjugate and divide by determinant
    A_inv[0][0] = (A[1][1]*A[2][2] - A[1][2]*A[2][1]) * invDet;
    A_inv[0][1] = (A[0][2]*A[2][1] - A[0][1]*A[2][2]) * invDet;
    A_inv[0][2] = (A[0][1]*A[1][2] - A[0][2]*A[1][1]) * invDet;

    A_inv[1][0] = (A[1][2]*A[2][0] - A[1][0]*A[2][2]) * invDet;
    A_inv[1][1] = (A[0][0]*A[2][2] - A[0][2]*A[2][0]) * invDet;
    A_inv[1][2] = (A[0][2]*A[1][0] - A[0][0]*A[1][2]) * invDet;

    A_inv[2][0] = (A[1][0]*A[2][1] - A[1][1]*A[2][0]) * invDet;
    A_inv[2][1] = (A[0][1]*A[2][0] - A[0][0]*A[2][1]) * invDet;
    A_inv[2][2] = (A[0][0]*A[1][1] - A[0][1]*A[1][0]) * invDet;

    return true;
}

// Solve 3×3 linear system: A·x = b
bool solve3x3(float A[3][3], float b[3], float x[3]) {
    float A_inv[3][3];

    if (!invert3x3(A, A_inv)) {
        return false;
    }

    // x = A⁻¹ · b
    for (int i = 0; i < 3; i++) {
        x[i] = 0;
        for (int j = 0; j < 3; j++) {
            x[i] += A_inv[i][j] * b[j];
        }
    }

    return true;
}

// ===== MULTILATERATION ALGORITHM =====

Position3D multilaterate(float distances[], int N) {
    Position3D pos = {0, 0, 0};

    // Check minimum requirements
    if (N < 3) {
        Serial.println("ERROR: Need at least 3 anchors");
        return pos;
    }

    // Step 1: Initialize with centroid of anchors
    for (int i = 0; i < N; i++) {
        pos.x += anchors[i].pos.x;
        pos.y += anchors[i].pos.y;
        pos.z += anchors[i].pos.z;
    }
    pos.x /= N;
    pos.y /= N;
    pos.z /= N;

    // Step 2: Iterative refinement (Gauss-Newton)
    for (int iter = 0; iter < MAX_ITERATIONS; iter++) {

        // Build Jacobian matrix H and residual vector
        float H[MAX_ANCHORS][3];
        float deltaD[MAX_ANCHORS];

        for (int i = 0; i < N; i++) {
            // Vector from current position to anchor
            float dx = pos.x - anchors[i].pos.x;
            float dy = pos.y - anchors[i].pos.y;
            float dz = pos.z - anchors[i].pos.z;

            // Estimated distance
            float d_est = sqrt(dx*dx + dy*dy + dz*dz);

            // Avoid division by zero
            if (d_est < 0.01) d_est = 0.01;

            // Jacobian row (unit vector toward anchor)
            H[i][0] = dx / d_est;
            H[i][1] = dy / d_est;
            H[i][2] = dz / d_est;

            // Residual (measured - estimated)
            deltaD[i] = distances[i] - d_est;
        }

        // Step 3: Compute Hᵀ·H and Hᵀ·ΔD
        float HTH[3][3] = {{0}};
        float HTdeltaD[3] = {0};

        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                for (int k = 0; k < N; k++) {
                    HTH[i][j] += H[k][i] * H[k][j];
                }
            }
        }

        for (int i = 0; i < 3; i++) {
            for (int k = 0; k < N; k++) {
                HTdeltaD[i] += H[k][i] * deltaD[k];
            }
        }

        // Step 4: Solve (Hᵀ·H)·Δp = Hᵀ·ΔD
        float deltaP[3];
        if (!solve3x3(HTH, HTdeltaD, deltaP)) {
            Serial.println("WARNING: Matrix singular, using previous estimate");
            break;
        }

        // Step 5: Update position
        pos.x += deltaP[0];
        pos.y += deltaP[1];
        pos.z += deltaP[2];

        // Check convergence
        float correction = sqrt(deltaP[0]*deltaP[0] +
                               deltaP[1]*deltaP[1] +
                               deltaP[2]*deltaP[2]);

        if (correction < CONVERGENCE_THRESHOLD) {
            Serial.print("Converged in ");
            Serial.print(iter + 1);
            Serial.println(" iterations");
            break;
        }
    }

    return pos;
}

// ===== GDOP COMPUTATION =====

float computeGDOP(Position3D target) {
    // Build Jacobian at target position
    float H[MAX_ANCHORS][3];

    for (int i = 0; i < numAnchors; i++) {
        float dx = target.x - anchors[i].pos.x;
        float dy = target.y - anchors[i].pos.y;
        float dz = target.z - anchors[i].pos.z;
        float d = sqrt(dx*dx + dy*dy + dz*dz);

        if (d < 0.01) d = 0.01;

        H[i][0] = dx / d;
        H[i][1] = dy / d;
        H[i][2] = dz / d;
    }

    // Compute Hᵀ·H
    float HTH[3][3] = {{0}};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < numAnchors; k++) {
                HTH[i][j] += H[k][i] * H[k][j];
            }
        }
    }

    // Invert Hᵀ·H
    float HTH_inv[3][3];
    if (!invert3x3(HTH, HTH_inv)) {
        return 999.9;  // Error flag
    }

    // GDOP = sqrt(trace(HTH⁻¹))
    float gdop = sqrt(HTH_inv[0][0] + HTH_inv[1][1] + HTH_inv[2][2]);

    return gdop;
}

// ===== FILTERING =====

struct SimpleFilter {
    Position3D estimate;
    float alpha;  // Smoothing factor (0-1)
    bool initialized;
};

SimpleFilter posFilter = {{0, 0, 0}, 0.3, false};

Position3D filterPosition(Position3D measurement) {
    if (!posFilter.initialized) {
        posFilter.estimate = measurement;
        posFilter.initialized = true;
        return measurement;
    }

    // Exponential moving average
    posFilter.estimate.x = posFilter.alpha * measurement.x +
                           (1 - posFilter.alpha) * posFilter.estimate.x;
    posFilter.estimate.y = posFilter.alpha * measurement.y +
                           (1 - posFilter.alpha) * posFilter.estimate.y;
    posFilter.estimate.z = posFilter.alpha * measurement.z +
                           (1 - posFilter.alpha) * posFilter.estimate.z;

    return posFilter.estimate;
}

// ===== ANCHOR MANAGEMENT =====

void addAnchor(uint8_t id, float x, float y, float z) {
    if (numAnchors >= MAX_ANCHORS) {
        Serial.println("ERROR: Max anchors reached");
        return;
    }

    anchors[numAnchors].id = id;
    anchors[numAnchors].pos.x = x;
    anchors[numAnchors].pos.y = y;
    anchors[numAnchors].pos.z = z;
    anchors[numAnchors].active = true;

    numAnchors++;

    Serial.print("Added anchor ");
    Serial.print(id, HEX);
    Serial.print(" at (");
    Serial.print(x, 2); Serial.print(", ");
    Serial.print(y, 2); Serial.print(", ");
    Serial.print(z, 2); Serial.println(")");
}

// ===== MAIN POSITIONING FUNCTION =====

Position3D computePosition(float distances[], int N) {
    unsigned long t0 = micros();

    // Compute raw position
    Position3D rawPos = multilaterate(distances, N);

    // Apply filter
    Position3D filteredPos = filterPosition(rawPos);

    // Compute GDOP
    float gdop = computeGDOP(filteredPos);

    unsigned long t1 = micros();

    // Output results
    Serial.print("Position: (");
    Serial.print(filteredPos.x, 2); Serial.print(", ");
    Serial.print(filteredPos.y, 2); Serial.print(", ");
    Serial.print(filteredPos.z, 2); Serial.print(") m, GDOP: ");
    Serial.print(gdop, 1);
    Serial.print(", Time: ");
    Serial.print(t1 - t0);
    Serial.println(" μs");

    return filteredPos;
}

// ===== SETUP AND LOOP =====

void setup() {
    Serial.begin(115200);
    Serial.println("=== UWB Multilateration Demo ===");

    // Define anchor positions (example: 4 corners of 10m × 10m room)
    addAnchor(0x01, 0.0, 0.0, 2.5);    // Corner 1
    addAnchor(0x02, 10.0, 0.0, 2.5);   // Corner 2
    addAnchor(0x03, 10.0, 10.0, 2.5);  // Corner 3
    addAnchor(0x04, 0.0, 10.0, 2.5);   // Corner 4

    Serial.print("Configured ");
    Serial.print(numAnchors);
    Serial.println(" anchors");
}

void loop() {
    // Simulated distance measurements (replace with actual UWB ranging)
    float distances[4] = {
        5.12,  // Distance to anchor 1
        6.34,  // Distance to anchor 2
        7.21,  // Distance to anchor 3
        5.88   // Distance to anchor 4
    };

    // Compute position
    Position3D pos = computePosition(distances, numAnchors);

    delay(100);  // 10 Hz update rate
}
```

### 6.2 Integration with DW1000 Ranging

```cpp
/**
 * Integration: DW1000 Ranging + Multilateration
 * Tag initiates ranging with anchors, computes position
 */

#include <SPI.h>
#include "DW1000Ranging.h"

// Include multilateration code from previous section
// ... (Position3D, multilaterate, etc.)

// ===== DW1000 SETUP =====

const uint8_t PIN_RST = 9;
const uint8_t PIN_IRQ = 2;
const uint8_t PIN_SS = SS;

// Ranging results buffer
float distanceBuffer[MAX_ANCHORS];
bool distanceReady[MAX_ANCHORS];
int rangeCount = 0;

// ===== CALLBACKS =====

void newRange() {
    uint16_t anchorAddr = DW1000Ranging.getDistantDevice()->getShortAddress();
    float distance = DW1000Ranging.getDistantDevice()->getRange();

    // Find anchor by address
    for (int i = 0; i < numAnchors; i++) {
        if (anchors[i].id == anchorAddr) {
            distanceBuffer[i] = distance;
            distanceReady[i] = true;
            rangeCount++;

            Serial.print("Range to 0x");
            Serial.print(anchorAddr, HEX);
            Serial.print(": ");
            Serial.print(distance, 2);
            Serial.println(" m");
            break;
        }
    }

    // If all anchors ranged, compute position
    if (rangeCount >= numAnchors) {
        Position3D pos = computePosition(distanceBuffer, numAnchors);

        // Reset for next cycle
        rangeCount = 0;
        for (int i = 0; i < numAnchors; i++) {
            distanceReady[i] = false;
        }
    }
}

void newDevice(DW1000Device* device) {
    Serial.print("New device: 0x");
    Serial.println(device->getShortAddress(), HEX);
}

void inactiveDevice(DW1000Device* device) {
    Serial.print("Lost device: 0x");
    Serial.println(device->getShortAddress(), HEX);
}

// ===== SETUP =====

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("=== UWB Positioning Tag ===");

    // Configure anchors (must match actual anchor addresses)
    addAnchor(0x01, 0.0, 0.0, 2.5);
    addAnchor(0x02, 10.0, 0.0, 2.5);
    addAnchor(0x03, 10.0, 10.0, 2.5);
    addAnchor(0x04, 0.0, 10.0, 2.5);

    // Initialize DW1000
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachNewDevice(newDevice);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);

    // Optional: enable built-in range filter
    // DW1000Ranging.useRangeFilter(true);

    // Start as tag
    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C",
                             DW1000.MODE_LONGDATA_RANGE_ACCURACY);

    Serial.println("Tag started, ranging with anchors...");
}

void loop() {
    // DW1000Ranging library handles ranging protocol
    DW1000Ranging.loop();
}
```

### 6.3 UART Output to Flight Controller

```cpp
/**
 * Send position data to flight controller via UART
 * Compatible with MAVLink or custom protocol
 */

// ASCII protocol (human-readable, easy debugging)
void sendPositionASCII(Position3D pos, float gdop) {
    Serial.print("$POS,");
    Serial.print(pos.x, 3);
    Serial.print(",");
    Serial.print(pos.y, 3);
    Serial.print(",");
    Serial.print(pos.z, 3);
    Serial.print(",");
    Serial.print(gdop, 2);
    Serial.println();
    // Example: "$POS,5.123,3.456,2.100,2.30"
}

// Binary protocol (efficient, 17 bytes)
struct __attribute__((packed)) PositionMsg {
    uint8_t header;       // 0xAA (sync byte)
    uint32_t timestamp;   // millis()
    float x, y, z;        // Position (meters)
    float gdop;           // Geometric DOP
    uint8_t checksum;     // XOR of all bytes
};

void sendPositionBinary(Position3D pos, float gdop) {
    PositionMsg msg;
    msg.header = 0xAA;
    msg.timestamp = millis();
    msg.x = pos.x;
    msg.y = pos.y;
    msg.z = pos.z;
    msg.gdop = gdop;

    // Compute checksum
    uint8_t* bytes = (uint8_t*)&msg;
    msg.checksum = 0;
    for (int i = 0; i < sizeof(msg) - 1; i++) {
        msg.checksum ^= bytes[i];
    }

    // Send via hardware serial (TX pin)
    Serial.write(bytes, sizeof(msg));
}
```

---

## 7. Testing and Calibration

### 7.1 Testing Procedures

**Test 1: Anchor Geometry Validation**

```cpp
void testGDOP() {
    Serial.println("=== GDOP Test ===");

    // Test multiple points in flight zone
    Position3D testPoints[] = {
        {5.0, 5.0, 2.0},   // Center
        {2.0, 2.0, 2.0},   // Near corner
        {8.0, 8.0, 2.0},   // Far corner
        {5.0, 5.0, 0.5},   // Low altitude
        {5.0, 5.0, 4.0}    // High altitude
    };

    for (int i = 0; i < 5; i++) {
        float gdop = computeGDOP(testPoints[i]);
        Serial.print("Point (");
        Serial.print(testPoints[i].x, 1); Serial.print(", ");
        Serial.print(testPoints[i].y, 1); Serial.print(", ");
        Serial.print(testPoints[i].z, 1); Serial.print(") → GDOP: ");
        Serial.println(gdop, 2);
    }
}
```

**Expected Output:**
```
=== GDOP Test ===
Point (5.0, 5.0, 2.0) → GDOP: 1.80  ✓ Good
Point (2.0, 2.0, 2.0) → GDOP: 2.50  ✓ Acceptable
Point (8.0, 8.0, 2.0) → GDOP: 2.45  ✓ Acceptable
Point (5.0, 5.0, 0.5) → GDOP: 3.20  ⚠ Moderate
Point (5.0, 5.0, 4.0) → GDOP: 3.50  ⚠ Moderate
```

**Test 2: Static Position Accuracy**

```
Setup:
1. Place tag at known position (e.g., 5.0m, 5.0m, 2.0m)
2. Measure with tape measure from each anchor
3. Run ranging for 100 samples
4. Compare computed position to ground truth

Metrics:
- Mean error (bias)
- Standard deviation (precision)
- Maximum error
- CEP (circular error probable)
```

```cpp
void staticAccuracyTest() {
    Position3D groundTruth = {5.0, 5.0, 2.0};
    int numSamples = 100;

    float errors[numSamples];
    float sumError = 0;

    for (int i = 0; i < numSamples; i++) {
        // Get distances (from ranging)
        float distances[numAnchors];
        // ... perform ranging

        // Compute position
        Position3D measured = multilaterate(distances, numAnchors);

        // Calculate error
        float error = sqrt(pow(measured.x - groundTruth.x, 2) +
                          pow(measured.y - groundTruth.y, 2) +
                          pow(measured.z - groundTruth.z, 2));

        errors[i] = error;
        sumError += error;

        delay(100);  // 10 Hz
    }

    // Compute statistics
    float meanError = sumError / numSamples;

    float sumSqDev = 0;
    for (int i = 0; i < numSamples; i++) {
        sumSqDev += pow(errors[i] - meanError, 2);
    }
    float stdDev = sqrt(sumSqDev / numSamples);

    // Sort errors for percentiles
    // ... (sorting code)

    Serial.println("=== Static Accuracy Test ===");
    Serial.print("Mean error: ");
    Serial.print(meanError * 100, 1);
    Serial.println(" cm");
    Serial.print("Std deviation: ");
    Serial.print(stdDev * 100, 1);
    Serial.println(" cm");
    Serial.print("95% within: ");
    Serial.print(errors[95] * 100, 1);  // 95th percentile
    Serial.println(" cm");
}
```

**Test 3: Dynamic Tracking**

```
Setup:
1. Move tag along predefined path
2. Record ground truth positions (e.g., waypoints)
3. Log UWB positions
4. Compare trajectories

Metrics:
- Path following error
- Latency (time delay)
- Update rate consistency
```

### 7.2 Calibration Procedures

**Calibration Step 1: Anchor Positioning**

```
1. Measure anchor positions precisely
   - Use laser rangefinder or tape measure
   - Establish reference coordinate system
   - Recommended accuracy: ±5 cm

2. Update anchor positions in code
   addAnchor(0x01, 0.00, 0.00, 2.50);  // Precise values

3. Verify GDOP across flight zone
```

**Calibration Step 2: Ranging Offset**

```
Some ranging systems have systematic bias (constant offset).

Test:
1. Place tag at exactly 1.000 m from anchor
2. Measure 100 samples
3. Compute average measured distance
4. Calculate offset: offset = avgMeasured - 1.000

Apply correction:
float calibratedDistance = rawDistance - offset;
```

**Calibration Step 3: Filter Tuning**

```
Adjust filter smoothing factor (α) based on application:

Low α (0.1-0.2): Smooth, slow response
  - Good for: Hovering, slow motion
  - Bad for: Fast maneuvers

High α (0.5-0.8): Responsive, noisy
  - Good for: Agile flight, obstacle avoidance
  - Bad for: Smooth video, stable hover

Recommended starting point: α = 0.3
```

```cpp
// Test different alpha values
void tuneFilter() {
    float alphas[] = {0.1, 0.2, 0.3, 0.5, 0.7};

    for (int i = 0; i < 5; i++) {
        posFilter.alpha = alphas[i];
        posFilter.initialized = false;

        Serial.print("Testing alpha = ");
        Serial.println(alphas[i], 1);

        // Run for 10 seconds
        for (int j = 0; j < 100; j++) {
            // ... range and compute position
            delay(100);
        }

        Serial.println();
    }
}
```

### 7.3 Validation Metrics

**Position Accuracy Metrics:**

```cpp
struct AccuracyMetrics {
    float meanError;        // Average error (bias)
    float stdDev;           // Standard deviation (precision)
    float rmsError;         // Root mean square error
    float maxError;         // Worst case
    float cep50;            // 50% circular error probable
    float cep95;            // 95% CEP
};

AccuracyMetrics computeMetrics(Position3D measured[],
                                Position3D groundTruth[],
                                int N) {
    AccuracyMetrics metrics = {0};

    float sumError = 0;
    float sumSqError = 0;

    for (int i = 0; i < N; i++) {
        float error = sqrt(pow(measured[i].x - groundTruth[i].x, 2) +
                          pow(measured[i].y - groundTruth[i].y, 2) +
                          pow(measured[i].z - groundTruth[i].z, 2));

        sumError += error;
        sumSqError += error * error;

        if (error > metrics.maxError) {
            metrics.maxError = error;
        }
    }

    metrics.meanError = sumError / N;
    metrics.rmsError = sqrt(sumSqError / N);
    metrics.stdDev = sqrt((sumSqError / N) -
                          pow(metrics.meanError, 2));

    // For CEP, need to sort errors (code omitted)
    // metrics.cep50 = errors[N/2];
    // metrics.cep95 = errors[(int)(N * 0.95)];

    return metrics;
}

void printMetrics(AccuracyMetrics m) {
    Serial.println("=== Accuracy Report ===");
    Serial.print("Mean error: ");
    Serial.print(m.meanError * 100, 1); Serial.println(" cm");
    Serial.print("Std dev: ");
    Serial.print(m.stdDev * 100, 1); Serial.println(" cm");
    Serial.print("RMS error: ");
    Serial.print(m.rmsError * 100, 1); Serial.println(" cm");
    Serial.print("Max error: ");
    Serial.print(m.maxError * 100, 1); Serial.println(" cm");
    Serial.print("CEP 50%: ");
    Serial.print(m.cep50 * 100, 1); Serial.println(" cm");
    Serial.print("CEP 95%: ");
    Serial.print(m.cep95 * 100, 1); Serial.println(" cm");
}
```

**Acceptance Criteria:**

| Metric | Target (Good) | Acceptable | Poor |
|--------|---------------|------------|------|
| Mean error | < 20 cm | < 40 cm | > 40 cm |
| Std deviation | < 15 cm | < 30 cm | > 30 cm |
| RMS error | < 25 cm | < 50 cm | > 50 cm |
| CEP 95% | < 50 cm | < 100 cm | > 100 cm |
| GDOP | < 2.5 | < 4.0 | > 4.0 |

---

## 8. References

### 8.1 Academic Papers

1. **"A Survey of Positioning Systems Using Visible LED Lights"**
   - Zhuang et al., IEEE Communications Surveys & Tutorials, 2018
   - Trilateration fundamentals, GDOP analysis

2. **"Ultra-Wideband Indoor Positioning Systems"**
   - Gezici et al., IEEE Signal Processing Magazine, 2005
   - Multilateration algorithms, error analysis

3. **"Least Squares Solution of Over-Determined Set of Linear Equations"**
   - Classic numerical methods textbook reference
   - Matrix inversion techniques

### 8.2 Technical Documentation

4. **DW1000 User Manual**
   - Qorvo (formerly Decawave), Rev 2.18
   - Timestamp resolution, ranging accuracy specifications

5. **IEEE 802.15.4a-2007 Standard**
   - UWB physical layer specification
   - TWR protocol definitions

### 8.3 Implementation References

6. **arduino-dw1000 Library**
   - GitHub: thotro/arduino-dw1000
   - DW1000Ranging module provides ranging protocol

7. **SwarmLoc Project Documentation**
   - `/DWS1000_UWB/docs/roadmap.md` - Project overview
   - `/findings/UWB_Swarm_Ranging_Architecture_Research.md` - Architecture analysis

### 8.4 Online Resources

8. **"Multilateration: Theory and Practice"**
   - Aviation navigation techniques (transferable to UWB)
   - GDOP optimization strategies

9. **"Kalman Filter Tutorial"**
   - Greg Welch & Gary Bishop, UNC Chapel Hill
   - Practical Kalman filter implementation

---

## Appendix A: Quick Reference

### Minimum Anchor Requirements
- **2D**: 3 anchors (4 recommended)
- **3D**: 4 anchors (5-6 recommended)

### Best Algorithm for Arduino Uno
- **Linear Least Squares** (fast, accurate enough)

### Expected Performance
- **Position Accuracy**: ±20-50 cm (with ±10-20cm ranging)
- **Computation Time**: ~5 ms per update
- **RAM Usage**: ~200 bytes

### Optimal Anchor Placement
- Surround target volume
- 120° separation (3 anchors)
- Avoid collinearity
- GDOP < 3.0

### Filter Recommendations
- **Simple**: Exponential moving average (α = 0.3)
- **Advanced**: Offload Kalman filter to flight controller

### Position Output
```
Serial: "POS 1.23 4.56 2.10 2.35" (x, y, z, GDOP)
Update rate: 2-10 Hz (depending on ranging protocol)
```

---

**Document Version**: 1.0
**Last Updated**: 2026-01-11
**Author**: SwarmLoc Project Research
**Status**: Ready for Implementation
**Next Steps**: Implement code, test with real hardware, calibrate and validate
