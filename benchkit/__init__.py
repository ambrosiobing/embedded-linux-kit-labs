"""Parts every stream-shaped lab in this volume needs.

Sequence accounting, sources, the lab book and the spectrum arithmetic are
not properties of P04. P02 and P03 drive a modem over the same kind of
line-oriented link, P06 reads a ranging grid over one, P18 counts gaps in a
sequence, and P13 decides on a timeout. Each of those wants most of this,
so it lives here rather than in the first lab that happened to need it.
"""

__version__ = "0.1.0"
