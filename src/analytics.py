"""
=============================================================================
  Formula Manipal EV Telemetry — Batch Analytics Engine (T1-T12)
  Runs all 12 analytics techniques across ALL preprocessed CSV files.
=============================================================================
  Output:
    analysis_results/kpi_all_sessions.csv    -- one row per file, all KPIs
    analysis_results/plots/                  -- per-file + aggregate plots
    analysis_results/analysis_report.txt     -- narrative summary
=============================================================================
"""

import os
import warnings
import numpy as np
import pandas as pd
from scipy import signal, stats
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import LogNorm

warnings.filterwarnings("ignore")

# ─────────────────────────────────────────────────────────────────────────────
# CONFIGURATION
# ─────────────────────────────────────────────────────────────────────────────
PREPROCESSED_DIR = r"c:\Users\sanan\OneDrive\Formula Manipal\ResearchPaper\preprocessed"
OUTPUT_DIR       = r"c:\Users\sanan\OneDrive\Formula Manipal\ResearchPaper\analysis_results"
PLOTS_DIR        = os.path.join(OUTPUT_DIR, "plots")
os.makedirs(OUTPUT_DIR, exist_ok=True)
os.makedirs(PLOTS_DIR, exist_ok=True)

TARGET_HZ = 10          # Hz — matches preprocessing
MIN_DURATION_S = 30     # skip files shorter than this for analytics

# Only generate individual detailed plots for sessions longer than this
DETAIL_PLOT_THRESHOLD_S = 120

plt.rcParams.update({
    "figure.dpi": 150,
    "font.family": "DejaVu Sans",
    "font.size": 9,
    "axes.titlesize": 10,
    "axes.labelsize": 9,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "axes.grid": True,
    "grid.alpha": 0.3,
    "lines.linewidth": 1.2,
    "figure.facecolor": "white",
})

COLORS = {
    "primary":  "#2E86AB",
    "warning":  "#E84855",
    "neutral":  "#6B6B6B",
    "accent":   "#F18F01",
    "success":  "#44BBA4",
    "purple":   "#7B2D8B",
}

# ─────────────────────────────────────────────────────────────────────────────
# HELPERS
# ─────────────────────────────────────────────────────────────────────────────
def load(filepath):
    df = pd.read_csv(filepath)
    if "t_sec" not in df.columns:
        df["t_sec"] = (df["Timestamp"] - df["Timestamp"].iloc[0]) / 1000
    return df

def col_ok(df, *cols):
    return all(c in df.columns and df[c].notna().sum() > 10 for c in cols)

def save_fig(fig, name):
    path = os.path.join(PLOTS_DIR, f"{name}.png")
    fig.savefig(path, bbox_inches="tight")
    plt.close(fig)

# ─────────────────────────────────────────────────────────────────────────────
# PER-FILE ANALYTICS — returns a flat dict of KPIs for this session
# ─────────────────────────────────────────────────────────────────────────────
def analyze_file(df, fname, make_plots=False):
    """
    Run all 12 techniques on a single preprocessed DataFrame.
    Returns a dict of KPIs. Plots only if make_plots=True.
    """
    tag = fname.replace("_preprocessed.csv", "")
    kpi = {"file": fname,
           "duration_s": round(float(df["t_sec"].iloc[-1]), 1),
           "rows": len(df)}
    t = df["t_sec"]

    # ── T1: Time Series — peaks and ranges ───────────────────────────────────
    for col in ["Motor_Speed", "DC_Bus_Voltage", "DC_Bus_Current",
                "Module_A_Temp", "Module_B_Temp", "Module_C_Temp",
                "RTD1_Temp", "RTD2_Temp", "v_mag_cog"]:
        if col_ok(df, col):
            s = df[col].dropna()
            kpi[f"T1_max_{col}"] = round(s.max(), 2)
            kpi[f"T1_min_{col}"] = round(s.min(), 2)

    if make_plots and col_ok(df, "Motor_Speed", "Commanded_Torque", "Torque_Feedback"):
        fig, axes = plt.subplots(3, 1, figsize=(14, 8), sharex=True)
        fig.suptitle(f"T1 — Time Series: {tag}", fontweight="bold")
        axes[0].plot(t, df["Motor_Speed"], color=COLORS["primary"])
        axes[0].set_ylabel("Motor Speed (rpm)")
        axes[1].plot(t, df["Commanded_Torque"], color=COLORS["accent"],
                     label="Commanded", alpha=0.8)
        axes[1].plot(t, df["Torque_Feedback"], color=COLORS["warning"],
                     label="Feedback", alpha=0.8)
        axes[1].set_ylabel("Torque (Nm)")
        axes[1].legend(fontsize=7)
        if col_ok(df, "power_electrical"):
            axes[2].plot(t, df["power_electrical"] / 1000, color=COLORS["success"])
            axes[2].set_ylabel("Power (kW)")
        axes[2].set_xlabel("Time (s)")
        save_fig(fig, f"T1_motor_{tag}")

        # Thermal plot
        temp_map = {"Module_A_Temp": "IGBT A", "Module_B_Temp": "IGBT B",
                    "Module_C_Temp": "IGBT C", "RTD1_Temp": "Winding 1",
                    "RTD2_Temp": "Winding 2"}
        avail = {k: v for k, v in temp_map.items() if col_ok(df, k)}
        if avail:
            fig, ax = plt.subplots(figsize=(14, 5))
            fig.suptitle(f"T1 — Thermal: {tag}", fontweight="bold")
            pal = [COLORS["primary"], COLORS["warning"], COLORS["accent"],
                   COLORS["success"], COLORS["purple"]]
            for i, (c, lbl) in enumerate(avail.items()):
                ax.plot(t, df[c], color=pal[i % 5], label=lbl, linewidth=1.5)
            ax.axhline(80, color="red", linestyle="--", alpha=0.5, label="80C warn")
            ax.set_xlabel("Time (s)")
            ax.set_ylabel("Temp (C)")
            ax.legend(fontsize=8, ncol=2)
            save_fig(fig, f"T1_thermal_{tag}")

    # ── T2: Statistics ────────────────────────────────────────────────────────
    stat_cols = ["Motor_Speed", "Commanded_Torque", "Torque_Feedback",
                 "DC_Bus_Voltage", "DC_Bus_Current", "v_mag_cog",
                 "ax_cog", "ay_cog", "Module_A_Temp", "RTD1_Temp",
                 "BMS_TotalV", "BMS_LowestCellV"]
    for col in stat_cols:
        if col_ok(df, col):
            s = df[col].dropna()
            mu = s.mean()
            std = s.std()
            kpi[f"T2_mean_{col}"] = round(mu, 3)
            kpi[f"T2_std_{col}"]  = round(std, 3)
            kpi[f"T2_cv_{col}"]   = round(abs(std / mu * 100) if mu != 0 else np.nan, 1)
            kpi[f"T2_p95_{col}"]  = round(s.quantile(0.95), 3)

    # ── T3: Cross-correlation torque lag ──────────────────────────────────────
    if col_ok(df, "Commanded_Torque", "Torque_Feedback"):
        x = df["Commanded_Torque"].dropna()
        y = df["Torque_Feedback"].dropna()
        n = min(len(x), len(y), 2000)   # cap to 200s for speed
        x, y = x.iloc[:n], y.iloc[:n]
        if x.std() > 0 and y.std() > 0:
            xn = (x - x.mean()) / x.std()
            yn = (y - y.mean()) / y.std()
            max_lag = min(50, n // 4)
            lags = np.arange(-max_lag, max_lag + 1)
            xcorr = np.array([
                float(np.dot(xn.values[:n-abs(l)], yn.values[abs(l):]) / (n-abs(l)))
                if l >= 0
                else float(np.dot(xn.values[abs(l):], yn.values[:n-abs(l)]) / (n-abs(l)))
                for l in lags
            ])
            peak_idx = np.argmax(np.abs(xcorr))
            kpi["T3_torque_lag_ms"] = round(float(lags[peak_idx]) * 1000 / TARGET_HZ, 0)
            kpi["T3_torque_peak_corr"] = round(float(xcorr[peak_idx]), 3)

    # Lateral accel -> yaw rate
    if col_ok(df, "Accel_Y", "Gyro_Z"):
        x = df["Accel_Y"].dropna()
        y = df["Gyro_Z"].dropna()
        n = min(len(x), len(y), 2000)
        x, y = x.iloc[:n], y.iloc[:n]
        if x.std() > 0 and y.std() > 0:
            xn = (x - x.mean()) / x.std()
            yn = (y - y.mean()) / y.std()
            max_lag = min(30, n // 4)
            lags = np.arange(-max_lag, max_lag + 1)
            xcorr = np.array([
                float(np.dot(xn.values[:n-abs(l)], yn.values[abs(l):]) / (n-abs(l)))
                if l >= 0
                else float(np.dot(xn.values[abs(l):], yn.values[:n-abs(l)]) / (n-abs(l)))
                for l in lags
            ])
            peak_idx = np.argmax(np.abs(xcorr))
            kpi["T3_ay_gyro_lag_ms"] = round(float(lags[peak_idx]) * 1000 / TARGET_HZ, 0)
            kpi["T3_ay_gyro_corr"]   = round(float(xcorr[peak_idx]), 3)

    # ── T4: Derived feature stats ─────────────────────────────────────────────
    for col in ["power_electrical", "power_mechanical", "efficiency",
                "drivetrain_slip", "segment_spread", "phase_imbalance", "RTD_imbalance"]:
        if col_ok(df, col):
            s = df[col].dropna()
            kpi[f"T4_{col}_mean"] = round(s.mean(), 3)
            kpi[f"T4_{col}_max"]  = round(s.max(), 3)
            kpi[f"T4_{col}_p95"]  = round(s.quantile(0.95), 3)

    # ── T5: Threshold detection ───────────────────────────────────────────────
    if col_ok(df, "ax_cog"):
        braking = (df["ax_cog"] < -2.0).astype(bool)
        kpi["T5_braking_pct"]    = round(float(braking.mean()) * 100, 1)
        kpi["T5_peak_decel"]     = round(df["ax_cog"].min(), 2)
        prev = braking.shift(1).fillna(False).astype(bool)
        kpi["T5_braking_events"] = int((braking & ~prev).sum())

    if col_ok(df, "Module_A_Temp"):
        kpi["T5_igbt_above_60C_pct"] = round(float((df["Module_A_Temp"] > 60).mean()) * 100, 1)
        kpi["T5_igbt_above_80C_pct"] = round(float((df["Module_A_Temp"] > 80).mean()) * 100, 1)

    if col_ok(df, "is_regen"):
        regen = df["is_regen"].fillna(0).astype(bool)
        kpi["T5_regen_pct"]    = round(float(regen.mean()) * 100, 1)
        prev_r = regen.shift(1).fillna(False).astype(bool)
        kpi["T5_regen_events"] = int((regen & ~prev_r).sum())

    if col_ok(df, "BMS_FaultFlags"):
        kpi["T5_bms_fault_pct"] = round(
            float((df["BMS_FaultFlags"].fillna(0) != 0).mean()) * 100, 1)

    # ── T6: Mandatory KPIs (award) with explicit fault plan ──────────────────
    # Each mandatory KPI carries a *_status field: "ok" or
    # "unavailable: <missing channel / reason>", per the award fault-plan rule.

    # 1) Total energy (Wh)
    if col_ok(df, "energy_cumulative"):
        total_energy_Wh = df["energy_cumulative"].iloc[-1] / 3600
        kpi["T6_total_energy_Wh"] = round(total_energy_Wh, 2)
        kpi["T6_total_energy_status"] = "ok"
    else:
        base_missing = [c for c in ("DC_Bus_Voltage", "DC_Bus_Current")
                        if not col_ok(df, c)]
        reason = (", ".join(base_missing) + " channel absent") if base_missing \
                 else "energy_cumulative not derived"
        kpi["T6_total_energy_status"] = f"unavailable: {reason}"

    # 2) Regen energy (additional KPI, same fault pattern)
    if col_ok(df, "energy_regen_cumulative"):
        regen_Wh = df["energy_regen_cumulative"].iloc[-1] / 3600
        kpi["T6_regen_energy_Wh"] = round(regen_Wh, 2)
        if kpi.get("T6_total_energy_Wh", 0) > 0:
            kpi["T6_regen_pct"] = round(
                regen_Wh / kpi["T6_total_energy_Wh"] * 100, 1)

    # 3) Distance (km) — GNSS/COG speed primary, motor-derived fallback
    if col_ok(df, "distance_cumulative"):
        dist_m = df["distance_cumulative"].iloc[-1]
        kpi["T6_distance_m"] = round(dist_m, 1)
        src = (str(df["distance_source"].iloc[0])
               if "distance_source" in df.columns else "unknown")
        kpi["T6_distance_source"] = src
        kpi["T6_distance_status"] = "ok"
    else:
        dist_m = 0.0
        kpi["T6_distance_status"] = ("unavailable: no speed source "
                                     "(neither GNSS/COG nor Motor_Speed present)")

    # 4) Energy per km (Wh/km) — depends on 1) and 3)
    if dist_m > 100 and kpi.get("T6_total_energy_Wh", 0) > 0:
        kpi["T6_Whpkm"] = round(kpi["T6_total_energy_Wh"] / (dist_m / 1000), 1)
        kpi["T6_Whpkm_status"] = "ok"
    else:
        reasons = []
        if kpi.get("T6_total_energy_status", "ok") != "ok" \
           or kpi.get("T6_total_energy_Wh", 0) <= 0:
            reasons.append("energy unavailable or non-positive")
        if dist_m <= 100:
            reasons.append("distance below 100 m minimum")
        kpi["T6_Whpkm_status"] = "unavailable: " + "; ".join(reasons)

    # 5) Run duration (s)
    if col_ok(df, "t_sec"):
        kpi["T6_duration_s"] = round(float(df["t_sec"].iloc[-1]), 1)
        kpi["T6_duration_status"] = "ok"
    else:
        kpi["T6_duration_status"] = "unavailable: time base t_sec missing"

    # Roll-up: are all four mandatory KPIs computable on this run?
    _mand = ["T6_total_energy_status", "T6_distance_status",
             "T6_Whpkm_status", "T6_duration_status"]
    kpi["T6_mandatory_ok"] = all(kpi.get(s) == "ok" for s in _mand)

    # ── T7: Distribution percentiles ──────────────────────────────────────────
    for col in ["ay_cog", "Commanded_Torque", "Motor_Speed", "Module_A_Temp",
                "DC_Bus_Current", "v_mag_cog"]:
        if col_ok(df, col):
            s = df[col].dropna()
            for p, pct in [(10, "p10"), (50, "p50"), (90, "p90")]:
                kpi[f"T7_{col}_{pct}"] = round(s.quantile(p / 100), 3)

    # ── T8: Friction circle ────────────────────────────────────────────────────
    if col_ok(df, "ax_cog", "ay_cog"):
        ax_v = df["ax_cog"].dropna().values
        ay_v = df["ay_cog"].dropna().values
        n = min(len(ax_v), len(ay_v))
        g = np.sqrt(ax_v[:n]**2 + ay_v[:n]**2)
        g_peak = np.percentile(g, 99)
        g_mean = g.mean()
        kpi["T8_g_peak_p99"] = round(g_peak, 3)
        kpi["T8_g_mean"]     = round(g_mean, 3)
        kpi["T8_grip_util_pct"] = round(g_mean / g_peak * 100 if g_peak > 0 else 0, 1)

        if make_plots:
            fig, ax = plt.subplots(figsize=(7, 7))
            fig.suptitle(f"T8 — Friction Circle: {tag}", fontweight="bold")
            sc = ax.scatter(ay_v[:n], ax_v[:n], c=g, cmap="plasma",
                            s=4, alpha=0.4, linewidths=0)
            plt.colorbar(sc, ax=ax, label="|g| (m/s^2)")
            theta = np.linspace(0, 2*np.pi, 300)
            ax.plot(g_peak * np.cos(theta), g_peak * np.sin(theta),
                    "r--", linewidth=1.5, label=f"P99: {g_peak:.2f} m/s^2")
            ax.axhline(0, color="grey", linewidth=0.5)
            ax.axvline(0, color="grey", linewidth=0.5)
            ax.set_aspect("equal")
            ax.set_xlabel("ay (m/s^2)")
            ax.set_ylabel("ax (m/s^2)")
            ax.legend(fontsize=8)
            save_fig(fig, f"T8_gg_{tag}")

    # ── T9: Peak acceleration KPI (reportable; m/s^2 and g) ─────────────────────
    # Combined value mirrors T8's 99th-pct method (spike-robust); directional
    # peaks use full-range extremes. Includes an explicit fault status.
    G = 9.81
    if col_ok(df, "ax_cog", "ay_cog"):
        ax_s = df["ax_cog"].dropna()
        ay_s = df["ay_cog"].dropna()
        n = min(len(ax_s), len(ay_s))
        combined = np.sqrt(ax_s.values[:n]**2 + ay_s.values[:n]**2)
        a_fwd   = float(ax_s.max())        # peak forward (drive)
        a_brake = float(-ax_s.min())       # peak deceleration (positive)
        a_lat   = float(ay_s.abs().max())  # peak lateral
        a_comb  = float(np.percentile(combined, 99))  # robust combined peak
        kpi["T9_accel_fwd_ms2"]    = round(a_fwd, 2)
        kpi["T9_accel_fwd_g"]      = round(a_fwd / G, 2)
        kpi["T9_accel_brake_ms2"]  = round(a_brake, 2)
        kpi["T9_accel_brake_g"]    = round(a_brake / G, 2)
        kpi["T9_accel_lat_ms2"]    = round(a_lat, 2)
        kpi["T9_accel_lat_g"]      = round(a_lat / G, 2)
        kpi["T9_accel_peak_ms2"]   = round(a_comb, 2)   # headline "max acceleration"
        kpi["T9_accel_peak_g"]     = round(a_comb / G, 2)
        kpi["T9_accel_status"]     = "ok"
    else:
        missing = [c for c in ("ax_cog", "ay_cog") if not col_ok(df, c)]
        kpi["T9_accel_status"] = f"unavailable: missing {', '.join(missing)}"

    # ── T10: Regression / trend ────────────────────────────────────────────────
    for xcol, ycol, key in [
        ("t_sec", "Module_A_Temp", "igbt_thermal"),
        ("t_sec", "RTD1_Temp",     "winding_thermal"),
        ("DC_Bus_Current", "DC_Bus_Voltage", "voltage_sag"),
        ("t_sec", "BMS_LowestCellV", "cell_depletion"),
    ]:
        if col_ok(df, xcol, ycol):
            xy = df[[xcol, ycol]].dropna()
            if len(xy) > 10 and xy[xcol].std() > 0:
                slope, intercept, r, _, _ = stats.linregress(
                    xy[xcol].values, xy[ycol].values)
                kpi[f"T10_{key}_slope"] = round(slope, 6)
                kpi[f"T10_{key}_r2"]    = round(r**2, 4)

    # ── T11: Rate of change ────────────────────────────────────────────────────
    dt = 1 / TARGET_HZ
    for col, key in [("Module_A_Temp", "igbt_rise"),
                     ("RTD1_Temp",     "winding_rise"),
                     ("ax_cog",        "jerk")]:
        if col_ok(df, col):
            rate = df[col].diff() / dt
            kpi[f"T11_{key}_p99_rate"] = round(float(rate.abs().quantile(0.99)), 4)

    # ── T12: Efficiency map summary ────────────────────────────────────────────
    if col_ok(df, "Motor_Speed", "Torque_Feedback", "efficiency"):
        running = df[(df["Motor_Speed"] > 200) &
                     (df["Torque_Feedback"].abs() > 2)].copy()
        if len(running) >= 50:
            eff = running["efficiency"].dropna()
            kpi["T12_mean_efficiency_pct"] = round(float(eff.mean()), 1)
            kpi["T12_peak_efficiency_pct"] = round(float(eff.quantile(0.95)), 1)
            kpi["T12_n_operating_points"]  = len(running)

            if make_plots:
                rpm  = running["Motor_Speed"].values
                torq = running["Torque_Feedback"].values
                eff_v= running["efficiency"].values
                fig, axes = plt.subplots(1, 2, figsize=(13, 6))
                fig.suptitle(f"T12 — Efficiency Map: {tag}", fontweight="bold")
                sc = axes[0].scatter(torq, rpm, c=eff_v, cmap="RdYlGn",
                                     s=6, alpha=0.5, vmin=50, vmax=100)
                plt.colorbar(sc, ax=axes[0], label="Efficiency (%)")
                axes[0].set_xlabel("Torque Feedback (Nm)")
                axes[0].set_ylabel("Motor Speed (rpm)")
                axes[1].hist2d(torq, rpm, bins=[25, 25], cmap="Blues", norm=LogNorm())
                axes[1].set_xlabel("Torque Feedback (Nm)")
                axes[1].set_ylabel("Motor Speed (rpm)")
                axes[1].set_title("Operating Density")
                save_fig(fig, f"T12_effmap_{tag}")

    return kpi


# ─────────────────────────────────────────────────────────────────────────────
# AGGREGATE PLOTS — run after all files processed
# ─────────────────────────────────────────────────────────────────────────────
def aggregate_plots(all_kpis):
    """Generate cross-session comparison plots from the master KPI table."""
    df_kpi = pd.DataFrame(all_kpis).sort_values("duration_s", ascending=False)

    # ── T9: Session comparison — key KPIs bar chart ───────────────────────────
    fig, axes = plt.subplots(2, 3, figsize=(16, 9))
    fig.suptitle("T9 — Cross-Session Comparison (All Files)", fontweight="bold")
    axes = axes.flatten()
    labels = [f.replace("_preprocessed.csv","") for f in df_kpi["file"]]

    kpi_plots = [
        ("T2_mean_Motor_Speed",    "Mean Motor Speed (rpm)"),
        ("T2_mean_DC_Bus_Voltage", "Mean DC Bus Voltage (V)"),
        ("T2_mean_Module_A_Temp",  "Mean IGBT A Temp (C)"),
        ("T8_g_peak_p99",          "Peak G envelope (m/s^2)"),
        ("T6_total_energy_Wh",     "Total Energy (Wh)"),
        ("T5_regen_pct",           "Regen Time (%)"),
    ]
    for i, (col, ylabel) in enumerate(kpi_plots):
        if col not in df_kpi.columns:
            axes[i].set_visible(False)
            continue
        vals = df_kpi[col].fillna(0)
        bars = axes[i].bar(range(len(vals)), vals,
                           color=COLORS["primary"], edgecolor="white", linewidth=0.3)
        axes[i].set_ylabel(ylabel, fontsize=8)
        axes[i].set_xticks(range(len(labels)))
        axes[i].set_xticklabels(labels, rotation=90, fontsize=6)
        axes[i].set_title(ylabel, fontsize=8)

    plt.tight_layout()
    save_fig(fig, "T9_session_comparison_all")

    # ── T7 aggregate: combined distribution overlays ──────────────────────────
    # Reload the 5 longest sessions for overlay
    long_files = df_kpi.head(5)["file"].tolist()
    loaded = []
    for fname in long_files:
        fp = os.path.join(PREPROCESSED_DIR, fname)
        if os.path.exists(fp):
            loaded.append((fname.replace("_preprocessed.csv",""), load(fp)))

    if loaded:
        fig, axes = plt.subplots(2, 3, figsize=(16, 9))
        fig.suptitle("T7 — Distribution Overlay: Top 5 Sessions", fontweight="bold")
        axes = axes.flatten()
        palette = [COLORS["primary"], COLORS["warning"], COLORS["accent"],
                   COLORS["success"], COLORS["purple"]]
        dist_cols = [
            ("Motor_Speed",    "Motor Speed (rpm)"),
            ("DC_Bus_Current", "DC Bus Current (A)"),
            ("Module_A_Temp",  "IGBT A Temp (C)"),
            ("v_mag_cog",      "Vehicle Speed (m/s)"),
            ("ay_cog",         "Lateral Accel (m/s^2)"),
            ("efficiency",     "Drivetrain Efficiency (%)"),
        ]
        for i, (col, label) in enumerate(dist_cols):
            for j, (lbl, df_) in enumerate(loaded):
                if col_ok(df_, col):
                    s = df_[col].dropna()
                    axes[i].hist(s, bins=50, color=palette[j], alpha=0.45,
                                 density=True, label=lbl, linewidth=0)
            axes[i].set_xlabel(label, fontsize=8)
            axes[i].set_ylabel("Density", fontsize=8)
            axes[i].legend(fontsize=6, ncol=2)
        plt.tight_layout()
        save_fig(fig, "T7_distributions_overlay")

    # ── T10 scatter: thermal trends across all sessions ───────────────────────
    if "T10_igbt_thermal_slope" in df_kpi.columns:
        fig, axes = plt.subplots(1, 2, figsize=(12, 5))
        fig.suptitle("T10 — Thermal Trends: All Sessions", fontweight="bold")

        ax = axes[0]
        vals = df_kpi["T10_igbt_thermal_slope"].dropna()
        names = [labels[i] for i in vals.index]
        colors = [COLORS["warning"] if v > 0 else COLORS["success"] for v in vals]
        ax.barh(range(len(vals)), vals, color=colors)
        ax.set_yticks(range(len(vals)))
        ax.set_yticklabels(names, fontsize=7)
        ax.axvline(0, color="black", linewidth=0.8)
        ax.set_xlabel("IGBT Temp Slope (C/s)")
        ax.set_title("IGBT Thermal Trend")

        ax = axes[1]
        if "T10_voltage_sag_slope" in df_kpi.columns:
            vals2 = df_kpi["T10_voltage_sag_slope"].dropna()
            names2 = [labels[i] for i in vals2.index]
            ax.bar(range(len(vals2)), vals2, color=COLORS["primary"])
            ax.set_xticks(range(len(vals2)))
            ax.set_xticklabels(names2, rotation=90, fontsize=6)
            ax.set_ylabel("Slope (V/A)")
            ax.set_title("Voltage Sag per Ampere")

        plt.tight_layout()
        save_fig(fig, "T10_thermal_trends_all")

    # ── Master heatmap: KPI matrix ────────────────────────────────────────────
    heatmap_cols = [
        "T2_mean_Motor_Speed", "T2_mean_DC_Bus_Voltage", "T2_mean_Module_A_Temp",
        "T8_g_peak_p99", "T8_grip_util_pct", "T5_braking_pct",
        "T5_regen_pct", "T6_total_energy_Wh", "T12_mean_efficiency_pct",
        "T10_igbt_thermal_slope", "T3_torque_lag_ms", "T3_ay_gyro_corr",
    ]
    avail_cols = [c for c in heatmap_cols if c in df_kpi.columns]
    if avail_cols:
        hm_df = df_kpi[["file"] + avail_cols].set_index("file")
        hm_df.index = [i.replace("_preprocessed.csv","") for i in hm_df.index]
        hm_norm = (hm_df - hm_df.min()) / (hm_df.max() - hm_df.min() + 1e-9)

        fig, ax = plt.subplots(figsize=(max(12, len(avail_cols) * 1.2),
                                        max(6, len(hm_df) * 0.4)))
        fig.suptitle("KPI Heatmap — All Sessions", fontweight="bold")
        im = ax.imshow(hm_norm.values, cmap="RdYlGn", aspect="auto",
                       vmin=0, vmax=1)
        plt.colorbar(im, ax=ax, label="Normalised 0-1")
        ax.set_xticks(range(len(avail_cols)))
        ax.set_xticklabels([c.replace("T2_mean_", "").replace("T8_", "")
                             .replace("T5_", "").replace("T6_", "")
                             .replace("T12_", "").replace("T10_", "")
                             .replace("T3_", "")
                             for c in avail_cols], rotation=45, ha="right", fontsize=8)
        ax.set_yticks(range(len(hm_df)))
        ax.set_yticklabels(hm_df.index, fontsize=7)
        plt.tight_layout()
        save_fig(fig, "KPI_heatmap_all_sessions")

    print(f"  [AGG] Aggregate plots saved: T9 comparison, T7 overlay, T10 thermal, KPI heatmap")
    return df_kpi


# ─────────────────────────────────────────────────────────────────────────────
# MASTER BATCH RUNNER
# ─────────────────────────────────────────────────────────────────────────────
def run_all():
    files = sorted([f for f in os.listdir(PREPROCESSED_DIR)
                    if f.endswith("_preprocessed.csv")])

    print(f"\n{'='*65}")
    print(f"  Formula Manipal EV — Batch Analytics Engine (T1-T12)")
    print(f"  Files found   : {len(files)}")
    print(f"  Output        : {OUTPUT_DIR}")
    print(f"{'='*65}\n")

    all_kpis = []
    errors = []

    for idx, fname in enumerate(files, 1):
        fpath = os.path.join(PREPROCESSED_DIR, fname)
        try:
            df = load(fpath)
            dur = float(df["t_sec"].iloc[-1])

            if dur < MIN_DURATION_S:
                print(f"  [{idx:02d}/{len(files)}] SKIP  {fname} ({dur:.0f}s < {MIN_DURATION_S}s)")
                continue

            make_plots = dur >= DETAIL_PLOT_THRESHOLD_S
            kpi = analyze_file(df, fname, make_plots=make_plots)
            all_kpis.append(kpi)

            tag = "*" if make_plots else " "
            print(f"  [{idx:02d}/{len(files)}]{tag} OK    {fname} "
                  f"| {dur:.0f}s | {len(df)} rows | "
                  f"RPM={kpi.get('T2_mean_Motor_Speed', 'N/A')} "
                  f"V={kpi.get('T2_mean_DC_Bus_Voltage', 'N/A')}")

        except Exception as e:
            print(f"  [{idx:02d}/{len(files)}] ERROR {fname}: {e}")
            errors.append({"file": fname, "error": str(e)})

    print(f"\n  Processed: {len(all_kpis)} files | Skipped/Errors: "
          f"{len(files) - len(all_kpis)}\n")

    # ── Aggregate plots ────────────────────────────────────────────────────────
    print("  Generating aggregate plots...")
    df_kpi = aggregate_plots(all_kpis)

    # ── Save master KPI table ─────────────────────────────────────────────────
    kpi_path = os.path.join(OUTPUT_DIR, "kpi_all_sessions.csv")
    df_kpi.to_csv(kpi_path, index=False)
    print(f"  Master KPI table: {kpi_path}  ({len(df_kpi)} rows x {len(df_kpi.columns)} cols)")

    # ── Save narrative report ─────────────────────────────────────────────────
    txt_path = os.path.join(OUTPUT_DIR, "analysis_report.txt")
    with open(txt_path, "w", encoding="utf-8") as f:
        f.write("FORMULA MANIPAL EV — BATCH ANALYTICS REPORT\n")
        f.write("=" * 60 + "\n")
        f.write(f"Files processed : {len(df_kpi)}\n")
        f.write(f"Total data      : {df_kpi['duration_s'].sum():.0f} s "
                f"({df_kpi['duration_s'].sum()/3600:.2f} h)\n\n")

        f.write("SESSION SUMMARY (sorted by duration)\n" + "-"*40 + "\n")
        fmt_cols = ["file", "duration_s", "T2_mean_Motor_Speed",
                    "T2_mean_DC_Bus_Voltage", "T2_mean_Module_A_Temp",
                    "T8_g_peak_p99", "T6_total_energy_Wh",
                    "T12_mean_efficiency_pct"]
        avail = [c for c in fmt_cols if c in df_kpi.columns]
        f.write(df_kpi[avail].to_string(index=False))
        f.write("\n\n")

        f.write("FLEET-WIDE STATISTICS\n" + "-"*40 + "\n")
        numeric = df_kpi.select_dtypes(include=np.number)
        f.write(numeric.describe().round(3).to_string())
        f.write("\n")

    plots = os.listdir(PLOTS_DIR)
    print(f"\n{'='*65}")
    print(f"  Done! {len(plots)} plots in {PLOTS_DIR}")
    print(f"  KPI table : {kpi_path}")
    print(f"  Report    : {txt_path}")
    print(f"{'='*65}\n")

    return df_kpi


if __name__ == "__main__":
    run_all()
