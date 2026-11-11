"""Train a reproducible *synthetic policy*, then export it for C++ inference.

The score below encodes a hypothetical deployment preference. It is NOT a
benchmark of either cipher. Replace labels with measured deployment outcomes
before claiming that the classifier optimizes performance.
"""
import argparse
import csv
import json
from pathlib import Path

import numpy as np
import sklearn
from sklearn.metrics import classification_report, confusion_matrix
from sklearn.model_selection import train_test_split
from sklearn.tree import DecisionTreeClassifier

ROOT = Path(__file__).resolve().parents[1]
FEATURES = ["latency_ms", "jitter_ms", "loss_pct"]


def export_tree(model, destination):
    tree = model.tree_
    rows = [f"ACS_TREE_V1 {tree.node_count}"]
    for i in range(tree.node_count):
        leaf = tree.children_left[i] == tree.children_right[i]
        label = int(model.classes_[np.argmax(tree.value[i])])
        rows.append(f"{-1 if leaf else tree.feature[i]} {tree.threshold[i]:.17g} "
                    f"{tree.children_left[i]} {tree.children_right[i]} {label}")
    destination.write_text("\n".join(rows) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--samples", type=int, default=12000)
    parser.add_argument("--out", type=Path, default=ROOT / "models")
    parser.add_argument("--data", type=Path, help="Optional measured CSV: latency_ms,jitter_ms,loss_pct,label (1 or 2)")
    args = parser.parse_args()
    rng = np.random.default_rng(args.seed)
    if args.data:
        with args.data.open(newline="", encoding="utf-8") as stream:
            rows = list(csv.DictReader(stream))
        x = np.array([[float(row[f]) for f in FEATURES] for row in rows])
        raw_y = np.array([float(row["label"]) for row in rows])
        if not np.isin(raw_y, [1, 2]).all():
            parser.error("Labels must be 1 (AES) or 2 (ChaCha20)")
        y = raw_y.astype(int)
        source = "user-supplied labels; quality depends on the measurement procedure"
    else:
        if args.samples < 200:
            parser.error("Use at least 200 samples")
        # Mixture of clean, variable, and congested simulated network conditions.
        x = np.column_stack([rng.uniform(0, 250, args.samples),
                             rng.uniform(0, 90, args.samples),
                             rng.uniform(0, 35, args.samples)])
        stress = x[:, 0] / 140 + x[:, 1] / 45 + x[:, 2] / 12
        stress += 0.3 * np.sin(x[:, 0] / 35) * (x[:, 1] / 90)
        y = np.where(stress > 2.2, 2, 1)
        source = "synthetic preference labels from documented stress score; not measured cipher speed"
    if x.ndim != 2 or x.shape[1] != 3 or len(x) < 100 or not np.isfinite(x).all():
        parser.error("Need at least 100 rows with three finite features")
    if (x < 0).any() or (x[:, 2] > 100).any() or set(y) != {1, 2}:
        parser.error("Need nonnegative metrics, loss <= 100, and both classes")
    train_x, test_x, train_y, test_y = train_test_split(x, y, test_size=0.25,
                                                     stratify=y, random_state=args.seed)
    model = DecisionTreeClassifier(max_depth=6, min_samples_leaf=20, random_state=args.seed)
    model.fit(train_x, train_y)
    predicted = model.predict(test_x)
    args.out.mkdir(parents=True, exist_ok=True)
    export_tree(model, args.out / "policy.tree")
    with (args.out / "parity.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(FEATURES + ["prediction"])
        # Include both ordinary samples and float32 values adjacent to every split.
        probes = list(test_x[:500])
        for feature, threshold in zip(model.tree_.feature, model.tree_.threshold):
            if feature >= 0:
                for value in [np.nextafter(np.float32(threshold), np.float32(-np.inf)),
                              np.float32(threshold),
                              np.nextafter(np.float32(threshold), np.float32(np.inf))]:
                    row = np.array([100., 30., 10.])
                    row[feature] = value
                    probes.append(row)
        for row, label in zip(probes, model.predict(np.array(probes))):
            writer.writerow([*map(float, row), int(label)])
    report = {
        "source": source, "seed": args.seed, "samples": len(x),
        "train_samples": len(train_x), "test_samples": len(test_x),
        "features": FEATURES, "classes": {"1": "AES-256-GCM", "2": "ChaCha20-Poly1305"},
        "sklearn_version": sklearn.__version__, "numpy_version": np.__version__,
        "node_count": model.tree_.node_count,
        "classification_report": classification_report(test_y, predicted, output_dict=True, zero_division=0),
        "confusion_matrix": confusion_matrix(test_y, predicted, labels=[1, 2]).tolist(),
        "warning": "Held-out synthetic policy agreement is not evidence of throughput, latency, or security improvements."
    }
    (args.out / "training_report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Exported {model.tree_.node_count}-node model to {args.out}")
    print(classification_report(test_y, predicted, zero_division=0))


if __name__ == "__main__":
    main()
