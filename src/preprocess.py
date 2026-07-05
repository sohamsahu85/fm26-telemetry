"""
=============================================================================
  Formula Manipal EV Telemetry — Data Preprocessing Pipeline
  Steps 1–10 as defined in EV_Complete_Analytics_Framework_FINAL.docx
=============================================================================
  Author  : Formula Manipal Racing — Data Analytics Team
  Output  : preprocessed/<filename>_preprocessed.csv  (83 columns, 100 Hz)
            preprocessed/pipeline_report.txt           (per-file quality report)
=============================================================================
"""

import os
import sys
import warnings
import numpy as np
import pandas as pd
from scipy import signal

warnings.filterwarnings("ignore")

# ─────────────────────────────────────────────────────────────────────────────
# CONFIGURATION
# ─────────────────────────────────────────────────────────────────────────────
DATA_DIR   = r"c:\Users\sanan\OneDrive\Formula Manipal\ResearchPaper"
OUTPUT_DIR = os.path.join(DATA_DIR, "preprocessed")
os.makedirs(OUTPUT_DIR, exist_ok=True)

TARGET_HZ   = 10          # 10 Hz (100 ms) — robust for multi-rate sparse data
TARGET_DT   = 1000 / TARGET_HZ   # ms

# Vehicle constants (update when values are confirmed)
TIRE_RADIUS = 0.2286       # metres — rolling radius 9 in (18-inch tyre diameter, 10-inch rim)
GEAR_RATIO  = 5.8181818          # motor-to-wheel (placeholder)

# Physical valid ranges — confirmed vehicle specs (Formula Manipal EV)
VALID_RANGES = {
    "Motor_Speed":       (0,    6500),   # rpm — confirmed motor max
    "Module_A_Temp":     (-40,  200),    # °C — inverter IGBT thermal limit
    "Module_B_Temp":     (-40,  200),
    "Module_C_Temp":     (-40,  200),
    "Gate_Driver_Temp":  (-40,  200),
    "Control_Board_Temp":(-40,  200),
    "RTD1_Temp":         (-40,  200),    # °C — motor winding limit
    "RTD2_Temp":         (-40,  200),
    "Stall_Burst_Temp":  (-40,  200),
    "BMS_PackAvgTemp":   (-40,  100),    # °C — battery pack thermal limit
    "DC_Bus_Voltage":    (0,    378),    # V — confirmed max pack voltage (fully charged)
    "DC_Bus_Current":    (-50,  180),    # A — confirmed max discharge 180A; regen capped at -50A
    "Phase_A_Current":   (-200, 200),    # Arms — PM100DZ peak 200 Arms (10s)
    "Phase_B_Current":   (-200, 200),
    "Phase_C_Current":   (-200, 200),
    "v_mag_cog":         (0,    45),     # m/s — FSAE car practical limit (~160 km/h)
    "ax_cog":            (-30,  30),     # m/s² — ±3G physical limit
    "ay_cog":            (-30,  30),
    "Gyro_Z":            (-300, 300),    # deg/s
    "BMS_LowestCellV":   (2.5,  4.25),  # V — LiPo/NMC cell limits
    "BMS_TotalV":        (0,    378),    # V — same as DC_Bus_Voltage max
}

# ─────────────────────────────────────────────────────────────────────────────
# COLUMN NAME MAPS — normalise all generations to framework spec
# ─────────────────────────────────────────────────────────────────────────────
GEN1_RENAME = {
    "Timestamp":                "Timestamp",
    "Module A Temp":            "Module_A_Temp",
    "Module B Temp":            "Module_B_Temp",
    "Module C Temp":            "Module_C_Temp",
    "Gate Driver Temp":         "Gate_Driver_Temp",
    "Timestamp.1":              "T1_Timestamp",
    "Control Board Temp":       "Control_Board_Temp",
    "RTD #1 Temp":              "RTD1_Temp",
    "RTD #2 Temp":              "RTD2_Temp",
    "Stall Burst Model Temp":   "Stall_Burst_Temp",
    "Timestamp.2":              "MotPos_Timestamp",
    "Motor Angle":              "Motor_Angle",
    "Motor Speed":              "Motor_Speed",
    "Electrical Frequency":     "Elec_Frequency",
    "Delta Resolver Filtered":  "Delta_Resolver",
    "Timestamp.3":              "Cur_Timestamp",
    "Phase A Current":          "Phase_A_Current",
    "Phase B Current":          "Phase_B_Current",
    "Phase C Current":          "Phase_C_Current",
    "DC Bus Current":           "DC_Bus_Current",
    "Timestamp.4":              "Vol_Timestamp",
    "DC Bus Voltage":           "DC_Bus_Voltage",
    "Output Voltage":           "Output_Voltage",
    "VAB/Vd Voltage":           "VAB_Vd_Voltage",
    "VBC/Vq Voltage":           "VBC_Vq_Voltage",
    "Timestamp.5":              "Torq_Timestamp",
    "Commanded Torque":         "Commanded_Torque",
    "Torque Feedback":          "Torque_Feedback",
    "Power On Timer":           "Power_On_Timer",
}

# All 67 expected final columns (framework specification)
ALL_67_COLS = [
    "Timestamp", "T1_Timestamp",
    "Module_A_Temp", "Module_B_Temp", "Module_C_Temp", "Gate_Driver_Temp",
    "T2_Timestamp",
    "Control_Board_Temp", "RTD1_Temp", "RTD2_Temp", "Stall_Burst_Temp",
    "MotPos_Timestamp",
    "Motor_Angle", "Motor_Speed", "Elec_Frequency", "Delta_Resolver",
    "Cur_Timestamp",
    "Phase_A_Current", "Phase_B_Current", "Phase_C_Current", "DC_Bus_Current",
    "Vol_Timestamp",
    "DC_Bus_Voltage", "Output_Voltage", "VAB_Vd_Voltage", "VBC_Vq_Voltage",
    "Torq_Timestamp",
    "Commanded_Torque", "Torque_Feedback", "Power_On_Timer",
    "VN_Timestamp",
    "Yaw", "Pitch", "Roll",
    "Pos_Ex", "Pos_Ey", "Pos_Ez",
    "Vel_Ex", "Vel_Ey", "Vel_Ez",
    "Accel_X", "Accel_Y", "Accel_Z",
    "Gyro_X", "Gyro_Y", "Gyro_Z",
    "vx_cog", "vy_cog", "vz_cog", "v_mag_cog",
    "ax_cog", "ay_cog", "az_cog", "a_mag_cog",
    "BMS_Seg0", "BMS_Seg1", "BMS_Seg2", "BMS_Seg3", "BMS_Seg4",
    "BMS_TotalV", "BMS_Current", "BMS_LowestCellV",
    "BMS_LowestIC", "BMS_LowestIndex", "BMS_PackAvgTemp",
    "BMS_Latch", "BMS_FaultFlags",
]

# Columns present in 45-col intermediate format (Gen 1.5 — VN but no COG/BMS)
GEN15_VN_ONLY_COLS = [
    "VN_Timestamp", "Yaw", "Pitch", "Roll",
    "Pos_Ex", "Pos_Ey", "Pos_Ez",
    "Vel_Ex", "Vel_Ey", "Vel_Ez",
    "Accel_X", "Accel_Y", "Accel_Z",
    "Gyro_X", "Gyro_Y", "Gyro_Z",
]

# Signals that use zero-order hold (status/discrete)
ZOH_COLS = ["BMS_FaultFlags", "BMS_Latch", "BMS_LowestIC", "BMS_LowestIndex"]

# Moving average signals
MA_COLS = {
    "Motor_Speed": 5,
    "Module_A_Temp": 10, "Module_B_Temp": 10, "Module_C_Temp": 10,
    "Gate_Driver_Temp": 10, "Control_Board_Temp": 10,
    "RTD1_Temp": 10, "RTD2_Temp": 10, "Stall_Burst_Temp": 10,
    "BMS_PackAvgTemp": 10,
    "BMS_Seg0": 5, "BMS_Seg1": 5, "BMS_Seg2": 5,
    "BMS_Seg3": 5, "BMS_Seg4": 5, "BMS_TotalV": 5, "BMS_LowestCellV": 5,
}

# Butterworth filter specs: col_pattern -> (order, cutoff_hz)
BUTTER_COLS = {
    "Accel_X": (4, 20), "Accel_Y": (4, 20), "Accel_Z": (4, 20),
    "ax_cog":  (4, 20), "ay_cog":  (4, 20), "az_cog":  (4, 20),
    "Gyro_X":  (4, 18), "Gyro_Y":  (4, 18), "Gyro_Z":  (4, 18),
    "Commanded_Torque": (2, 25), "Torque_Feedback": (2, 25),
    "Phase_A_Current":  (2, 40), "Phase_B_Current": (2, 40),
    "Phase_C_Current":  (2, 40), "DC_Bus_Current":  (2, 40),
    "DC_Bus_Voltage":   (2, 15), "Output_Voltage":  (2, 15),
    "VAB_Vd_Voltage":   (2, 15), "VBC_Vq_Voltage":  (2, 15),
}

# ─────────────────────────────────────────────────────────────────────────────
# STEP 1 — Raw Data Ingestion and Validation
# ─────────────────────────────────────────────────────────────────────────────
def step1_ingest(filepath):
    """Load CSV, detect generation, rename columns, report completeness."""
    log = []
    fname = os.path.basename(filepath)
    df = pd.read_csv(filepath)
    log.append(f"[S1] File: {fname} | Raw rows: {len(df)} | Raw cols: {len(df.columns)}")

    # Detect generation by column presence
    if "Module A Temp" in df.columns:
        generation = 1          # 29-col CAN-only, old names
        df.rename(columns=GEN1_RENAME, inplace=True)
        log.append(f"[S1] Generation: 1 (29-col CAN-only, renamed to spec)")
    elif "vx_cog" in df.columns and "BMS_Seg0" in df.columns:
        generation = 3          # Full 67-col
        log.append(f"[S1] Generation: 3 (67-col full system)")
    elif "VN_Timestamp" in df.columns:
        generation = 2          # 45-col with VN but no COG/BMS
        log.append(f"[S1] Generation: 2 (45-col CAN+VN, no COG/BMS)")
    else:
        generation = 0
        log.append(f"[S1] Generation: UNKNOWN")

    # Add missing columns as NaN
    for col in ALL_67_COLS:
        if col not in df.columns:
            df[col] = np.nan

    # Reorder to canonical 67-col order
    df = df[ALL_67_COLS]

    # SIGN CONVENTION: DC-bus current sign is validated per run AFTER derivation
    # (see step: sign auto-validation) rather than hard-coded here. The motor
    # controller's rotation-direction convention can flip the apparent sign of
    # current/power; a physics check (median electrical power while the motor
    # is demonstrably spinning must be positive = discharge) detects and
    # corrects inverted runs, and flags them in 'sign_convention_corrected'.
    # NOTE: BMS_Current is measured by the BMS's own internal sensor.
        log.append(f"[S1] DC_Bus_Current sign FLIPPED (sensor wired in reverse)")

    # Timestamp monotonicity check
    ts = df["Timestamp"]
    non_mono = (ts.diff() < 0).sum()
    if non_mono > 0:
        log.append(f"[S1] WARNING: {non_mono} non-monotonic timestamps — sorting")
        df = df.sort_values("Timestamp").drop_duplicates("Timestamp")

    # Duration
    duration_s = (df["Timestamp"].iloc[-1] - df["Timestamp"].iloc[0]) / 1000
    log.append(f"[S1] Duration: {duration_s:.1f}s | Rows: {len(df)}")

    if duration_s < 10:
        log.append(f"[S1] ABORT: Recording < 10s, likely corrupt.")
        return None, log, generation

    # Completeness per column
    completeness = (df.notna().sum() / len(df) * 100).round(1)
    critical_cols = ["Timestamp", "Motor_Speed", "DC_Bus_Voltage",
                     "DC_Bus_Current", "Commanded_Torque", "Torque_Feedback"]
    for c in critical_cols:
        pct = completeness.get(c, 0)
        if pct < 80:
            log.append(f"[S1] WARNING: Critical column '{c}' only {pct}% complete")

    log.append(f"[S1] Column completeness (non-null): "
               f"min={completeness.min():.0f}%, max={completeness.max():.0f}%")

    return df, log, generation


# ─────────────────────────────────────────────────────────────────────────────
# STEP 2 — Timestamp Synchronization
# ─────────────────────────────────────────────────────────────────────────────
def step2_timestamp_sync(df, log):
    """Align all module timestamps to master Timestamp column."""
    module_ts_cols = [
        "T1_Timestamp", "T2_Timestamp", "MotPos_Timestamp",
        "Cur_Timestamp", "Vol_Timestamp", "Torq_Timestamp", "VN_Timestamp",
    ]
    master = df["Timestamp"].values.astype(float)

    for col in module_ts_cols:
        if df[col].notna().sum() < 2:
            continue
        mod_ts = df[col].values.astype(float)
        # Calculate initial offset (first valid sample)
        first_valid = np.where(~np.isnan(mod_ts))[0]
        if len(first_valid) == 0:
            continue
        i0 = first_valid[0]
        dt_start = mod_ts[i0] - master[i0]

        # Check for drift at end
        last_valid = np.where(~np.isnan(mod_ts))[0][-1]
        dt_end = mod_ts[last_valid] - master[last_valid]
        n = len(master)

        # Apply linear drift correction
        drift = np.linspace(dt_start, dt_end, n)
        df[col] = mod_ts - drift

    log.append(f"[S2] Timestamp sync complete — all module clocks aligned to master")
    return df, log


# ─────────────────────────────────────────────────────────────────────────────
# STEP 3 — Resampling to Common Frequency
# ─────────────────────────────────────────────────────────────────────────────
def step3_resample(df, log, target_hz=TARGET_HZ):
    """Resample all channels to a uniform time grid at target_hz."""
    t_start = df["Timestamp"].iloc[0]
    t_end   = df["Timestamp"].iloc[-1]
    t_new   = np.arange(t_start, t_end, 1000 / target_hz)   # ms grid

    df_out = pd.DataFrame({"Timestamp": t_new})

    # Merge on nearest timestamp (tolerance = 2 × expected Δt)
    df_sorted = df.set_index("Timestamp").sort_index()

    for col in ALL_67_COLS[1:]:   # skip Timestamp itself
        if df[col].notna().sum() < 2:
            df_out[col] = np.nan
            continue

        series = df_sorted[col].dropna()
        if col in ZOH_COLS:
            # Zero-order hold: forward-fill after reindex
            reindexed = series.reindex(
                series.index.union(t_new)
            ).ffill().reindex(t_new)
        else:
            # Linear interpolation
            reindexed = series.reindex(
                series.index.union(t_new)
            ).interpolate(method="index").reindex(t_new)

        df_out[col] = reindexed.values

    log.append(f"[S3] Resampled to {target_hz} Hz → {len(df_out)} rows "
               f"(Δt = {1000/target_hz:.0f} ms)")
    return df_out, log


# ─────────────────────────────────────────────────────────────────────────────
# STEP 4 — Missing Data Handling
# ─────────────────────────────────────────────────────────────────────────────
def step4_missing(df, log):
    """Classify and handle gaps; mark long gaps as invalid."""
    expected_dt = TARGET_DT          # ms
    gap_threshold_short  = 100       # ms
    gap_threshold_medium = 2000      # ms

    ts = df["Timestamp"].values
    dt = np.diff(ts)
    gap_mask = dt > 2 * expected_dt

    short_gaps  = ((dt > 2*expected_dt) & (dt <= gap_threshold_short)).sum()
    medium_gaps = ((dt > gap_threshold_short) & (dt <= gap_threshold_medium)).sum()
    long_gaps   = (dt > gap_threshold_medium).sum()
    total_gap_s = dt[gap_mask].sum() / 1000

    log.append(f"[S4] Gaps — short(<100ms): {short_gaps}, "
               f"medium(100ms-2s): {medium_gaps}, long(>2s): {long_gaps}")
    log.append(f"[S4] Total gap time: {total_gap_s:.1f}s")

    # Mark data in long-gap segments as NaN (do not interpolate across >2s gaps)
    gap_indices = np.where(dt > gap_threshold_medium)[0]
    dynamic_cols = [c for c in ALL_67_COLS
                    if c not in ZOH_COLS and c != "Timestamp"]

    for idx in gap_indices:
        # Blank out 1 row on either side of long gap
        window = min(5, len(df)-1)
        for w in range(window):
            if idx + 1 + w < len(df):
                df.loc[df.index[idx + 1 + w], dynamic_cols] = np.nan

    # Detect stuck values (std == 0 over 50+ consecutive samples)
    for col in ["DC_Bus_Current", "DC_Bus_Voltage", "Motor_Speed",
                "ax_cog", "ay_cog", "Phase_A_Current"]:
        if df[col].notna().sum() < 50:
            continue
        rolling_std = df[col].rolling(50).std()
        stuck = (rolling_std == 0) & df[col].notna()
        if stuck.sum() > 0:
            df.loc[stuck, col] = np.nan
            log.append(f"[S4] Stuck value detected in '{col}': {stuck.sum()} samples → NaN")

    return df, log


# ─────────────────────────────────────────────────────────────────────────────
# STEP 5 — Noise Filtering
# ─────────────────────────────────────────────────────────────────────────────
def _butter_filter(series, order, cutoff_hz, fs):
    """Apply zero-phase Butterworth low-pass filter to a pandas Series."""
    nyq = fs / 2
    Wn  = min(cutoff_hz / nyq, 0.99)   # clamp to valid range
    b, a = signal.butter(order, Wn, btype="low")
    valid = series.dropna()
    if len(valid) < max(3*order + 1, 15):
        return series
    filtered = signal.filtfilt(b, a, valid.values)
    out = series.copy()
    out.loc[valid.index] = filtered
    return out


def step5_filter(df, log):
    """Apply Butterworth filters and moving averages per framework Table."""
    fs = TARGET_HZ

    # Butterworth filters
    for col, (order, cutoff) in BUTTER_COLS.items():
        if col in df.columns and df[col].notna().sum() > 20:
            df[col] = _butter_filter(df[col], order, cutoff, fs)

    # Moving averages
    for col, n in MA_COLS.items():
        if col in df.columns and df[col].notna().sum() > n:
            df[col] = df[col].rolling(n, center=True, min_periods=1).mean()

    log.append(f"[S5] Noise filtering complete — Butterworth on {len(BUTTER_COLS)} channels, "
               f"MA on {len(MA_COLS)} channels")
    return df, log


# ─────────────────────────────────────────────────────────────────────────────
# STEP 6 — Unit Conversion and Standardisation
# ─────────────────────────────────────────────────────────────────────────────
def step6_units(df, log):
    """Convert units to SI where needed (for internal calculations)."""
    # Motor_Speed: rpm → rad/s stored as Motor_Speed_rads (keep original rpm for display)
    if "Motor_Speed" in df.columns:
        df["Motor_Speed_rads"] = df["Motor_Speed"] * (2 * np.pi / 60)

    # Gyro: deg/s → rad/s (for calculations)
    for col in ["Gyro_X", "Gyro_Y", "Gyro_Z"]:
        if col in df.columns:
            df[col + "_rads"] = df[col] * (np.pi / 180)

    # Angles: deg → rad (for calculations)
    for col in ["Yaw", "Pitch", "Roll", "Motor_Angle"]:
        if col in df.columns:
            df[col + "_rad"] = df[col] * (np.pi / 180)

    # Timestamp: ms → s (utility column for integration)
    df["t_sec"] = (df["Timestamp"] - df["Timestamp"].iloc[0]) / 1000

    log.append(f"[S6] Unit conversion done — added Motor_Speed_rads, Gyro_*_rads, "
               f"*_rad angles, t_sec")
    return df, log


# ─────────────────────────────────────────────────────────────────────────────
# STEP 7 — Outlier Detection and Removal
# ─────────────────────────────────────────────────────────────────────────────
def step7_outliers(df, log):
    """Physical range clip + Modified Z-Score on dynamic signals."""
    total_replaced = 0

    # Physical range validation
    for col, (lo, hi) in VALID_RANGES.items():
        if col not in df.columns:
            continue
        oob = (df[col] < lo) | (df[col] > hi)
        n_oob = oob.sum()
        if n_oob > 0:
            df.loc[oob, col] = np.nan
            total_replaced += n_oob

    # Modified Z-Score on dynamic signals
    dynamic_outlier_cols = [
        "ax_cog", "ay_cog", "DC_Bus_Current",
        "Commanded_Torque", "Torque_Feedback",
        "Phase_A_Current", "Phase_B_Current", "Phase_C_Current",
    ]
    for col in dynamic_outlier_cols:
        if col not in df.columns or df[col].notna().sum() < 10:
            continue
        vals = df[col].dropna()
        med = vals.median()
        mad = (np.abs(vals - med)).median()
        if mad == 0:
            continue
        mod_z = 0.6745 * (df[col] - med) / mad
        outliers = np.abs(mod_z) > 3.5
        n_stat = outliers.sum()
        if n_stat > 0:
            df.loc[outliers, col] = np.nan
            total_replaced += n_stat

    # Re-interpolate small NaN gaps created by outlier removal
    for col in ALL_67_COLS[1:]:
        if col in ZOH_COLS:
            df[col] = df[col].ffill()
        else:
            df[col] = df[col].interpolate(method="linear",
                                          limit=10, limit_direction="both")

    log.append(f"[S7] Outlier removal — {total_replaced} values replaced with NaN "
               f"(range + Modified Z-Score), then re-interpolated")
    return df, log


# ─────────────────────────────────────────────────────────────────────────────
# STEP 8 — Derived Column Calculation
# ─────────────────────────────────────────────────────────────────────────────
def step8_derived(df, log):
    """Calculate all 16 derived columns defined in the framework."""
    dt = df["t_sec"].diff().fillna(1 / TARGET_HZ)    # seconds

    # D1: Electrical power (W)
    if all(c in df for c in ["DC_Bus_Voltage", "DC_Bus_Current"]):
        df["power_electrical"] = df["DC_Bus_Voltage"] * df["DC_Bus_Current"]
    else:
        df["power_electrical"] = np.nan

    # D2: Mechanical power (W)
    if all(c in df for c in ["Torque_Feedback", "Motor_Speed_rads"]):
        df["power_mechanical"] = df["Torque_Feedback"] * df["Motor_Speed_rads"]
    else:
        df["power_mechanical"] = np.nan

    # D3: Drivetrain efficiency (%)
    with np.errstate(divide="ignore", invalid="ignore"):
        df["efficiency"] = np.where(
            df["power_electrical"].abs() > 0.1,
            df["power_mechanical"] / df["power_electrical"] * 100,
            np.nan
        )
    # Clip efficiency to physically meaningful range
    df["efficiency"] = df["efficiency"].clip(-5, 105)

    # D4: Cumulative electrical energy (J)
    df["energy_cumulative"] = (df["power_electrical"].fillna(0) * dt).cumsum()

    # D5: Cumulative regen energy (J)
    regen_power = df["power_electrical"].where(
        df["DC_Bus_Current"].fillna(0) < 0, 0
    ).abs()
    df["energy_regen_cumulative"] = (regen_power.fillna(0) * dt).cumsum()

    # D5b: SIGN AUTO-VALIDATION — physics check per run.
    # While the motor is demonstrably spinning (|Motor_Speed| > 500 rpm),
    # median electrical power must be positive (discharge). If negative, the
    # run's sign convention is inverted: flip current/power, recompute the
    # energy chains, and flag the correction for traceability.
    df["sign_convention_corrected"] = False
    if "Motor_Speed" in df.columns:
        moving = df["Motor_Speed"].fillna(0).abs() > 500
        if moving.sum() >= 10 and df.loc[moving, "power_electrical"].median() < 0:
            for c in ("DC_Bus_Current", "power_electrical"):
                if c in df.columns:
                    df[c] = -df[c]
            df["energy_cumulative"] = (df["power_electrical"].fillna(0) * dt).cumsum()
            regen_power = df["power_electrical"].where(
                df["DC_Bus_Current"].fillna(0) < 0, 0).abs()
            df["energy_regen_cumulative"] = (regen_power.fillna(0) * dt).cumsum()
            df["sign_convention_corrected"] = True
        if moving.sum() >= 10 and "power_mechanical" in df.columns \
           and df.loc[moving, "power_mechanical"].median() < 0:
            df["power_mechanical"] = -df["power_mechanical"]
            df["sign_convention_corrected"] = True

    # D6: Drivetrain slip (dimensionless)
    if all(c in df for c in ["Motor_Speed_rads", "v_mag_cog"]):
        wheel_speed = df["Motor_Speed_rads"] * TIRE_RADIUS / GEAR_RATIO  # m/s
        denom = df["v_mag_cog"].replace(0, np.nan)
        df["drivetrain_slip"] = (wheel_speed - df["v_mag_cog"]) / denom
        df["drivetrain_slip"] = df["drivetrain_slip"].clip(-1, 5)
    else:
        df["drivetrain_slip"] = np.nan

    # D7: Grip utilisation (%)
    if all(c in df for c in ["ax_cog", "ay_cog"]):
        g_total = np.sqrt(df["ax_cog"]**2 + df["ay_cog"]**2)
        g_peak  = g_total.quantile(0.99)
        df["grip_utilization"] = (g_total / g_peak * 100).clip(0, 100) \
                                  if g_peak > 0 else np.nan
    else:
        df["grip_utilization"] = np.nan

    # D8: Battery segment voltage spread (V)
    seg_cols = [c for c in ["BMS_Seg0","BMS_Seg1","BMS_Seg2","BMS_Seg3","BMS_Seg4"]
                if c in df and df[c].notna().any()]
    if seg_cols:
        df["segment_spread"] = df[seg_cols].max(axis=1) - df[seg_cols].min(axis=1)
    else:
        df["segment_spread"] = np.nan

    # D9: Phase current imbalance (%)
    ph_cols = [c for c in ["Phase_A_Current","Phase_B_Current","Phase_C_Current"]
               if c in df and df[c].notna().any()]
    if len(ph_cols) == 3:
        ph = df[ph_cols].abs()
        ph_max  = ph.max(axis=1)
        ph_min  = ph.min(axis=1)
        ph_mean = ph.mean(axis=1).replace(0, np.nan)
        df["phase_imbalance"] = (ph_max - ph_min) / ph_mean * 100
    else:
        df["phase_imbalance"] = np.nan

    # D10: RTD temperature imbalance (°C)
    if all(c in df for c in ["RTD1_Temp", "RTD2_Temp"]):
        df["RTD_imbalance"] = (df["RTD1_Temp"] - df["RTD2_Temp"]).abs()
    else:
        df["RTD_imbalance"] = np.nan

    # D11: Cumulative distance (m) — primary GNSS/COG speed, motor-speed fallback
    #      Fallback validated within ~6% of GNSS speed on dual-source runs.
    if "v_mag_cog" in df and df["v_mag_cog"].notna().any():
        _spd = df["v_mag_cog"].fillna(0)
        df["distance_source"] = "gnss_cog"
    elif "Motor_Speed_rads" in df and df["Motor_Speed_rads"].notna().any():
        _spd = (df["Motor_Speed_rads"] * TIRE_RADIUS / GEAR_RATIO).fillna(0)
        df["distance_source"] = "motor_estimate"
    else:
        _spd = None
        df["distance_source"] = "none"
    if _spd is not None:
        df["ground_speed_ms"] = _spd
        df["distance_cumulative"] = (_spd * dt).cumsum()
    else:
        df["distance_cumulative"] = np.nan

    # D12: Torque rate (Nm/s)
    if "Commanded_Torque" in df:
        df["torque_rate"] = df["Commanded_Torque"].diff() / dt
        df["torque_rate"] = df["torque_rate"].clip(-5000, 5000)
    else:
        df["torque_rate"] = np.nan

    # D13: Is braking (boolean)
    if "ax_cog" in df:
        df["is_braking"] = (df["ax_cog"] < -2.0).astype(float)
    else:
        df["is_braking"] = np.nan

    # D14: Is regen (boolean)
    if "DC_Bus_Current" in df:
        df["is_regen"] = (df["DC_Bus_Current"] < 0).astype(float)
    else:
        df["is_regen"] = np.nan

    # D15 & D16: Lap number and lap time — placeholder (GPS crossing not done yet)
    # Lap segmentation via GPS requires a specific start/finish coordinate.
    # Set to NaN for now; post-processing can apply coordinate-based detection.
    df["lap_number"] = np.nan
    df["lap_time"]   = np.nan

    log.append(f"[S8] Derived columns added: power_electrical, power_mechanical, efficiency, "
               f"energy_cumulative, energy_regen_cumulative, drivetrain_slip, "
               f"grip_utilization, segment_spread, phase_imbalance, RTD_imbalance, "
               f"distance_cumulative, torque_rate, is_braking, is_regen, lap_number, lap_time")
    return df, log


# ─────────────────────────────────────────────────────────────────────────────
# STEP 9 — Lap Segmentation (GPS crossing)
# ─────────────────────────────────────────────────────────────────────────────
def step9_lap_segment(df, log, sf_x1=None, sf_y1=None, sf_x2=None, sf_y2=None):
    """
    Detect lap crossings using Pos_Ex / Pos_Ey and a start/finish gate line.
    If no start/finish coordinates are provided, skips segmentation.
    Coordinates are in the ENU ECEF frame (metres, same as Pos_Ex/Pos_Ey).
    """
    if "Pos_Ex" not in df.columns or df["Pos_Ex"].notna().sum() < 10:
        log.append("[S9] Lap segmentation skipped — no GPS data available in this file")
        return df, log

    if any(v is None for v in [sf_x1, sf_y1, sf_x2, sf_y2]):
        log.append("[S9] Lap segmentation skipped — start/finish gate not configured. "
                   "Set sf_x1,sf_y1,sf_x2,sf_y2 to enable.")
        return df, log

    # Signed distance from start/finish line
    px = df["Pos_Ex"].values
    py = df["Pos_Ey"].values
    spd = df.get("v_mag_cog", pd.Series(np.ones(len(df)))).fillna(0).values

    dx = sf_x2 - sf_x1
    dy = sf_y2 - sf_y1

    signed_dist = (py - sf_y1) * dx - (px - sf_x1) * dy

    lap_num  = np.zeros(len(df), dtype=float)
    lap_time = np.full(len(df), np.nan)

    current_lap   = 1
    last_cross_idx = 0

    for i in range(1, len(df)):
        crossed = (np.sign(signed_dist[i]) != np.sign(signed_dist[i-1]))
        moving  = spd[i] > 2.0     # > 2 m/s to avoid false triggers
        if crossed and moving:
            # Mark lap times for this completed lap
            lt = (df["Timestamp"].iloc[i] - df["Timestamp"].iloc[last_cross_idx]) / 1000
            if 10 < lt < 600:      # sanity: 10s–10min
                lap_time[last_cross_idx:i] = lt
            lap_num[i:] = current_lap
            current_lap += 1
            last_cross_idx = i

    df["lap_number"] = lap_num
    df["lap_time"]   = lap_time

    n_laps = int(df["lap_number"].max())
    log.append(f"[S9] Lap segmentation complete — {n_laps} laps detected")
    return df, log


# ─────────────────────────────────────────────────────────────────────────────
# STEP 10 — Final Validation and Export
# ─────────────────────────────────────────────────────────────────────────────
DERIVED_COLS = [
    "Motor_Speed_rads", "Gyro_X_rads", "Gyro_Y_rads", "Gyro_Z_rads",
    "Yaw_rad", "Pitch_rad", "Roll_rad", "Motor_Angle_rad", "t_sec",
    "power_electrical", "power_mechanical", "efficiency",
    "energy_cumulative", "energy_regen_cumulative", "drivetrain_slip",
    "grip_utilization", "segment_spread", "phase_imbalance", "RTD_imbalance",
    "distance_cumulative", "torque_rate", "is_braking", "is_regen",
    "lap_number", "lap_time",
]

def step10_validate_export(df, log, out_path):
    """Run final validation checks and export the analysis-ready CSV."""
    checks = []

    # Check 1: Uniform Δt
    dt_arr = df["Timestamp"].diff().dropna()
    dt_mean = dt_arr.mean()
    dt_std  = dt_arr.std()
    expected_dt = 1000 / TARGET_HZ
    checks.append(("Uniform Δt", dt_std < 2.0,
                   f"mean={dt_mean:.2f}ms std={dt_std:.2f}ms (expect {expected_dt:.0f}ms)"))

    # Check 2: NaN in critical columns
    critical = ["DC_Bus_Voltage", "DC_Bus_Current"]
    for c in critical:
        if c in df.columns:
            nan_pct = df[c].isna().sum() / len(df) * 100
            checks.append((f"NaN in {c}", nan_pct < 20,
                           f"{nan_pct:.1f}% NaN"))

    # Check 3: Derived columns populated
    for c in ["power_electrical", "energy_cumulative"]:
        if c in df.columns:
            nan_pct = df[c].isna().sum() / len(df) * 100
            checks.append((f"Derived '{c}'", nan_pct < 20,
                           f"{nan_pct:.1f}% NaN"))

    # Check 4: Valid data > 90%
    total_valid = df[ALL_67_COLS[1:]].notna().mean().mean() * 100
    checks.append(("Overall data valid >80%", total_valid > 80,
                   f"{total_valid:.1f}% overall non-null"))

    log.append(f"[S10] Final validation:")
    for name, passed, detail in checks:
        status = "PASS" if passed else "WARN"
        log.append(f"  [{status}] {name}: {detail}")

    # Total rows and columns
    n_rows = len(df)
    n_cols = len(df.columns)
    log.append(f"[S10] Output: {n_rows} rows × {n_cols} cols → {os.path.basename(out_path)}")

    # Export
    df.to_csv(out_path, index=False)

    return df, log


# ─────────────────────────────────────────────────────────────────────────────
# MASTER PIPELINE — Run all 10 steps on one file
# ─────────────────────────────────────────────────────────────────────────────
def run_pipeline(filepath, sf_coords=None):
    """
    Run the full 10-step preprocessing pipeline on a single CSV file.

    sf_coords : dict with keys sf_x1, sf_y1, sf_x2, sf_y2 for lap detection
                (leave None to skip lap segmentation)
    Returns   : (df_clean, log_lines) or (None, log_lines) if aborted
    """
    fname = os.path.basename(filepath)
    out_path = os.path.join(OUTPUT_DIR, fname.replace(".csv", "_preprocessed.csv"))
    log = [f"\n{'='*60}", f"  PIPELINE: {fname}", f"{'='*60}"]

    # Step 1
    df, log, gen = step1_ingest(filepath)
    if df is None:
        return None, log

    # Step 2
    df, log = step2_timestamp_sync(df, log)

    # Step 3
    df, log = step3_resample(df, log)

    # Step 4
    df, log = step4_missing(df, log)

    # Step 5
    df, log = step5_filter(df, log)

    # Step 6
    df, log = step6_units(df, log)

    # Step 7
    df, log = step7_outliers(df, log)

    # Step 8
    df, log = step8_derived(df, log)

    # Step 9
    if sf_coords:
        df, log = step9_lap_segment(df, log, **sf_coords)
    else:
        df, log = step9_lap_segment(df, log)

    # Step 10
    df, log = step10_validate_export(df, log, out_path)

    return df, log


# ─────────────────────────────────────────────────────────────────────────────
# BATCH RUNNER — Process all (or selected) CSV files
# ─────────────────────────────────────────────────────────────────────────────
def batch_run(file_filter=None, sf_coords=None):
    """
    Process all CSV files in DATA_DIR through the pipeline.

    file_filter : list of filenames to process (e.g. ['can_data14.csv'])
                  or None to process all files.
    sf_coords   : optional dict for lap segmentation
                  e.g. {"sf_x1": 12.5, "sf_y1": -8.3, "sf_x2": 13.1, "sf_y2": -8.8}
    """
    all_csvs = sorted([f for f in os.listdir(DATA_DIR) if f.endswith(".csv")])

    if file_filter:
        all_csvs = [f for f in all_csvs if f in file_filter]

    all_logs = []
    summary  = []

    print(f"\nFormula Manipal EV — Preprocessing Pipeline")
    print(f"Target frequency : {TARGET_HZ} Hz")
    print(f"Output directory : {OUTPUT_DIR}")
    print(f"Files to process : {len(all_csvs)}\n")

    for idx, fname in enumerate(all_csvs, 1):
        fpath = os.path.join(DATA_DIR, fname)
        print(f"[{idx}/{len(all_csvs)}] Processing {fname} ...", end=" ", flush=True)

        try:
            df, log = run_pipeline(fpath, sf_coords=sf_coords)
            if df is not None:
                n_rows = len(df)
                dur    = (df["Timestamp"].iloc[-1] - df["Timestamp"].iloc[0]) / 1000
                print(f"OK — {n_rows} rows, {dur:.0f}s, {len(df.columns)} cols")
                summary.append(f"OK  | {fname:30s} | {n_rows:6d} rows | {dur:7.1f}s | {len(df.columns)} cols")
            else:
                print(f"SKIPPED (too short / corrupt)")
                summary.append(f"SKIP| {fname:30s} | aborted")
        except Exception as e:
            print(f"ERROR — {e}")
            summary.append(f"ERR | {fname:30s} | {str(e)[:60]}")
            log = [f"ERROR in {fname}: {e}"]

        all_logs.extend(log)

    # Write full report
    report_path = os.path.join(OUTPUT_DIR, "pipeline_report.txt")
    with open(report_path, "w", encoding="utf-8") as f:
        f.write("FORMULA MANIPAL EV — PREPROCESSING PIPELINE REPORT\n")
        f.write("="*70 + "\n\n")
        f.write("SUMMARY\n" + "-"*40 + "\n")
        f.write("\n".join(summary))
        f.write("\n\n" + "DETAILED LOGS\n" + "-"*40 + "\n")
        f.write("\n".join(all_logs))

    print(f"\n{'='*60}")
    print(f"Done. Report saved to: {report_path}")
    print(f"Preprocessed CSVs in: {OUTPUT_DIR}")
    print(f"{'='*60}\n")
    print("SUMMARY:")
    for s in summary:
        print(" ", s)


# ─────────────────────────────────────────────────────────────────────────────
# ENTRY POINT
# ─────────────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    # ── Option A: Process ALL files ──
    batch_run()

    # ── Option B: Process specific files only ──
    # batch_run(file_filter=["can_data14.csv", "can_data15.csv", "can_data21.csv"])

    # ── Option C: With lap segmentation (set your actual GPS coordinates) ──
    # batch_run(sf_coords={
    #     "sf_x1":  12.5,   # Pos_Ex of start/finish line point 1 (metres)
    #     "sf_y1":  -8.3,   # Pos_Ey of start/finish line point 1
    #     "sf_x2":  13.1,   # Pos_Ex of start/finish line point 2
    #     "sf_y2":  -8.8,   # Pos_Ey of start/finish line point 2
    # })
