from pathlib import Path

from rigdash.vectors import render

VECTORS = Path(__file__).resolve().parents[2] / "protocol" / "vectors.txt"


def test_committed_vectors_match_encoder():
    """If this fails, regenerate: python -m rigdash.vectors > protocol/vectors.txt"""
    assert VECTORS.read_text() == render()
