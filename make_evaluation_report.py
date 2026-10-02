"""Build a self-contained visual report from the paired evaluation CSV."""

import base64
import csv
import html
import io
import statistics
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import PercentFormatter


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "evaluation_results.csv"
OUTPUT = ROOT / "evaluation_report.html"
VARIANTS = ("v1", "v2", "v3-next", "v3-home", "v4")
COLORS = dict(zip(VARIANTS, ("#277da1", "#f8961e", "#43aa8b", "#b56576", "#603f8b")))


def image(fig):
    buffer = io.BytesIO()
    fig.savefig(buffer, format="png", dpi=145, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    return base64.b64encode(buffer.getvalue()).decode("ascii")


def line_chart(rows, key, title, xlabel, *, size=None, max_ants=None):
    data = defaultdict(list)
    for row in rows:
        if size is not None and row["size"] != size:
            continue
        if max_ants is not None and row["ants"] > max_ants:
            continue
        if row["upper_bound"]:
            data[row["variant"], row[key]].append(row["score"] / row["upper_bound"])
    fig, ax = plt.subplots(figsize=(8, 4.5))
    for variant in VARIANTS:
        xs = sorted(x for name, x in data if name == variant)
        ax.plot(xs, [statistics.fmean(data[variant, x]) for x in xs], label=variant,
                color=COLORS[variant], marker="o", markersize=4, linewidth=2)
    ax.set(title=title, xlabel=xlabel, ylabel="Mean score / optimistic bound")
    ax.yaxis.set_major_formatter(PercentFormatter(1))
    ax.grid(alpha=0.2)
    ax.spines[["top", "right"]].set_visible(False)
    ax.legend(ncol=5, loc="lower center", bbox_to_anchor=(0.5, -0.34), frameon=False)
    fig.subplots_adjust(bottom=0.25)
    return image(fig)


def heatmap(paired, sizes, densities):
    data = defaultdict(list)
    for group in paired.values():
        reference = group["v4"]
        data[reference["size"], reference["density_percent"]].append(
            reference["score"] - group["v2"]["score"])
    values = [[statistics.fmean(data[size, density]) for density in densities] for size in sizes]
    limit = max(abs(value) for row in values for value in row)
    fig, ax = plt.subplots(figsize=(9, 5.5))
    display = ax.imshow(values, cmap="RdBu", vmin=-limit, vmax=limit, aspect="auto")
    ax.set(title="V4 minus V2: mean food delivered", xlabel="Food density (%)",
           ylabel="Board size")
    ax.set_xticks(range(len(densities)), densities)
    ax.set_yticks(range(len(sizes)), [f"{size}×{size}" for size in sizes])
    fig.colorbar(display, ax=ax, label="Food items per world")
    for y, row in enumerate(values):
        for x, value in enumerate(row):
            ax.text(x, y, f"{value:+.1f}", ha="center", va="center", fontsize=8,
                    color="white" if abs(value) > 0.6 * limit else "#14202d")
    return image(fig)


def main():
    with SOURCE.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        raise ValueError(f"No rows in {SOURCE}")
    numeric = ("seed", "size", "ants", "density_percent", "score", "upper_bound",
               "initial_food", "remaining_energy")
    for row in rows:
        for field in numeric:
            row[field] = int(row[field])
    paired = defaultdict(dict)
    for row in rows:
        key = tuple(row[field] for field in ("seed", "size", "ants", "density_percent"))
        if row["variant"] in paired[key]:
            raise ValueError(f"Duplicate variant for world {key}")
        paired[key][row["variant"]] = row
    paired = {key: group for key, group in paired.items() if set(group) == set(VARIANTS)}
    if not paired:
        raise ValueError("No complete paired worlds")
    for key, group in paired.items():
        if len({row["upper_bound"] for row in group.values()}) != 1:
            raise ValueError(f"Upper bounds differ for world {key}")
    rows = [group[variant] for group in paired.values() for variant in VARIANTS]
    seeds = sorted({row["seed"] for row in rows})
    sizes = sorted({row["size"] for row in rows})
    densities = sorted({row["density_percent"] for row in rows})
    ants = sorted({row["ants"] for row in rows})
    table_rows = []
    for variant in VARIANTS:
        group = [row for row in rows if row["variant"] == variant]
        ratio = [row["score"] / row["upper_bound"] for row in group if row["upper_bound"]]
        wins = sum(grouped[variant]["score"] == max(item["score"] for item in grouped.values())
                   for grouped in paired.values())
        cells = (variant, f"{statistics.fmean(ratio):.1%}",
                 f"{statistics.fmean(row['score'] for row in group):.2f}",
                 f"{statistics.fmean(row['upper_bound'] - row['score'] for row in group):.2f}",
                 f"{statistics.fmean(row['remaining_energy'] for row in group):.1f}",
                 f"{wins:,}")
        table_rows.append("<tr>" + "".join(f"<td>{html.escape(cell)}</td>" for cell in cells) + "</tr>")
    charts = (
        ("Across board sizes", line_chart(rows, "size", "Performance by square board size", "Side length")),
        ("Across densities", line_chart(rows, "density_percent", "Performance by food density", "Food density (%)")),
        ("Across ant counts, balanced", line_chart(rows, "ants", "Performance by ant count (all sizes)",
                                                 "Ants", max_ants=10)),
        ("All ant counts at 50×50", line_chart(rows, "ants", "Performance by ant count (50×50 only)",
                                                "Ants", size=max(sizes))),
        ("Where V4 beats V2", heatmap(paired, sizes, densities)),
    )
    cards = "\n".join(f'<section><h2>{html.escape(title)}</h2><img alt="{html.escape(title)}" '
                      f'src="data:image/png;base64,{plot}"></section>' for title, plot in charts)
    report = f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Ant strategy evaluation</title><style>
body{{font:16px/1.5 system-ui,sans-serif;color:#17212d;background:#f5f7fa;margin:0}}
main{{max-width:1440px;margin:auto;padding:32px}}
h1{{margin-bottom:0}}p{{max-width:95ch;color:#475569}}
.grid{{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,620px),1fr));gap:20px}}
section{{background:white;border:1px solid #e1e6eb;border-radius:10px;padding:18px;overflow:auto}}
section img{{display:block;width:100%;height:auto}}h2{{font-size:1.1rem;margin:0 0 10px}}
table{{border-collapse:collapse;width:100%;font-variant-numeric:tabular-nums}}
th,td{{padding:9px 12px;text-align:right;border-bottom:1px solid #e1e6eb}}
th:first-child,td:first-child{{text-align:left}}th{{background:#eef2f6}}
@media(max-width:700px){{main{{padding:12px}}section{{padding:10px}}}}
</style></head><body><main>
<h1>Ant strategy evaluation</h1>
<p>{len(paired):,} identical worlds per variant; {len(rows):,} runs total. Seeds {min(seeds)}–{max(seeds)}, square boards {min(sizes)}–{max(sizes)}, food densities {min(densities)}–{max(densities)}%, and {min(ants)}–{max(ants)} ants. Source: <code>evaluation_results.csv</code>.</p>
<section><h2>Overall comparison</h2><table><thead><tr><th>Variant</th><th>Score / bound</th><th>Mean score</th><th>Mean gap</th><th>Energy left</th><th>Wins including ties</th></tr></thead><tbody>{''.join(table_rows)}</tbody></table></section>
<p>Score / bound is averaged per world; higher is better. The bound assumes known initial food locations and pooled energy, so it is optimistic, not an attainable guarantee. Mean gap is bound minus delivered food. Wins count ties. The 1–10-ant chart includes every board size at each count; the 50×50 chart shows 1–18 ants without changing board size. Board-size averages follow the configured ant-count range, which grows with board size. In the heatmap, blue means V4 delivers more than V2; red means less.</p>
<div class="grid">{cards}</div>
</main></body></html>"""
    OUTPUT.write_text(report, encoding="utf-8")
    print(f"Wrote {OUTPUT} from {len(paired):,} paired worlds")


if __name__ == "__main__":
    main()
