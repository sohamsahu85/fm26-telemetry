"""
Formula Manipal — KPI Engine
============================
The live-challenge tool. A KPI is ONE function. Everything else is automatic.

Define a KPI (this is all you write during the challenge):

    @kpi("time_above_08g_lat", unit="s", requires=["ay_cog", "t_sec"])
    def time_above_08g_lat(run):
        return (run.df["ay_cog"].abs() > 0.8 * G).sum() * run.dt

Run it:

    python3 kpi_engine.py                      # all KPIs, all valid runs
    python3 kpi_engine.py time_above_08g_lat   # one KPI, all valid runs

Contract (matches the technical report):
    every KPI produces {value, status, missing_channels}
    - requires=[...] is checked before the function runs -> fault plan for free
    - exceptions are caught -> status = "error: ...", never a crash mid-demo
Results print as a table and save to kpi_results.csv.
"""

import glob, os, sys, math, traceback
import numpy as np
import pandas as pd
import warnings; warnings.simplefilter("ignore")

G = 9.81
PREP_DIR = "preprocessed"
MIN_DURATION_S = 30

# ────────────────────────────────────────────────────────────────────────────
# Run object: what every KPI receives
# ────────────────────────────────────────────────────────────────────────────
class Run:
    """One run: the dataframe plus the helpers judges' KPIs always need."""
    def __init__(self, df: pd.DataFrame, name: str):
        self.df = df
        self.name = name
        t = df["t_sec"].dropna().values if "t_sec" in df else np.array([0.0])
        self.duration = float(t[-1]) if len(t) else 0.0
        self.dt = float(np.median(np.diff(t))) if len(t) > 2 else 0.1

    # -- channel access -----------------------------------------------------
    def has(self, *cols):
        return all(c in self.df.columns and self.df[c].notna().any() for c in cols)

    def ch(self, col):
        """Channel as a Series (NaNs kept — mask or dropna as needed)."""
        return self.df[col]

    # -- masks judges' KPIs are usually built from ---------------------------
    def moving(self, min_speed_ms=1.0):
        if self.has("v_mag_cog"):
            return self.df["v_mag_cog"].fillna(0) > min_speed_ms
        if self.has("ground_speed_ms"):
            return self.df["ground_speed_ms"].fillna(0) > min_speed_ms
        return pd.Series(True, index=self.df.index)

    def braking(self, decel_g=0.2):
        return self.df["ax_cog"].fillna(0) < -decel_g * G

    def cornering(self, lat_g=0.3):
        return self.df["ay_cog"].abs().fillna(0) > lat_g * G

    def events(self, mask):
        """Count rising edges of a boolean mask (e.g. braking events)."""
        m = mask.fillna(False).astype(bool)
        return int((m & ~m.shift(1).fillna(False)).sum())

    def speed_kmh(self):
        s = self.df.get("v_mag_cog", self.df.get("ground_speed_ms"))
        return s * 3.6 if s is not None else None


# ────────────────────────────────────────────────────────────────────────────
# Registry + decorator
# ────────────────────────────────────────────────────────────────────────────
REGISTRY = {}

def kpi(name, unit="", requires=(), doc=""):
    """Register a KPI. `requires` drives the automatic fault plan."""
    def wrap(fn):
        REGISTRY[name] = {"fn": fn, "unit": unit, "requires": list(requires),
                          "doc": doc or (fn.__doc__ or "").strip()}
        return fn
    return wrap


def compute(run: Run, name: str):
    """Run one KPI on one run -> dict(value, status, missing_channels)."""
    spec = REGISTRY[name]
    missing = [c for c in spec["requires"] if not run.has(c)]
    if missing:
        return {"value": None, "status": f"unavailable: missing {', '.join(missing)}",
                "missing_channels": missing}
    try:
        v = spec["fn"](run)
        if v is None or (isinstance(v, float) and not math.isfinite(v)):
            return {"value": None, "status": "unavailable: not computable on this run",
                    "missing_channels": []}
        return {"value": round(float(v), 3), "status": "ok", "missing_channels": []}
    except Exception as e:
        return {"value": None, "status": f"error: {e}", "missing_channels": []}


# ────────────────────────────────────────────────────────────────────────────
# Fleet runner
# ────────────────────────────────────────────────────────────────────────────
def load_runs(valid_only=True):
    runs = []
    for f in sorted(glob.glob(os.path.join(PREP_DIR, "*_preprocessed.csv"))):
        df = pd.read_csv(f)
        r = Run(df, os.path.basename(f).replace("_preprocessed.csv", ""))
        if valid_only and r.duration < MIN_DURATION_S:
            continue
        runs.append(r)
    return runs

def run_all(names=None, valid_only=True):
    names = names or list(REGISTRY)
    runs = load_runs(valid_only)
    rows = []
    for r in runs:
        row = {"run": r.name, "duration_s": round(r.duration, 1)}
        for n in names:
            res = compute(r, n)
            row[n] = res["value"] if res["status"] == "ok" else res["status"]
        rows.append(row)
    out = pd.DataFrame(rows)
    out.to_csv("kpi_results.csv", index=False)
    return out


# ────────────────────────────────────────────────────────────────────────────
# KPIs — the mandatory set plus examples written exactly as you would live
# ────────────────────────────────────────────────────────────────────────────

@kpi("total_energy_Wh", unit="Wh", requires=["energy_cumulative"],
     doc="Total electrical energy consumed")
def total_energy_Wh(run):
    return run.ch("energy_cumulative").iloc[-1] / 3600

@kpi("distance_km", unit="km", requires=["distance_cumulative"],
     doc="Distance covered (GNSS speed, motor fallback)")
def distance_km(run):
    return run.ch("distance_cumulative").iloc[-1] / 1000

@kpi("energy_per_km", unit="Wh/km", requires=["energy_cumulative", "distance_cumulative"],
     doc="Consumption; needs energy > 0 and distance > 100 m")
def energy_per_km(run):
    e = run.ch("energy_cumulative").iloc[-1] / 3600
    d = run.ch("distance_cumulative").iloc[-1] / 1000
    if d <= 0.1 or e <= 0:
        return None
    return e / d

@kpi("duration_s", unit="s", requires=["t_sec"], doc="Run duration")
def duration_s(run):
    return run.duration

# ---- examples in the judges' style -----------------------------------------

@kpi("peak_combined_g", unit="g", requires=["ax_cog", "ay_cog"],
     doc="99th-percentile combined acceleration")
def peak_combined_g(run):
    ax, ay = run.ch("ax_cog").dropna(), run.ch("ay_cog").dropna()
    n = min(len(ax), len(ay))
    return np.percentile(np.sqrt(ax.values[:n]**2 + ay.values[:n]**2), 99) / G

@kpi("braking_events", unit="count", requires=["ax_cog"],
     doc="Number of braking events over 0.3 g")
def braking_events(run):
    return run.events(run.braking(0.3))

@kpi("time_cornering_pct", unit="%", requires=["ay_cog", "t_sec"],
     doc="Share of moving time above 0.3 g lateral")
def time_cornering_pct(run):
    m = run.moving()
    if m.sum() == 0:
        return None
    return 100.0 * (run.cornering(0.3) & m).sum() / m.sum()

@kpi("max_igbt_temp_C", unit="degC", requires=["Module_A_Temp"],
     doc="Maximum inverter module temperature")
def max_igbt_temp_C(run):
    t = pd.concat([run.ch(c) for c in
                   ("Module_A_Temp", "Module_B_Temp", "Module_C_Temp") if run.has(c)])
    return t.max()


# ────────────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    names = sys.argv[1:] or None
    if names:
        unknown = [n for n in names if n not in REGISTRY]
        if unknown:
            print("unknown KPI(s):", ", ".join(unknown))
            print("registered:", ", ".join(REGISTRY)); sys.exit(1)
    table = run_all(names)
    with pd.option_context("display.width", 200, "display.max_columns", 40):
        print(table.to_string(index=False))
    print(f"\n{len(table)} valid runs · saved to kpi_results.csv")
    print("registered KPIs:", ", ".join(f"{n} [{REGISTRY[n]['unit']}]" for n in REGISTRY))
