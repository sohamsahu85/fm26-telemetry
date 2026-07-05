"""
Formula Manipal EV — Run Catalogue & Search
--------------------------------------------
Builds a searchable index over all preprocessed runs.

Two sources of metadata:
  1. AUTO-DERIVED from the data itself (duration, distance, max speed, peak
     accel, channel availability, validity) — always consistent, never manual.
  2. HUMAN-EDITABLE fields (date, event, driver, session_type, car config,
     origin) held in run_metadata.csv and merged by source_file.

Outputs:
  - run_metadata_template.csv : pre-filled with run ids + auto fields; the team
    fills the blank human columns once.
  - run_catalogue.csv         : the full searchable catalogue (auto + human).

Search:
  search(catalogue, event=..., driver=..., session_type=..., aeropack=...,
         valid_only=True, min_duration_s=...) -> filtered DataFrame
"""

import glob, os
import numpy as np
import pandas as pd

PREP_DIR   = "preprocessed"
META_FILE  = "run_metadata.csv"
G          = 9.81
MIN_VALID_DURATION_S = 30

# Human-editable columns (only the team can fill these)
HUMAN_COLS = ["date", "event", "driver", "session_type",
              "aeropack", "springs", "dampers", "arb", "origin", "notes"]


def _has(df, *cols):
    return all(c in df.columns and df[c].notna().any() for c in cols)


def derive(path):
    """Extract everything computable directly from one preprocessed run."""
    df = pd.read_csv(path)
    src = os.path.basename(path).replace("_preprocessed.csv", "")
    rec = {"source_file": src, "file_path": path}

    dur = float(df["t_sec"].iloc[-1]) if _has(df, "t_sec") else np.nan
    rec["duration_s"] = round(dur, 1) if np.isfinite(dur) else np.nan

    rec["distance_km"] = (round(df["distance_cumulative"].iloc[-1] / 1000, 3)
                          if _has(df, "distance_cumulative") else np.nan)
    rec["distance_source"] = (str(df["distance_source"].iloc[0])
                              if "distance_source" in df.columns else "unknown")
    rec["max_speed_kmh"] = (round(df["v_mag_cog"].max() * 3.6, 1)
                            if _has(df, "v_mag_cog") else np.nan)

    if _has(df, "ax_cog", "ay_cog"):
        ax, ay = df["ax_cog"].dropna(), df["ay_cog"].dropna()
        n = min(len(ax), len(ay))
        comb = np.sqrt(ax.values[:n] ** 2 + ay.values[:n] ** 2)
        rec["peak_accel_g"] = round(float(np.percentile(comb, 99)) / G, 2)
    else:
        rec["peak_accel_g"] = np.nan

    # channel availability -> validity
    rec["has_energy"]   = _has(df, "energy_cumulative") or _has(df, "DC_Bus_Current", "DC_Bus_Voltage")
    rec["has_imu"]      = _has(df, "Accel_X", "Accel_Y", "Accel_Z")
    rec["has_steering"] = _has(df, "steering_angle")   # currently False until sensor added
    rec["valid"] = bool(rec["has_energy"] and rec["has_imu"]
                        and rec["has_steering"]
                        and np.isfinite(dur) and dur >= MIN_VALID_DURATION_S)

    # advisory session guess (human session_type overrides this)
    rec["session_guess"] = _guess_session(rec)
    return rec


def _guess_session(rec):
    d, g = rec.get("duration_s"), rec.get("peak_accel_g")
    if not np.isfinite(d) or d < MIN_VALID_DURATION_S:
        return "too_short"
    if d < 120 and (g or 0) >= 0.8:
        return "acceleration/autocross"
    if d >= 600:
        return "endurance"
    return "practice"


def build(prep_dir=PREP_DIR, meta_file=META_FILE):
    files = sorted(glob.glob(os.path.join(prep_dir, "*_preprocessed.csv")))
    auto = pd.DataFrame([derive(f) for f in files])

    # load human metadata if present, else create a blank template
    if os.path.exists(meta_file):
        human = pd.read_csv(meta_file)
    else:
        human = pd.DataFrame({"source_file": auto["source_file"]})
        for c in HUMAN_COLS:
            human[c] = ""
        human.loc[human["origin"] == "", "origin"] = "real"

    cat = auto.merge(human, on="source_file", how="left")

    # canonical run_id: use human fields once filled, else a stable sequential id
    def _rid(r, i):
        if str(r.get("date", "")).strip() and str(r.get("event", "")).strip():
            drv = str(r.get("driver", "")).strip() or "NA"
            return f"{r['date']}_{r['event']}_{drv}_{i:02d}".replace(" ", "")
        return f"RUN_{i:03d}"
    cat.insert(0, "run_id", [_rid(r, i + 1) for i, r in cat.iterrows()])

    # write the fill-in template (auto fields shown read-only for reference)
    template = cat[["run_id", "source_file", "duration_s", "distance_km",
                    "max_speed_kmh", "peak_accel_g", "session_guess"] + HUMAN_COLS]
    template.to_csv("run_metadata_template.csv", index=False)
    cat.to_csv("run_catalogue.csv", index=False)
    return cat


def search(cat, valid_only=False, min_duration_s=None, **filters):
    """Filter the catalogue by any combination of fields.
    Example: search(cat, event='Endurance', driver='D2', aeropack='Y')
    """
    m = pd.Series(True, index=cat.index)
    if valid_only:
        m &= cat["valid"] == True
    if min_duration_s is not None:
        m &= cat["duration_s"] >= min_duration_s
    for k, v in filters.items():
        if k not in cat.columns:
            raise KeyError(f"unknown field '{k}'")
        m &= cat[k].astype(str).str.lower() == str(v).lower()
    cols = ["run_id", "date", "event", "driver", "session_type",
            "aeropack", "duration_s", "valid", "file_path"]
    return cat.loc[m, [c for c in cols if c in cat.columns]]


if __name__ == "__main__":
    cat = build()
    print(f"Catalogue built: {len(cat)} runs -> run_catalogue.csv")
    print(f"Fill-in template written -> run_metadata_template.csv\n")
    print("Auto-derived summary (first rows):")
    print(cat[["run_id", "duration_s", "distance_km", "max_speed_kmh",
               "peak_accel_g", "has_imu", "has_steering", "valid",
               "session_guess"]].head(8).to_string(index=False))
