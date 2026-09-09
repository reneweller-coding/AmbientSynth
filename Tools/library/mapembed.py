"""The preset map's layout: a free cloud, not a grid.

The old layout took half of each axis from a rank and then pushed every preset into its own cell
of a fine grid. Both of those spread the library evenly over the plane on purpose -- nothing hid
under anything else -- and both destroyed exactly what a map is for: you could not see that forty
presets are nearly the same sound, because they were forty evenly spaced dots like any others.

This one keeps the neighbourhoods instead. A k-nearest-neighbour graph is built in the descriptor
space and laid out with springs: neighbours pull, everything else pushes, which lets similar
presets ball up and leaves gaps where the library is thin. Then the cloud is turned so that the
two axes still read the way a browser should -- dark to bright across, still to evolving up --
because a cloud you cannot orient yourself in is a worse browser than a grid.

No sklearn or umap in the environment; scipy's KD-tree and numpy do it in a few seconds for the
whole library.
"""
import numpy as np
from scipy.spatial import cKDTree


def _knn(X, k):
    tree = cKDTree(X)
    d, i = tree.query(X, k=k + 1)      # the first hit is the point itself
    return d[:, 1:], i[:, 1:]


def embed(desc, axes=(0, 1), k=12, iters=400, seed=7, repel=0.9, graph=None):
    """desc: (n, m) descriptors, each already 0..1. axes: which two columns the finished cloud
    should be oriented by (across, up). Returns (n, 2) positions in 0..1.

    graph: the space neighbourhoods are decided in, when that is not the descriptors themselves --
    the descriptors say what a preset is like, a learned embedding says what it is, and the map is
    more useful if both have a say. It is used exactly as given (the caller has already weighted
    its blocks), while desc still decides which way round the finished cloud is turned."""
    rng = np.random.default_rng(seed)
    n = len(desc)
    X = np.asarray(graph, dtype=np.float64) if graph is not None \
        else (desc - desc.mean(axis=0)) / (desc.std(axis=0) + 1e-9)
    k = min(k, max(2, n - 1))
    nd, ni = _knn(X, k)
    # Neighbour weights: near neighbours pull harder, and the scale is each point's own distance
    # to its k-th neighbour, so a dense region is not dominated by a sparse one.
    sigma = np.maximum(nd[:, -1:], 1e-6)
    w = np.exp(-(nd / sigma) ** 2)
    w /= w.sum(axis=1, keepdims=True)

    # Start from the principal plane: the springs then only have to rearrange, not discover.
    U, S, _ = np.linalg.svd(X - X.mean(axis=0), full_matrices=False)
    Y = U[:, :2] * S[:2]
    Y = Y / (Y.std(axis=0) + 1e-9)
    Y += 0.01 * rng.standard_normal(Y.shape)

    grid = 48
    for it in range(iters):
        step = 0.25 * (1.0 - it / iters) + 0.02
        # Attraction: towards the weighted mean of the neighbours.
        target = (Y[ni] * w[:, :, None]).sum(axis=1)
        F = target - Y
        # Repulsion, approximated on a grid: every cell's centroid pushes, which is a Barnes-Hut
        # without the tree and is plenty at this size. Without it the cloud collapses to a blob.
        lo, hi = Y.min(axis=0), Y.max(axis=0)
        span = np.maximum(hi - lo, 1e-6)
        cell = np.clip(((Y - lo) / span * (grid - 1)).astype(np.int32), 0, grid - 1)
        flat = cell[:, 0] * grid + cell[:, 1]
        cnt = np.bincount(flat, minlength=grid * grid).astype(np.float64)
        cx = np.bincount(flat, weights=Y[:, 0], minlength=grid * grid)
        cy = np.bincount(flat, weights=Y[:, 1], minlength=grid * grid)
        occ = cnt > 0
        cpos = np.stack([cx[occ] / cnt[occ], cy[occ] / cnt[occ]], axis=1)
        cw = cnt[occ]
        d = Y[:, None, :] - cpos[None, :, :]
        r2 = (d ** 2).sum(axis=2) + 0.05
        F += repel * (d * (cw[None, :] / r2)[:, :, None]).sum(axis=1) / max(n, 1) * 4.0
        Y += step * F / (np.linalg.norm(F, axis=1, keepdims=True) + 1e-9)

    # Orient the cloud: the rotation (and flip) whose axes agree best with the two descriptors the
    # browser promises. A cloud is only navigable if left still means dark and up still means moving.
    a = desc[:, axes[0]] - desc[:, axes[0]].mean()
    b = desc[:, axes[1]] - desc[:, axes[1]].mean()
    best, bestScore = None, -1e9
    for ang in np.linspace(0, np.pi, 180, endpoint=False):
        c, s = np.cos(ang), np.sin(ang)
        R = np.array([[c, -s], [s, c]])
        for fx in (1, -1):
            for fy in (1, -1):
                P = (Y @ R.T) * np.array([fx, fy])
                sc = (np.corrcoef(P[:, 0], a)[0, 1] + np.corrcoef(P[:, 1], b)[0, 1])
                if sc > bestScore:
                    bestScore, best = sc, P
    Y = best
    # Into 0..1 with a margin, on the 1st..99th percentile so a single outlier cannot squeeze the
    # whole cloud into the middle of the plane.
    lo = np.percentile(Y, 0.5, axis=0)
    hi = np.percentile(Y, 99.5, axis=0)
    Y = (Y - lo) / np.maximum(hi - lo, 1e-9)
    Y = 0.03 + 0.94 * np.clip(Y, -0.02, 1.02)
    # Points that land on exactly the same spot get a hair of jitter -- enough that a click can
    # reach either, far too little to read as a grid.
    order = np.lexsort((Y[:, 1], Y[:, 0]))
    for i in range(1, len(order)):
        p, q = order[i - 1], order[i]
        if np.hypot(*(Y[q] - Y[p])) < 1e-4:
            Y[q] += rng.standard_normal(2) * 5e-4
    return np.clip(Y, 0.0, 1.0)


def kmeans(X, k, seed=3, iters=60):
    """Plain k-means with k-means++ seeding: no sklearn here."""
    rng = np.random.default_rng(seed)
    n = len(X)
    c = [X[rng.integers(n)]]
    for _ in range(k - 1):
        d = np.min(((X[:, None, :] - np.array(c)[None, :, :]) ** 2).sum(axis=2), axis=1)
        p = d / max(d.sum(), 1e-12)
        c.append(X[rng.choice(n, p=p)])
    C = np.array(c)
    lab = np.zeros(n, dtype=np.int32)
    for _ in range(iters):
        d = ((X[:, None, :] - C[None, :, :]) ** 2).sum(axis=2)
        new = d.argmin(axis=1).astype(np.int32)
        if (new == lab).all():
            break
        lab = new
        for j in range(k):
            m = lab == j
            if m.any():
                C[j] = X[m].mean(axis=0)
    return lab, C


# The descriptor columns, in the order the tables use them.
COLS = ["bright", "motion", "width", "noisy", "bass", "density", "evolve", "rough", "wet"]
# What a high and a low value of each is called, for naming a cluster after what it actually is.
HIGH = {"bright": "Bright", "motion": "Moving", "width": "Wide", "noisy": "Noisy", "bass": "Deep",
        "density": "Dense", "evolve": "Evolving", "rough": "Rough", "wet": "Far"}
LOW = {"bright": "Dark", "motion": "Calm", "width": "Narrow", "noisy": "Tonal", "bass": "Light",
       "density": "Sparse", "evolve": "Still", "rough": "Smooth", "wet": "Near"}


def cluster(desc, k=12, seed=3, space=None):
    """Groups of presets that measure alike, and a name for each from what makes it itself: the
    two descriptors furthest from the library's middle, said in words.

    space: where the grouping happens, if that is not the descriptors themselves. The name always
    comes from the group's descriptors, because those are the words we can say honestly."""
    lab, _ = kmeans(space if space is not None else desc, k, seed=seed)
    C = np.array([desc[lab == j].mean(axis=0) if (lab == j).any() else np.full(desc.shape[1], 0.5)
                  for j in range(k)])
    names = []
    for j in range(k):
        dev = C[j] - 0.5
        order = np.argsort(-np.abs(dev))
        parts = []
        for c in order[:2]:
            name = (HIGH if dev[c] > 0 else LOW)[COLS[c]]
            if name not in parts:
                parts.append(name)
        names.append(" ".join(parts) if parts else f"Group {j + 1}")
    # Two clusters can end up with the same two words. "Sparse Still" and "Sparse Still 2" stood
    # side by side in the browser's legend, and the number said nothing about either: the one that
    # collides is given the next descriptor that is furthest from the middle, which is what
    # actually tells them apart. Numbering is kept only for the case where even that repeats.
    taken = set()
    out = []
    for j, nm in enumerate(names):
        if nm in taken:
            dev = C[j] - 0.5
            order = np.argsort(-np.abs(dev))
            for c in order[2:]:
                extra = (HIGH if dev[c] > 0 else LOW)[COLS[c]]
                cand = f"{nm} {extra}"
                if extra not in nm.split() and cand not in taken:
                    nm = cand
                    break
        if nm in taken:
            n = 2
            while f"{nm} {n}" in taken:
                n += 1
            nm = f"{nm} {n}"
        taken.add(nm)
        out.append(nm)
    return lab, out


def trustworthiness(desc, xy, k=10, standardize=True):
    """How much of the neighbourhood survived the projection: the share of each preset's k nearest
    in the descriptor space that are still among its 2k nearest on the map. The old grid layout
    and this one are compared with it, so 'the clusters are real' is a number and not a claim."""
    X = (desc - desc.mean(0)) / (desc.std(0) + 1e-9) if standardize else np.asarray(desc, dtype=np.float64)
    _, ni = _knn(X, k)
    _, mi = _knn(xy, 2 * k)
    keep = 0
    for i in range(len(desc)):
        keep += len(set(ni[i].tolist()) & set(mi[i].tolist()))
    return keep / float(len(desc) * k)
